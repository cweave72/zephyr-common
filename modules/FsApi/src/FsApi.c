/*******************************************************************************
 *  @file: FsApi.c
 *
 *  @brief: Handle-based file system API over the Zephyr VFS and littlefs.
 *
 *  A mutex guards the handle pools. Each handle operation holds the mutex, so
 *  a concurrent close cannot release a handle while another thread uses it.
*******************************************************************************/
#include <string.h>
#include <zephyr/kernel.h>
#include <zephyr/devicetree.h>
#include <zephyr/fs/fs.h>
#include "CheckCond.h"
#include "FsApi.h"

#include <zephyr/logging/log.h>
LOG_MODULE_REGISTER(FsApi, CONFIG_FSAPI_LOG_LEVEL);

/** @brief The fstab node which describes the file system. */
#define FSAPI_NODE      DT_NODELABEL(fsapi_lfs)

BUILD_ASSERT(DT_NODE_EXISTS(FSAPI_NODE),
    "FsApi needs a zephyr,fstab,littlefs node with the nodelabel fsapi_lfs.");

/** @brief The partition which the fstab node references. */
#define FSAPI_PART_NODE     DT_PHANDLE(FSAPI_NODE, partition)

BUILD_ASSERT((DT_REG_SIZE(FSAPI_PART_NODE) %
    DT_PROP_OR(DT_GPARENT(FSAPI_PART_NODE), erase_block_size, 4096)) == 0,
    "The fsapi_lfs partition size must be a multiple of the erase block size.");

BUILD_ASSERT(CONFIG_FSAPI_MAX_OPEN_FILES <= CONFIG_FS_LITTLEFS_NUM_FILES,
    "CONFIG_FS_LITTLEFS_NUM_FILES must be >= CONFIG_FSAPI_MAX_OPEN_FILES.");

BUILD_ASSERT(CONFIG_FSAPI_MAX_OPEN_DIRS < CONFIG_FS_LITTLEFS_NUM_DIRS,
    "CONFIG_FS_LITTLEFS_NUM_DIRS must be > CONFIG_FSAPI_MAX_OPEN_DIRS. "
    "FsApi_listDir uses one more directory.");

/* littlefs allocates each open file cache (cache-size bytes) from a heap. With
   CONFIG_FS_LITTLEFS_FC_HEAP_SIZE <= 0, Zephyr sizes that heap for
   CONFIG_FS_LITTLEFS_NUM_FILES caches of CONFIG_FS_LITTLEFS_CACHE_SIZE bytes. A
   larger cache-size in the fstab node makes a later open fail with -ENOMEM. */
BUILD_ASSERT((CONFIG_FS_LITTLEFS_FC_HEAP_SIZE > 0) ||
    (DT_PROP(FSAPI_NODE, cache_size) <= CONFIG_FS_LITTLEFS_CACHE_SIZE),
    "CONFIG_FS_LITTLEFS_CACHE_SIZE must be >= the fsapi_lfs cache-size, or set "
    "CONFIG_FS_LITTLEFS_FC_HEAP_SIZE.");

FS_FSTAB_DECLARE_ENTRY(FSAPI_NODE);

static struct fs_mount_t *mountpoint = &FS_FSTAB_ENTRY(FSAPI_NODE);

/** @brief The file handle pool. */
static struct fs_file_t files[FSAPI_MAX_OPEN_FILES];
static bool files_inuse[FSAPI_MAX_OPEN_FILES];

/** @brief The directory handle pool. */
static struct fs_dir_t dirs[FSAPI_MAX_OPEN_DIRS];
static bool dirs_inuse[FSAPI_MAX_OPEN_DIRS];

/** @brief True while the file system is mounted. The pool mutex guards it. */
static bool mounted;

static K_MUTEX_DEFINE(pool_mtx);

#define lock()      k_mutex_lock(&pool_mtx, K_FOREVER)
#define unlock()    k_mutex_unlock(&pool_mtx)

/** @brief Macro which validates a file handle. Call with the lock held. */
#define file_valid(fd)  (((fd) >= 0) && ((fd) < FSAPI_MAX_OPEN_FILES) && \
                          files_inuse[(fd)])

/** @brief Macro which validates a directory handle. Call with the lock held. */
#define dir_valid(dd)   (((dd) >= 0) && ((dd) < FSAPI_MAX_OPEN_DIRS) && \
                          dirs_inuse[(dd)])

