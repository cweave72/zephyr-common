# FsApi

FsApi gives handle-based access to littlefs file systems through the Zephyr
VFS (`fs_*`). The module has two parts:

1. **FsApi** (`CONFIG_FSAPI`). A C API for local code. It mounts the file
   systems and keeps pools of file and directory handles.
2. **FsApiRpc** (`CONFIG_FSAPIRPC`). A ProtoRpc callset. It gives a host
   remote access to the same file systems. The host client is the `fsapi`
   Python package (`python/fsapi`).

A handle is a small integer. A remote client can thus keep a file open across
RPC calls. All paths are absolute VFS paths that include the mount point, for
example `/flash/logs/a.txt`.

## Mounts

FsApi keeps a table of mounts. There are two ways to add a mount:

| Source            | How                                                                                |
|-------------------|------------------------------------------------------------------------------------|
| A flash partition | A `zephyr,fstab,littlefs` devicetree node. `FsApi_init` mounts every enabled node. |
| Any other storage | A `struct fs_mount_t` in code, given to `FsApi_addMount` after `FsApi_init`.       |

In Zephyr 4.0 a littlefs fstab node can only describe a flash partition. Use
`FsApi_addMount` for littlefs on a RAM disk or another disk.

All mounts share the handle pools. A handle belongs to the mount of its path.
`FsApi_format` closes only the handles of the mount that it formats.

## Capabilities

| Group    | Functions                                                                                                        |
|----------|------------------------------------------------------------------------------------------------------------------|
| Mounts   | `FsApi_init`, `FsApi_addMount`, `FsApi_getMountCount`, `FsApi_getMountPoint`, `FsApi_getInfo`, `FsApi_format`    |
| Files    | `FsApi_open`, `FsApi_close`, `FsApi_read`, `FsApi_write`, `FsApi_seek`, `FsApi_tell`, `FsApi_size`, `FsApi_sync` |
| Handles  | `FsApi_closeAll`                                                                                                 |
| Paths    | `FsApi_stat`, `FsApi_remove`, `FsApi_rename`, `FsApi_mkdir`                                                      |
| Dirs     | `FsApi_opendir`, `FsApi_readdir`, `FsApi_closedir`, `FsApi_listDir`                                              |
| One-shot | `FsApi_readFile`, `FsApi_writeFile`                                                                              |

Rules for all functions:

- A negative return value is a negative errno value.
- Open flags are the Zephyr `FS_O_*` flags. Seek origins are `FS_SEEK_*`.
- `FsApi_init` mounts all fstab nodes. A failed mount does not stop the
  others. The return value is the first error.
- `FsApi_open` and `FsApi_opendir` return `-EMFILE` when all handles are in
  use.
- A closed or unknown handle gives `-EBADF`.
- `FsApi_getInfo` accepts a mount point or any path on the file system.
- `FsApi_listDir` returns one page of entries and the total count. It keeps
  no handle open.
- `FsApi_format` takes a mount point. It closes the handles of that mount,
  unmounts, formats and mounts again. It also works when the mount failed.
  All data of that mount is lost.
- `FsApi_rename` cannot move a file to another mount.
- The size of a directory entry is always 0.

A mutex guards the handle pools and the mount table. Each handle operation
holds the mutex, thus two threads can use FsApi at the same time.

## Kconfig

| Symbol                 | Default | Description                                        |
|------------------------|---------|----------------------------------------------------|
| `FSAPI`                | n       | Enables the FsApi library.                         |
| `FSAPI_MAX_MOUNTS`     | 2       | Size of the mount table: fstab nodes + others.     |
| `FSAPI_MAX_OPEN_FILES` | 4       | Number of file handles, for all mounts.            |
| `FSAPI_MAX_OPEN_DIRS`  | 2       | Number of directory handles, for all mounts.       |
| `FSAPI_PB`             | n       | Enables `FsApi_unpack_file` (protobuf blob files). |
| `FSAPIRPC`             | n       | Enables the FsApiRpc callset.                      |

`FSAPI` depends on `FILE_SYSTEM`, `FILE_SYSTEM_LITTLEFS` and
`FILE_SYSTEM_MKFS`. `FSAPI_PB` depends on `FSAPI`, `NANOPB` and `PBGENERIC`.
`FSAPIRPC` depends on `FSAPI`, `NANOPB`, `PBGENERIC` and `PROTORPC`. Kconfig does not enable a dependency. Set each one in the
application conf file. An unmet dependency removes the symbol with only a
warning.

A littlefs mount on a disk also needs `DISK_ACCESS`, the disk driver (for
example `DISK_DRIVER_RAM`) and `FS_LITTLEFS_BLK_DEV`.

## Devicetree

A flash mount is a `zephyr,fstab,littlefs` node. The node gives the partition
and the mount point. Do not set `automount`. FsApi mounts the file system in
`FsApi_init`.

