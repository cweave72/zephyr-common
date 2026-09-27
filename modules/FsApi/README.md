# FsApi

FsApi gives handle-based access to a littlefs file system through the Zephyr
VFS (`fs_*`). The module has two parts:

1. **FsApi** (`CONFIG_FSAPI`). A C API for local code. It mounts the file
   system and keeps pools of file and directory handles.
2. **FsApiRpc** (`CONFIG_FSAPIRPC`). A ProtoRpc callset. It gives a host
   remote access to the same file system. The host client is the `fsapi`
   Python package (`python/fsapi`).

A handle is a small integer. A remote client can thus keep a file open across
RPC calls. All paths are absolute VFS paths that include the mount point, for
example `/lfs/logs/a.txt`.

## Capabilities

| Group     | Functions                                                                                                        |
|-----------|------------------------------------------------------------------------------------------------------------------|
| Lifecycle | `FsApi_init`, `FsApi_format`, `FsApi_getMountPoint`, `FsApi_getInfo`                                             |
| Files     | `FsApi_open`, `FsApi_close`, `FsApi_read`, `FsApi_write`, `FsApi_seek`, `FsApi_tell`, `FsApi_size`, `FsApi_sync` |
| Handles   | `FsApi_closeAll`                                                                                                 |
| Paths     | `FsApi_stat`, `FsApi_remove`, `FsApi_rename`, `FsApi_mkdir`                                                      |
| Dirs      | `FsApi_opendir`, `FsApi_readdir`, `FsApi_closedir`, `FsApi_listDir`                                              |
| One-shot  | `FsApi_readFile`, `FsApi_writeFile`                                                                              |

Rules for all functions:

- A negative return value is a negative errno value.
- Open flags are the Zephyr `FS_O_*` flags. Seek origins are `FS_SEEK_*`.
- `FsApi_open` and `FsApi_opendir` return `-EMFILE` when all handles are in
  use.
- A closed or unknown handle gives `-EBADF`.
- `FsApi_listDir` returns one page of entries and the total count. It keeps
  no handle open.
- `FsApi_format` closes all handles, unmounts, formats and mounts again. It
  also works when the mount at boot failed. All data is lost.
- The size of a directory entry is always 0.

A mutex guards the handle pools. Each handle operation holds the mutex, thus
two threads can use FsApi at the same time.

## Kconfig

| Symbol                 | Default | Description                   |
|------------------------|---------|-------------------------------|
| `FSAPI`                | n       | Enables the FsApi library.    |
| `FSAPI_MAX_OPEN_FILES` | 4       | Number of file handles.       |
| `FSAPI_MAX_OPEN_DIRS`  | 2       | Number of directory handles.  |
| `FSAPIRPC`             | n       | Enables the FsApiRpc callset. |

`FSAPI` depends on `FILE_SYSTEM`, `FILE_SYSTEM_LITTLEFS` and
`FILE_SYSTEM_MKFS`. `FSAPIRPC` depends on `FSAPI`, `NANOPB`, `PBGENERIC` and
`PROTORPC`. Kconfig does not enable a dependency. Set each one in the
application conf file. An unmet dependency removes the symbol with only a
warning.

## Devicetree

FsApi mounts the `zephyr,fstab,littlefs` node with the nodelabel `fsapi_lfs`.
The node gives the partition and the mount point. Do not set `automount`.
FsApi mounts the file system in `FsApi_init`.

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
		fsapi_lfs: fsapi_lfs {
			compatible = "zephyr,fstab,littlefs";
			mount-point = "/lfs";
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

## Build checks

These errors stop the build. Without the checks, each error builds clean and
fails at run time.

| Check                                                     | Result without the check                                   |
|-----------------------------------------------------------|------------------------------------------------------------|
| The `fsapi_lfs` node exists.                              | No file system to mount.                                   |
| The partition size is a multiple of the erase block size. | The partial last block is unusable.                        |
| `FS_LITTLEFS_NUM_FILES` >= `FSAPI_MAX_OPEN_FILES`         | Open fails with `-ENOMEM`.                                 |
| `FS_LITTLEFS_NUM_DIRS` > `FSAPI_MAX_OPEN_DIRS`            | `FsApi_listDir` fails when all directory handles are open. |
| `FS_LITTLEFS_CACHE_SIZE` >= `cache-size` of the node      | The second open fails with `-ENOMEM`.                      |

`FsApi_listDir` uses one directory in addition to the directory handle pool.
Thus `FS_LITTLEFS_NUM_DIRS` must be larger than `FSAPI_MAX_OPEN_DIRS`.

## Usage

Configuration:

```
CONFIG_FLASH=y
CONFIG_FLASH_MAP=y
CONFIG_FILE_SYSTEM=y
CONFIG_FILE_SYSTEM_LITTLEFS=y
CONFIG_FILE_SYSTEM_MKFS=y
CONFIG_FS_LITTLEFS_CACHE_SIZE=256
CONFIG_FSAPI=y
```

Code:

```c
#include "FsApi.h"

int ret = FsApi_init();

int fd = FsApi_open("/lfs/log.txt", FS_O_CREATE | FS_O_WRITE | FS_O_APPEND);
if (fd >= 0)
{
    FsApi_write(fd, "boot\n", 5);
    FsApi_close(fd);
}

uint32_t count;
ssize_t n = FsApi_readFile("/lfs/count", 0, &count, sizeof(count));
```

## Remote access (FsApiRpc)

The callset is defined in `proto/FsApiRpc/FsApiRpc.proto` (package `fsapi`,
version 0.2.0). Register it in the application `rpc.c` file:

```c
#include "FsApiRpc.h"
#include "FsApiRpc.pb.h"

static ProtoRpc_Callset_Entry callsets[] = {
    PROTORPC_ADD_CALLSET(0, SystemRpc_resolver, system_Callset),
    PROTORPC_ADD_CALLSET(1, RtosUtilsRpc_resolver, rtosutils_Callset),
    PROTORPC_ADD_CALLSET(2, FsApiRpc_resolver, fsapi_Callset),
};
```

Call `FsApi_init` before the RPC server starts.

| Call                  | Description                                              |
|-----------------------|----------------------------------------------------------|
| GetFsInfo             | Mount point, block size, total blocks, free blocks.      |
| Stat                  | Type and size of a path.                                 |
| ListDir               | One page of up to 8 entries, from `start_idx`.           |
| Open, Close, CloseAll | File handles. Open flags are the `OpenFlags` enum bits.  |
| Read, Write           | Up to 1024 bytes for each call, with an optional offset. |
| Seek, Size            | File position and file size.                             |
| Remove, Rename, Mkdir | Path operations.                                         |
| Format                | Erases and mounts the file system again.                 |

Each reply has a `result` field. A negative `result` is a negative errno value
of the device. The device errno values are the Zephyr libc values. Some values
are not the same as the Linux values, for example `ENOTEMPTY` is 90 on the
device and 39 on Linux. The RPC status is `RPC_HANDLER_ERROR` only for a
malformed request.

The 1024-byte chunk size keeps a frame below the 2048-byte
`PROTORPC_MAX_MSG_SIZE`. The largest callset message sets the size of the RPC
buffers for all callsets.