/******************************************************************************
    clear_dir_size
*//**
    @brief Sets the size of a directory entry to 0. littlefs leaves the size of
    a directory undefined.
******************************************************************************/
static void
clear_dir_size(struct fs_dirent *entry)
{
    if (entry->type == FS_DIR_ENTRY_DIR)
    {
        entry->size = 0;
    }
}

/******************************************************************************
    [docimport FsApi_getMountPoint]
*//**
    @brief Returns the mount point of the file system, for example "/lfs".
******************************************************************************/
const char *
FsApi_getMountPoint(void)
{
    return mountpoint->mnt_point;
}

/******************************************************************************
    [docimport FsApi_getInfo]
*//**
    @brief Gets the volume statistics of the file system.
    @param[out] stat  Pointer to the statistics output.
    @return 0 on success, negative errno on failure.
******************************************************************************/
int
FsApi_getInfo(struct fs_statvfs *stat)
{
    CHECK_COND_RETURN(stat == NULL, -EINVAL);

    return fs_statvfs(mountpoint->mnt_point, stat);
}

/******************************************************************************
    [docimport FsApi_open]
*//**
    @brief Opens a file.
    @param[in] path  Absolute path of the file.
    @param[in] flags  Zephyr FS_O_* flags.
    @return The file handle (>= 0) on success, negative errno on failure.
      -EMFILE if all handles are in use.
******************************************************************************/
int
FsApi_open(const char *path, fs_mode_t flags)
{
    int fd;
    int ret;

    CHECK_COND_RETURN(path == NULL, -EINVAL);

    lock();
    for (fd = 0; fd < FSAPI_MAX_OPEN_FILES; fd++)
    {
        if (!files_inuse[fd])
        {
            break;
        }
    }

    if (fd == FSAPI_MAX_OPEN_FILES)
    {
        unlock();
        LOG_WRN("No free file handle for %s.", path);
        return -EMFILE;
    }

    fs_file_t_init(&files[fd]);
    ret = fs_open(&files[fd], path, flags);
    if (ret < 0)
    {
        unlock();
        LOG_DBG("fs_open(%s) returned %d.", path, ret);
        return ret;
    }

    files_inuse[fd] = true;
    unlock();

    LOG_DBG("Opened %s as fd %d.", path, fd);
    return fd;
}

/******************************************************************************
    [docimport FsApi_close]
*//**
    @brief Closes a file and releases its handle.
    @param[in] fd  The file handle.
    @return 0 on success, negative errno on failure.
******************************************************************************/
int
FsApi_close(int fd)
{
    int ret;

    lock();
    if (!file_valid(fd))
    {
        unlock();
        return -EBADF;
    }

    ret = fs_close(&files[fd]);
    /* Release the handle even if the close fails. Zephyr resets the file
       object in both cases. */
    files_inuse[fd] = false;
    unlock();

    return ret;
}

/******************************************************************************
    [docimport FsApi_closeAll]
*//**
    @brief Closes all open files and directories.
    @return The number of handles closed.
******************************************************************************/
int
FsApi_closeAll(void)
{
    int num = 0;
    int k;

    lock();
    for (k = 0; k < FSAPI_MAX_OPEN_FILES; k++)
    {
        if (files_inuse[k])
        {
            (void)fs_close(&files[k]);
            files_inuse[k] = false;
            num++;
        }
    }
    for (k = 0; k < FSAPI_MAX_OPEN_DIRS; k++)
    {
        if (dirs_inuse[k])
        {
            (void)fs_closedir(&dirs[k]);
            dirs_inuse[k] = false;
            num++;
        }
    }
    unlock();

    LOG_DBG("Closed %d handles.", num);
    return num;
}

/******************************************************************************
    [docimport FsApi_read]
*//**
    @brief Reads from the current position of a file.
    @param[in] fd  The file handle.
    @param[out] buf  Destination buffer.
    @param[in] size  Maximum number of bytes to read.
    @return The number of bytes read, negative errno on failure.
******************************************************************************/
ssize_t
FsApi_read(int fd, void *buf, size_t size)
{
    ssize_t ret;

    CHECK_COND_RETURN(buf == NULL, -EINVAL);

    lock();
    ret = file_valid(fd) ? fs_read(&files[fd], buf, size) : -EBADF;
    unlock();

    return ret;
}