```dts
&flash0 {
	partitions {
		lfs_partition: partition@1e0000 {
			label = "lfs";
			reg = <0x1e0000 DT_SIZE_K(128)>;
		};
	};
};

/ {
	fstab {
		compatible = "zephyr,fstab";
		flash_lfs: flash_lfs {
			compatible = "zephyr,fstab,littlefs";
			mount-point = "/flash";
			partition = <&lfs_partition>;
			read-size = <16>;
			prog-size = <16>;
			cache-size = <256>;
			lookahead-size = <32>;
			block-cycles = <512>;
		};
	};
};
```

The partition must not overlap the code partition. It must not be the
settings (NVS) partition. `applications/fs_demo` shows a flash split with a
configurable size.

A RAM disk is a `zephyr,ram-disk` node. Its content is lost at reset. The
first mount after a reset formats it.

```dts
/ {
	ramdisk0: ramdisk0 {
		compatible = "zephyr,ram-disk";
		disk-name = "RAM";
		sector-size = <512>;
		sector-count = <64>;
	};
};
```

## Build checks

These errors stop the build. Without the checks, each error builds clean and
fails at run time.

| Check                                                            | Result without the check                                   |
|------------------------------------------------------------------|------------------------------------------------------------|
| Each fstab partition size is a multiple of the erase block size. | The partial last block is unusable.                        |
| `FS_LITTLEFS_CACHE_SIZE` >= `cache-size` of each fstab node      | An open fails with `-ENOMEM`.                              |
| `FSAPI_MAX_MOUNTS` >= the number of fstab nodes                  | A mount is lost.                                           |
| `FS_LITTLEFS_NUM_FILES` >= `FSAPI_MAX_OPEN_FILES`                | An open fails with `-ENOMEM`.                              |
| `FS_LITTLEFS_NUM_DIRS` > `FSAPI_MAX_OPEN_DIRS`                   | `FsApi_listDir` fails when all directory handles are open. |

`FsApi_listDir` uses one directory in addition to the directory handle pool.
Thus `FS_LITTLEFS_NUM_DIRS` must be larger than `FSAPI_MAX_OPEN_DIRS`.

FsApi cannot check a mount from `FsApi_addMount` at build time. littlefs on a
disk uses one sector as its cache. Thus `FS_LITTLEFS_CACHE_SIZE` must be at
least the sector size (512 for the RAM disk above), or an open on that mount
fails with `-ENOMEM`.

## Usage

Configuration, for a flash mount and a RAM disk mount:

```
CONFIG_FLASH=y
CONFIG_FLASH_MAP=y
CONFIG_FILE_SYSTEM=y
CONFIG_FILE_SYSTEM_LITTLEFS=y
CONFIG_FILE_SYSTEM_MKFS=y
CONFIG_DISK_ACCESS=y
CONFIG_DISK_DRIVER_RAM=y
CONFIG_FS_LITTLEFS_BLK_DEV=y
CONFIG_FS_LITTLEFS_CACHE_SIZE=512
CONFIG_FSAPI=y
```

Code:

```c
#include <zephyr/fs/littlefs.h>
#include "FsApi.h"

/* littlefs on the RAM disk "RAM". On a disk, littlefs uses one 512 B sector
   as the block, read, prog and cache size, and a lookahead of 4 blocks. */
FS_LITTLEFS_DECLARE_CUSTOM_CONFIG(ram_lfs, 4, 512, 512, 512, 2048);

static struct fs_mount_t ram_mnt = {
    .type = FS_LITTLEFS,
    .fs_data = &ram_lfs,
    .storage_dev = (void *)"RAM",
    .mnt_point = "/ram",
    .flags = FS_MOUNT_FLAG_USE_DISK_ACCESS,
};

int ret = FsApi_init();            /* mounts /flash */
ret = FsApi_addMount(&ram_mnt);    /* mounts /ram */

int fd = FsApi_open("/flash/log.txt", FS_O_CREATE | FS_O_WRITE | FS_O_APPEND);
if (fd >= 0)
{
    FsApi_write(fd, "boot\n", 5);
    FsApi_close(fd);
}

uint32_t count;
ssize_t n = FsApi_readFile("/flash/count", 0, &count, sizeof(count));

ret = FsApi_format("/ram");
```

## Branding

Branding writes files into a flash file system at build time, for example
configuration files such as `/flash/etc/config/net.pb`. It is a separate
step from a firmware flash.

| Step                     | Tool                              | Output                                             |
|--------------------------|-----------------------------------|----------------------------------------------------|
| 1. Build the application | `make build` (FsApi CMake)        | `build/fsapi_layout.json`                          |
| 2. Build the image       | `make brandimage` (`fsapi-brand`) | `build/brand/brand.bin`, `brand.hex`, `brand.json` |
| 3. Flash the image       | `make brand` (steps 2 and 3)      | The fs partition on the device                     |

**Layout.** The FsApi build runs `scripts/fsapi_layout.py`. The script reads
the devicetree of the build (`edt.pickle`) and writes one entry for each
enabled `zephyr,fstab,littlefs` node: the mount point, the partition offset
and size, the flash base address, the erase block size and the littlefs
sizes. The image thus always has the geometry of the firmware.

