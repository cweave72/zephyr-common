/*******************************************************************************
 *  @file: FsApiPb.c
 *
 *  @brief: Reads protobuf blob files into nanopb structs. See FsApiPb.h.
 *
 *  A nanopb input stream reads the file through an FsApi handle. Thus the
 *  function needs no buffer for the whole file.
*******************************************************************************/
#include <stdint.h>
#include <zephyr/kernel.h>
#include "CheckCond.h"
#include "FsApi.h"
#include "FsApiPb.h"
#include "PbGeneric.h"

#include <zephyr/logging/log.h>
LOG_MODULE_DECLARE(FsApi, CONFIG_FSAPI_LOG_LEVEL);

/******************************************************************************
    read_cb
*//**
    @brief nanopb stream callback. Reads count bytes from the file.
    @param[in] stream  The stream. stream->state holds the file handle.
    @param[out] buf  Destination buffer. NULL skips count bytes.
    @param[in] count  Number of bytes to read.
    @return true if the function read count bytes; false otherwise.
******************************************************************************/
static bool
read_cb(pb_istream_t *stream, pb_byte_t *buf, size_t count)
{
    int fd = (int)(intptr_t)stream->state;
    ssize_t num;

    if (buf == NULL)
    {
        return FsApi_seek(fd, (off_t)count, FS_SEEK_CUR) == 0;
    }
    num = FsApi_read(fd, buf, count);
    return num == (ssize_t)count;
}

/******************************************************************************
    [docimport FsApi_unpack_file]
*//**
    @brief Reads a protobuf blob file and unpacks it into a nanopb struct. The
    function reads the file in small pieces, so the file size has no limit.
    On failure, the target struct can hold part of the data. Clear it before
    a fallback to default values.
    @param[in] path  Absolute path of the blob file.
    @param[out] target  Pointer to the nanopb struct of the message.
    @param[in] fields  The nanopb fields object of the message, for example
      netconf_NetConf_fields.
    @return 0 on success. -ENOENT if the file does not exist. -EBADMSG if the
      file is not a valid message. Another negative errno on a file system
      error.
******************************************************************************/
int
FsApi_unpack_file(const char *path, void *target, const void *fields)
{
    off_t size;
    int fd;
    int ret;

    CHECK_COND_RETURN(path == NULL || target == NULL || fields == NULL,
        -EINVAL);

    fd = FsApi_open(path, FS_O_READ);
    if (fd < 0)
    {
        return fd;
    }

    size = FsApi_size(fd);
    if (size < 0)
    {
        (void)FsApi_close(fd);
        return (int)size;
    }

    pb_istream_t stream = {
        .callback = read_cb,
        .state = (void *)(intptr_t)fd,
        .bytes_left = (size_t)size,
    };

    ret = 0;
    if (!Pb_unpack_stream(&stream, target, fields))
    {
        LOG_WRN("%s: not a valid message (%d B).", path, (int)size);
        ret = -EBADMSG;
    }

    (void)FsApi_close(fd);
    return ret;
}