/******************************************************************************
    [docimport FsApi_write]
*//**
    @brief Writes at the current position of a file.
    @param[in] fd  The file handle.
    @param[in] buf  Source buffer.
    @param[in] size  Number of bytes to write.
    @return The number of bytes written, negative errno on failure.
******************************************************************************/
ssize_t
FsApi_write(int fd, const void *buf, size_t size)
{
    ssize_t ret;

    CHECK_COND_RETURN(buf == NULL, -EINVAL);

    lock();
    ret = file_valid(fd) ? fs_write(&files[fd], buf, size) : -EBADF;
    unlock();

    return ret;
}

/******************************************************************************
    [docimport FsApi_seek]
*//**
    @brief Moves the position of a file.
    @param[in] fd  The file handle.
    @param[in] offset  The offset relative to whence.
    @param[in] whence  FS_SEEK_SET, FS_SEEK_CUR or FS_SEEK_END.
    @return 0 on success, negative errno on failure.
******************************************************************************/
int
FsApi_seek(int fd, off_t offset, int whence)
{
    int ret;

    lock();
    ret = file_valid(fd) ? fs_seek(&files[fd], offset, whence) : -EBADF;
    unlock();

    return ret;
}

/******************************************************************************
    [docimport FsApi_tell]
*//**
    @brief Gets the position of a file.
    @param[in] fd  The file handle.
    @return The position, negative errno on failure.
******************************************************************************/
off_t
FsApi_tell(int fd)
{
    off_t ret;

    lock();
    ret = file_valid(fd) ? fs_tell(&files[fd]) : -EBADF;
    unlock();

    return ret;
}

/******************************************************************************
    [docimport FsApi_size]
*//**
    @brief Gets the size of an open file. The file position does not change.
    @param[in] fd  The file handle.
    @return The size in bytes, negative errno on failure.
******************************************************************************/
off_t
FsApi_size(int fd)
{
    off_t pos;
    off_t size;
    int ret;

    lock();
    if (!file_valid(fd))
    {
        unlock();
        return -EBADF;
    }

    pos = fs_tell(&files[fd]);
    if (pos < 0)
    {
        unlock();
        return pos;
    }

    ret = fs_seek(&files[fd], 0, FS_SEEK_END);
    if (ret < 0)
    {
        unlock();
        return ret;
    }

    size = fs_tell(&files[fd]);
    ret = fs_seek(&files[fd], pos, FS_SEEK_SET);
    unlock();

    return (ret < 0) ? ret : size;
}

/******************************************************************************
    [docimport FsApi_sync]
*//**
    @brief Flushes the cached data of an open file to storage.
    @param[in] fd  The file handle.
    @return 0 on success, negative errno on failure.
******************************************************************************/
int
FsApi_sync(int fd)
{
    int ret;

    lock();
    ret = file_valid(fd) ? fs_sync(&files[fd]) : -EBADF;
    unlock();

    return ret;
}

/******************************************************************************
    [docimport FsApi_stat]
*//**
    @brief Gets the type and size of a file or directory.
    @param[in] path  Absolute path.
    @param[out] entry  Pointer to the entry output.
    @return 0 on success, -ENOENT if the path does not exist.
******************************************************************************/
int
FsApi_stat(const char *path, struct fs_dirent *entry)
{
    int ret;

    CHECK_COND_RETURN((path == NULL) || (entry == NULL), -EINVAL);

    ret = fs_stat(path, entry);
    CHECK_COND_RETURN(ret < 0, ret);

    clear_dir_size(entry);
    return 0;
}

/******************************************************************************
    [docimport FsApi_remove]
*//**
    @brief Removes a file or an empty directory.
    @param[in] path  Absolute path.
    @return 0 on success, negative errno on failure.
******************************************************************************/
int
FsApi_remove(const char *path)
{
    CHECK_COND_RETURN(path == NULL, -EINVAL);

    return fs_unlink(path);
}

/******************************************************************************
    [docimport FsApi_rename]
*//**
    @brief Renames or moves a file or directory.
    @param[in] from  Absolute source path.
    @param[in] to  Absolute destination path.
    @return 0 on success, negative errno on failure.
******************************************************************************/
int
FsApi_rename(const char *from, const char *to)
{
    CHECK_COND_RETURN((from == NULL) || (to == NULL), -EINVAL);

    return fs_rename(from, to);
}

/******************************************************************************
    [docimport FsApi_mkdir]
*//**
    @brief Creates a directory.
    @param[in] path  Absolute path.
    @return 0 on success, negative errno on failure.
******************************************************************************/
int
FsApi_mkdir(const char *path)
{
    CHECK_COND_RETURN(path == NULL, -EINVAL);

    return fs_mkdir(path);
}