**Image.** `fsapi-brand` (`python/fsapi`) builds a littlefs image of the whole
partition from a directory. The directory maps to the mount root:
`brand/default/etc/bootmsg.txt` becomes `/flash/etc/bootmsg.txt`. Dot
files are skipped. A `<name>.pb.yaml` file becomes the protobuf blob
`<name>.pb`. See [Protobuf blobs](#protobuf-blobs). The tool writes littlefs on-disk format 2.1, the format of
the device littlefs (v2.9). After it writes the image, the tool mounts the
image again and compares each file.

**Flash.** `make brand` flashes only the fs partition. The image replaces the
whole partition, so the runtime files of that mount are lost. `make flash`
never touches the partition, so a firmware update keeps the branded files.

```bash
make brand                              # BRAND defaults to brand/default
make brand BRAND=~/secrets/site-a       # a directory outside git
make brand BRAND_MOUNT=/flash           # when the build has more flash mounts
```

`BRAND_FLASH_ARGS` holds the `west flash` arguments which write only the
image. The default, `--hex-file build/brand/brand.hex`, suits a runner which
flashes a hex file (openocd on the w55rp20). The hex holds only the partition
address range. An esp32 board (esptool) sets this in its `board.mk`:

```make
BRAND_FLASH_ARGS = --bin-file build/brand/brand.bin \
                   --esp-app-address $$(cat build/brand/partition_offset)
```

The esp32s3 boards have no flash fs partition yet. See the FsApi layouts in
`python/app_generator/README.md`.

## Protobuf blobs

A blob file holds one raw protobuf message, with no header. Branding writes
blob files from `.pb.yaml` files, and `fsapi-cli pbput` replaces them on a
running device (see "Protobuf blobs" in `python/fsapi/README.md`).
`FsApi_unpack_file` (`CONFIG_FSAPI_PB`, `FsApiPb.h`) reads a blob file into
the nanopb struct of the message:

```c
#include "FsApiPb.h"
#include "NetConf.pb.h"

static netconf_NetConf conf;

ret = FsApi_unpack_file("/flash/etc/config/net.pb", &conf,
    netconf_NetConf_fields);
if (ret != 0)
{
    /* A failed decode can leave part of the data. */
    conf = (netconf_NetConf)netconf_NetConf_init_zero;
}
```

| Return               | Meaning                                                       |
|----------------------|---------------------------------------------------------------|
| 0                    | The struct holds the message.                                 |
| `-ENOENT`            | The file does not exist. Use the default values.              |
| `-EBADMSG`           | The file is not a valid message. The function logs a warning. |
| Other negative errno | A file system error, for example `-EMFILE`.                   |

The function uses one file handle. A nanopb input stream reads the file in
small pieces through `FsApi_read`, so it needs no buffer for the file.
`Pb_unpack_stream` (PbGeneric) does the decode.

Put the `.proto` in the application `proto/` directory, or in the workspace
`proto/` directory if more applications use it. Add the directory to
`nanopb_build_sources` in the application `CMakeLists.txt`. `fsapi-brand`
finds the same `.proto` for the `.pb.yaml` file. See `applications/fs_demo`
(`proto/NetConf.proto`, `src/net_ip.c`).

## Remote access (FsApiRpc)

The callset is defined in `proto/FsApiRpc/FsApiRpc.proto` (package `fsapi`,
version 0.3.0). Register it in the application `rpc.c` file:

```c
#include "FsApiRpc.h"
#include "FsApiRpc.pb.h"

static ProtoRpc_Callset_Entry callsets[] = {
    PROTORPC_ADD_CALLSET(0, SystemRpc_resolver, system_Callset),
    PROTORPC_ADD_CALLSET(1, RtosUtilsRpc_resolver, rtosutils_Callset),
    PROTORPC_ADD_CALLSET(2, FsApiRpc_resolver, fsapi_Callset),
};
```

Mount all file systems before the RPC server starts.

| Call                  | Description                                                                       |
|-----------------------|-----------------------------------------------------------------------------------|
| ListMounts            | The mount points, up to 4.                                                        |
| GetFsInfo             | For the mount which holds a path: mount point, block size, total and free blocks. |
| Stat                  | Type and size of a path.                                                          |
| ListDir               | One page of up to 8 entries, from `start_idx`.                                    |
| Open, Close, CloseAll | File handles. Open flags are the `OpenFlags` enum bits.                           |
| Read, Write           | Up to 1024 bytes for each call, with an optional offset.                          |
| Seek, Size            | File position and file size.                                                      |
| Remove, Rename, Mkdir | Path operations.                                                                  |
| Format                | Erases one mount (given by its mount point) and mounts it again.                  |

Each reply has a `result` field. A negative `result` is a negative errno value
of the device. The device errno values are the Zephyr libc values. Some values
are not the same as the Linux values, for example `ENOTEMPTY` is 90 on the
device and 39 on Linux. The RPC status is `RPC_HANDLER_ERROR` only for a
malformed request.

The 1024-byte chunk size keeps a frame below the 2048-byte
`PROTORPC_MAX_MSG_SIZE`. The largest callset message sets the size of the RPC
buffers for all callsets.
