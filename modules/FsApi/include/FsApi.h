/*******************************************************************************
 *  @file: FsApi.h
 *
 *  @brief: Handle-based file system API over the Zephyr VFS and littlefs.
 *
 *  FsApi manages a table of mounts. FsApi_init mounts every enabled
 *  zephyr,fstab,littlefs node. FsApi_addMount adds any other mount, for
 *  example littlefs on a RAM disk. Files and directories are referenced by
 *  integer handles, so a remote client (see FsApiRpc) can keep them open
 *  across calls.
 *
 *  All paths are absolute VFS paths which include the mount point, for
 *  example "/flash/logs/a.txt". All functions return a negative errno on
 *  failure.
*******************************************************************************/
#ifndef FSAPI_H
#define FSAPI_H

#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <zephyr/fs/fs.h>

/** @brief Maximum number of files open at one time. */
#define FSAPI_MAX_OPEN_FILES    CONFIG_FSAPI_MAX_OPEN_FILES

/** @brief Maximum number of directories open at one time. */
#define FSAPI_MAX_OPEN_DIRS     CONFIG_FSAPI_MAX_OPEN_DIRS

/******************************************************************************
    [docexport FsApi_init]
*//**
    @brief Initializes the handle pools. Registers and mounts every enabled
    zephyr,fstab,littlefs node. A mount failure formats the partition, unless
    the node sets no-format. Call FsApi_addMount afterwards for other mounts.
    @return 0 on success, or the first mount error. A failed mount does not
      stop the other mounts.
******************************************************************************/
int
FsApi_init(void);

/******************************************************************************
    [docexport FsApi_addMount]
*//**
    @brief Registers a mount and mounts it. Use it for a file system which no
    fstab node describes, for example littlefs on a RAM disk. Call it after
    FsApi_init. The mount descriptor must stay valid while FsApi runs.
    @param[in] mp  The Zephyr mount descriptor.
    @return 0 on success, negative errno on failure. -ENOMEM if the mount
      table is full, -EEXIST if the mount point is already registered.
******************************************************************************/
int
FsApi_addMount(struct fs_mount_t *mp);

/******************************************************************************
    [docexport FsApi_getMountCount]
*//**
    @brief Returns the number of registered mounts.
******************************************************************************/
int
FsApi_getMountCount(void);

/******************************************************************************
    [docexport FsApi_format]
*//**
    @brief Formats one file system and mounts it again. All its data is lost.
    Closes the open handles on that file system first. Works also when the
    mount failed, for example on a corrupted file system.
    @param[in] mnt_point  The mount point, for example "/flash".
    @return 0 on success, negative errno on failure. -ENOENT if no mount has
      this mount point.
******************************************************************************/
int
FsApi_format(const char *mnt_point);

/******************************************************************************
    [docexport FsApi_getMountPoint]
*//**
    @brief Returns the mount point of a registered mount, for example "/flash".
    @param[in] idx  The mount index, 0 to FsApi_getMountCount() - 1.
    @return The mount point, or NULL if idx is out of range.
******************************************************************************/
const char *
FsApi_getMountPoint(int idx);

/******************************************************************************
    [docexport FsApi_getInfo]
*//**
    @brief Gets the volume statistics of the file system which holds a path.
    @param[in] path  A mount point or any path on the file system.
    @param[out] stat  Pointer to the statistics output.
    @return 0 on success, negative errno on failure.
******************************************************************************/
int
FsApi_getInfo(const char *path, struct fs_statvfs *stat);

/******************************************************************************
    [docexport FsApi_open]
*//**
    @brief Opens a file.
    @param[in] path  Absolute path of the file.
    @param[in] flags  Zephyr FS_O_* flags.
    @return The file handle (>= 0) on success, negative errno on failure.
      -EMFILE if all handles are in use.
******************************************************************************/
int
FsApi_open(const char *path, fs_mode_t flags);

/******************************************************************************
    [docexport FsApi_close]
*//**
    @brief Closes a file and releases its handle.
    @param[in] fd  The file handle.
    @return 0 on success, negative errno on failure.
******************************************************************************/
int
FsApi_close(int fd);

/******************************************************************************
    [docexport FsApi_closeAll]
*//**
    @brief Closes all open files and directories.
    @return The number of handles closed.
******************************************************************************/
int
FsApi_closeAll(void);

/******************************************************************************
    [docexport FsApi_read]
*//**
    @brief Reads from the current position of a file.
    @param[in] fd  The file handle.
    @param[out] buf  Destination buffer.
    @param[in] size  Maximum number of bytes to read.
    @return The number of bytes read, negative errno on failure.
******************************************************************************/
ssize_t
FsApi_read(int fd, void *buf, size_t size);