/******************************************************************************
    [docimport FsApi_opendir]
*//**
    @brief Opens a directory for iteration.
    @param[in] path  Absolute path.
    @return The directory handle (>= 0) on success, negative errno on failure.
      -EMFILE if all handles are in use.
******************************************************************************/
int
FsApi_opendir(const char *path)
{
    int dd;
    int ret;

    CHECK_COND_RETURN(path == NULL, -EINVAL);

    lock();
    for (dd = 0; dd < FSAPI_MAX_OPEN_DIRS; dd++)
    {
        if (!dirs_inuse[dd])
        {
            break;
        }
    }

    if (dd == FSAPI_MAX_OPEN_DIRS)
    {
        unlock();
        LOG_WRN("No free directory handle for %s.", path);
        return -EMFILE;
    }

    fs_dir_t_init(&dirs[dd]);
    ret = fs_opendir(&dirs[dd], path);
    if (ret < 0)
    {
        unlock();
        LOG_DBG("fs_opendir(%s) returned %d.", path, ret);
        return ret;
    }

    dirs_inuse[dd] = true;
    unlock();

    return dd;
}

/******************************************************************************
    [docimport FsApi_readdir]
*//**
    @brief Reads the next entry of a directory.
    @param[in] dd  The directory handle.
    @param[out] entry  Pointer to the entry output.
    @return 1 if an entry was read, 0 at the end of the directory, negative
      errno on failure.
******************************************************************************/
int
FsApi_readdir(int dd, struct fs_dirent *entry)
{
    int ret;

    CHECK_COND_RETURN(entry == NULL, -EINVAL);

    lock();
    ret = dir_valid(dd) ? fs_readdir(&dirs[dd], entry) : -EBADF;
    unlock();

    CHECK_COND_RETURN(ret < 0, ret);

    /* Zephyr signals the end of the directory with an empty name. */
    CHECK_COND_RETURN(entry->name[0] == '\0', 0);

    clear_dir_size(entry);
    return 1;
}

/******************************************************************************
    [docimport FsApi_closedir]
*//**
    @brief Closes a directory and releases its handle.
    @param[in] dd  The directory handle.
    @return 0 on success, negative errno on failure.
******************************************************************************/
int
FsApi_closedir(int dd)
{
    int ret;

    lock();
    if (!dir_valid(dd))
    {
        unlock();
        return -EBADF;
    }

    ret = fs_closedir(&dirs[dd]);
    dirs_inuse[dd] = false;
    unlock();

    return ret;
}

/******************************************************************************
    [docimport FsApi_listDir]
*//**
    @brief Reads one page of directory entries without keeping a handle open.
    @param[in] path  Absolute path of the directory.
    @param[in] start_idx  Index of the first entry to return.
    @param[out] entries  Array for the entries.
    @param[in] max  Number of elements in entries.
    @param[out] total  Total number of entries in the directory. Can be NULL.
    @return The number of entries written to entries, negative errno on
      failure.
******************************************************************************/
int
FsApi_listDir(const char *path, uint32_t start_idx, struct fs_dirent *entries,
    uint32_t max, uint32_t *total)
{
    struct fs_dir_t dir;
    struct fs_dirent entry;
    uint32_t idx = 0;
    uint32_t num = 0;
    int ret;

    CHECK_COND_RETURN((path == NULL) || (entries == NULL), -EINVAL);

    /* This directory object is not in the handle pool, because it does not
       outlive the call. */
    fs_dir_t_init(&dir);
    ret = fs_opendir(&dir, path);
    CHECK_COND_RETURN(ret < 0, ret);

    while (1)
    {
        ret = fs_readdir(&dir, &entry);
        if ((ret < 0) || (entry.name[0] == '\0'))
        {
            break;
        }

        if ((idx >= start_idx) && (num < max))
        {
            clear_dir_size(&entry);
            entries[num++] = entry;
        }
        idx++;
    }

    (void)fs_closedir(&dir);
    CHECK_COND_RETURN(ret < 0, ret);

    if (total != NULL)
    {
        *total = idx;
    }

    return (int)num;
}

/******************************************************************************
    [docimport FsApi_readFile]
*//**
    @brief Opens a file, reads from an offset and closes the file.
    @param[in] path  Absolute path of the file.
    @param[in] offset  Offset from the start of the file.
    @param[out] buf  Destination buffer.
    @param[in] size  Maximum number of bytes to read.
    @return The number of bytes read, negative errno on failure.
******************************************************************************/
ssize_t
FsApi_readFile(const char *path, off_t offset, void *buf, size_t size)
{
    ssize_t ret;
    int fd;

    fd = FsApi_open(path, FS_O_READ);
    CHECK_COND_RETURN(fd < 0, fd);

    ret = FsApi_seek(fd, offset, FS_SEEK_SET);
    if (ret == 0)
    {
        ret = FsApi_read(fd, buf, size);
    }

    (void)FsApi_close(fd);
    return ret;
}

/******************************************************************************
    [docimport FsApi_writeFile]
*//**
    @brief Opens a file, writes at an offset and closes the file.
    @param[in] path  Absolute path of the file.
    @param[in] offset  Offset from the start of the file. Ignored if flags
      contains FS_O_APPEND.
    @param[in] buf  Source buffer.
    @param[in] size  Number of bytes to write.
    @param[in] flags  Additional FS_O_* flags, for example FS_O_CREATE or
      FS_O_TRUNC. FS_O_WRITE is always set.
    @return The number of bytes written, negative errno on failure.
******************************************************************************/
ssize_t
FsApi_writeFile(const char *path, off_t offset, const void *buf, size_t size,
    fs_mode_t flags)
{
    ssize_t ret = 0;
    int close_ret;
    int fd;

    fd = FsApi_open(path, flags | FS_O_WRITE);
    CHECK_COND_RETURN(fd < 0, fd);

    if ((flags & FS_O_APPEND) == 0)
    {
        ret = FsApi_seek(fd, offset, FS_SEEK_SET);
    }
    if (ret == 0)
    {
        ret = FsApi_write(fd, buf, size);
    }

    /* A close error means the data did not reach storage. */
    close_ret = FsApi_close(fd);
    return ((ret >= 0) && (close_ret < 0)) ? close_ret : ret;
}

/******************************************************************************
    [docimport FsApi_format]
*//**
    @brief Formats the file system and mounts it again. All data is lost.
    Closes all open handles first. Works also when the mount failed, for
    example on a corrupted file system.
    @return 0 on success, negative errno on failure.
******************************************************************************/
int
FsApi_format(void)
{
    int ret;

    /* Hold the pool lock for the whole format, so no handle opens on the
       old file system. k_mutex is recursive, thus FsApi_closeAll can lock. */
    lock();
    (void)FsApi_closeAll();

    if (mounted)
    {
        ret = fs_unmount(mountpoint);
        if (ret < 0)
        {
            unlock();
            LOG_ERR("fs_unmount failed: %d", ret);
            return ret;
        }
        mounted = false;
    }

    LOG_WRN("Formatting %s.", mountpoint->mnt_point);
    ret = fs_mkfs(mountpoint->type, (uintptr_t)mountpoint->storage_dev,
        mountpoint->fs_data, mountpoint->flags);
    if (ret == 0)
    {
        ret = fs_mount(mountpoint);
        mounted = (ret == 0);
    }
    unlock();

    CHECK_COND_RETURN_MSG(ret < 0, ret, "format failed");

    LOG_INF("Formatted and mounted %s.", mountpoint->mnt_point);
    return 0;
}

/******************************************************************************
    [docimport FsApi_init]
*//**
    @brief Initializes the handle pools and mounts the file system. A mount
    failure formats the partition, unless the fstab node sets no-format.
    @return 0 on success, negative errno on failure.
******************************************************************************/
int
FsApi_init(void)
{
    int ret;
    int k;

    lock();
    for (k = 0; k < FSAPI_MAX_OPEN_FILES; k++)
    {
        fs_file_t_init(&files[k]);
        files_inuse[k] = false;
    }
    for (k = 0; k < FSAPI_MAX_OPEN_DIRS; k++)
    {
        fs_dir_t_init(&dirs[k]);
        dirs_inuse[k] = false;
    }
    unlock();

    lock();
    ret = fs_mount(mountpoint);
    if (ret == -EBUSY)
    {
        /* The fstab node sets automount, so the fs is already mounted. */
        LOG_INF("%s is already mounted.", mountpoint->mnt_point);
        ret = 0;
    }
    mounted = (ret == 0);
    unlock();

    CHECK_COND_RETURN_MSG(ret < 0, ret, "fs_mount failed");

    LOG_INF("Mounted %s.", mountpoint->mnt_point);
    return 0;
}