/******************************************************************************
    [docexport FsApi_write]
*//**
    @brief Writes at the current position of a file.
    @param[in] fd  The file handle.
    @param[in] buf  Source buffer.
    @param[in] size  Number of bytes to write.
    @return The number of bytes written, negative errno on failure.
******************************************************************************/
ssize_t
FsApi_write(int fd, const void *buf, size_t size);

/******************************************************************************
    [docexport FsApi_seek]
*//**
    @brief Moves the position of a file.
    @param[in] fd  The file handle.
    @param[in] offset  The offset relative to whence.
    @param[in] whence  FS_SEEK_SET, FS_SEEK_CUR or FS_SEEK_END.
    @return 0 on success, negative errno on failure.
******************************************************************************/
int
FsApi_seek(int fd, off_t offset, int whence);

/******************************************************************************
    [docexport FsApi_tell]
*//**
    @brief Gets the position of a file.
    @param[in] fd  The file handle.
    @return The position, negative errno on failure.
******************************************************************************/
off_t
FsApi_tell(int fd);

/******************************************************************************
    [docexport FsApi_size]
*//**
    @brief Gets the size of an open file. The file position does not change.
    @param[in] fd  The file handle.
    @return The size in bytes, negative errno on failure.
******************************************************************************/
off_t
FsApi_size(int fd);

/******************************************************************************
    [docexport FsApi_sync]
*//**
    @brief Flushes the cached data of an open file to storage.
    @param[in] fd  The file handle.
    @return 0 on success, negative errno on failure.
******************************************************************************/
int
FsApi_sync(int fd);

/******************************************************************************
    [docexport FsApi_stat]
*//**
    @brief Gets the type and size of a file or directory.
    @param[in] path  Absolute path.
    @param[out] entry  Pointer to the entry output.
    @return 0 on success, -ENOENT if the path does not exist.
******************************************************************************/
int
FsApi_stat(const char *path, struct fs_dirent *entry);

/******************************************************************************
    [docexport FsApi_remove]
*//**
    @brief Removes a file or an empty directory.
    @param[in] path  Absolute path.
    @return 0 on success, negative errno on failure.
******************************************************************************/
int
FsApi_remove(const char *path);

/******************************************************************************
    [docexport FsApi_rename]
*//**
    @brief Renames or moves a file or directory.
    @param[in] from  Absolute source path.
    @param[in] to  Absolute destination path.
    @return 0 on success, negative errno on failure.
******************************************************************************/
int
FsApi_rename(const char *from, const char *to);

/******************************************************************************
    [docexport FsApi_mkdir]
*//**
    @brief Creates a directory.
    @param[in] path  Absolute path.
    @return 0 on success, negative errno on failure.
******************************************************************************/
int
FsApi_mkdir(const char *path);

/******************************************************************************
    [docexport FsApi_opendir]
*//**
    @brief Opens a directory for iteration.
    @param[in] path  Absolute path.
    @return The directory handle (>= 0) on success, negative errno on failure.
      -EMFILE if all handles are in use.
******************************************************************************/
int
FsApi_opendir(const char *path);

/******************************************************************************
    [docexport FsApi_readdir]
*//**
    @brief Reads the next entry of a directory.
    @param[in] dd  The directory handle.
    @param[out] entry  Pointer to the entry output.
    @return 1 if an entry was read, 0 at the end of the directory, negative
      errno on failure.
******************************************************************************/
int
FsApi_readdir(int dd, struct fs_dirent *entry);

/******************************************************************************
    [docexport FsApi_closedir]
*//**
    @brief Closes a directory and releases its handle.
    @param[in] dd  The directory handle.
    @return 0 on success, negative errno on failure.
******************************************************************************/
int
FsApi_closedir(int dd);

/******************************************************************************
    [docexport FsApi_listDir]
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
    uint32_t max, uint32_t *total);

/******************************************************************************
    [docexport FsApi_readFile]
*//**
    @brief Opens a file, reads from an offset and closes the file.
    @param[in] path  Absolute path of the file.
    @param[in] offset  Offset from the start of the file.
    @param[out] buf  Destination buffer.
    @param[in] size  Maximum number of bytes to read.
    @return The number of bytes read, negative errno on failure.
******************************************************************************/
ssize_t
FsApi_readFile(const char *path, off_t offset, void *buf, size_t size);

/******************************************************************************
    [docexport FsApi_writeFile]
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
    fs_mode_t flags);

#endif /* FSAPI_H */
