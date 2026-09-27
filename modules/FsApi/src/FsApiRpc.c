/*******************************************************************************
 *  @file: FsApiRpc.c
 *
 *  @brief: Handlers for FsApiRpc. Each handler translates a proto message to
 *  an FsApi call. The reply 'result' field carries the FsApi return value.
 *  The RPC status is RPC_HANDLER_ERROR only for a malformed request.
*******************************************************************************/
#include <string.h>
#include <zephyr/logging/log.h>
#include "FsApi.h"
#include "FsApiRpc.h"
#include "FsApiRpc.pb.h"
#include "pb.h"

LOG_MODULE_REGISTER(FsApiRpc, CONFIG_FSAPIRPC_LOG_LEVEL);

CallsetInfo fsapi_Callset_info = {
    .ver_major = fsapi_CallsetVersion_MAJOR,
    .ver_minor = fsapi_CallsetVersion_MINOR,
    .ver_patch = fsapi_CallsetVersion_PATCH,
    .name = "fsapi",
};

/** @brief Maximum number of entries in one ListDir reply. */
#define LISTDIR_MAX_ENTRIES \
    (sizeof(((fsapi_ListDir_reply *)0)->entries) / \
     sizeof(((fsapi_ListDir_reply *)0)->entries[0]))

/** @brief Scratch entries for ListDir. A struct fs_dirent is too large for
      the RPC thread stack. The RPC server runs one call at a time. */
static struct fs_dirent listdir_entries[LISTDIR_MAX_ENTRIES];

/******************************************************************************
    map_open_flags
*//**
    @brief Maps fsapi_OpenFlags bits to Zephyr FS_O_* flags.
******************************************************************************/
static fs_mode_t
map_open_flags(uint32_t flags)
{
    fs_mode_t mode = 0;

    mode |= (flags & fsapi_OpenFlags_OPEN_READ) ? FS_O_READ : 0;
    mode |= (flags & fsapi_OpenFlags_OPEN_WRITE) ? FS_O_WRITE : 0;
    mode |= (flags & fsapi_OpenFlags_OPEN_CREATE) ? FS_O_CREATE : 0;
    mode |= (flags & fsapi_OpenFlags_OPEN_APPEND) ? FS_O_APPEND : 0;
    mode |= (flags & fsapi_OpenFlags_OPEN_TRUNC) ? FS_O_TRUNC : 0;

    return mode;
}

/******************************************************************************
    fill_info
*//**
    @brief Copies a Zephyr directory entry to a FileInfo message.
******************************************************************************/
static void
fill_info(fsapi_FileInfo *info, const struct fs_dirent *entry)
{
    info->type = (entry->type == FS_DIR_ENTRY_DIR) ?
        fsapi_EntryType_ENTRY_DIR : fsapi_EntryType_ENTRY_FILE;
    info->size = (uint32_t)entry->size;
    strncpy(info->name, entry->name, sizeof(info->name) - 1);
    info->name[sizeof(info->name) - 1] = '\0';
}

/******************************************************************************
    mount_of
*//**
    @brief Returns the mount point which holds a path: the longest mount point
    which is a prefix of the path at a '/' boundary. Returns "" if none.
******************************************************************************/
static const char *
mount_of(const char *path)
{
    const char *best = "";
    size_t best_len = 0;
    int k;

    for (k = 0; k < FsApi_getMountCount(); k++)
    {
        const char *mnt = FsApi_getMountPoint(k);
        size_t len = strlen(mnt);

        if ((len > best_len) && (strncmp(path, mnt, len) == 0) &&
            ((path[len] == '\0') || (path[len] == '/')))
        {
            best = mnt;
            best_len = len;
        }
    }
    return best;
}

/******************************************************************************
    listmounts_handler

    Call params:
    Reply params:
        reply->result: sint32
        reply->mount_points: string[]
*//**
    @brief Implements the RPC listmounts handler.
******************************************************************************/
static void
listmounts_handler(void *call_frame, void *reply_frame, StatusEnum *status)
{
    fsapi_Callset *reply_msg = (fsapi_Callset *)reply_frame;
    fsapi_ListMounts_reply *reply = &reply_msg->msg.listmounts_reply;
    const int max = ARRAY_SIZE(reply->mount_points);
    int num = FsApi_getMountCount();
    int k;

    (void)call_frame;

    LOG_DBG("In listmounts handler");

    reply_msg->which_msg = fsapi_Callset_listmounts_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    memset(reply, 0, sizeof(*reply));
    if (num > max)
    {
        LOG_WRN("%d mounts; the reply holds %d.", num, max);
        num = max;
    }
    for (k = 0; k < num; k++)
    {
        strncpy(reply->mount_points[k], FsApi_getMountPoint(k),
            sizeof(reply->mount_points[k]) - 1);
    }
    reply->mount_points_count = num;
    reply->result = num;
}

/******************************************************************************
    getfsinfo_handler

    Call params:
        call->path: string
    Reply params:
        reply->result: sint32
        reply->mount_point: string
        reply->block_size: uint32
        reply->total_blocks: uint32
        reply->free_blocks: uint32
*//**
    @brief Implements the RPC getfsinfo handler.
******************************************************************************/
static void
getfsinfo_handler(void *call_frame, void *reply_frame, StatusEnum *status)
{
    fsapi_Callset *call_msg = (fsapi_Callset *)call_frame;
    fsapi_Callset *reply_msg = (fsapi_Callset *)reply_frame;
    fsapi_GetFsInfo_call *call = &call_msg->msg.getfsinfo_call;
    fsapi_GetFsInfo_reply *reply = &reply_msg->msg.getfsinfo_reply;
    struct fs_statvfs stat;

    LOG_DBG("In getfsinfo handler");

    reply_msg->which_msg = fsapi_Callset_getfsinfo_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    memset(reply, 0, sizeof(*reply));
    strncpy(reply->mount_point, mount_of(call->path),
        sizeof(reply->mount_point) - 1);

    reply->result = FsApi_getInfo(call->path, &stat);
    if (reply->result == 0)
    {
        reply->block_size = stat.f_frsize;
        reply->total_blocks = stat.f_blocks;
        reply->free_blocks = stat.f_bfree;
    }
}

/******************************************************************************
    stat_handler

    Call params:
        call->path: string
    Reply params:
        reply->result: sint32
        reply->info: FileInfo
*//**
    @brief Implements the RPC stat handler.
******************************************************************************/
static void
stat_handler(void *call_frame, void *reply_frame, StatusEnum *status)
{
    fsapi_Callset *call_msg = (fsapi_Callset *)call_frame;
    fsapi_Callset *reply_msg = (fsapi_Callset *)reply_frame;
    fsapi_Stat_call *call = &call_msg->msg.stat_call;
    fsapi_Stat_reply *reply = &reply_msg->msg.stat_reply;

    LOG_DBG("In stat handler");

    reply_msg->which_msg = fsapi_Callset_stat_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    memset(reply, 0, sizeof(*reply));
    reply->result = FsApi_stat(call->path, &listdir_entries[0]);
    if (reply->result == 0)
    {
        reply->has_info = true;
        fill_info(&reply->info, &listdir_entries[0]);
    }
}

/******************************************************************************
    listdir_handler

    Call params:
        call->path: string
        call->start_idx: uint32
    Reply params:
        reply->result: sint32
        reply->total: uint32
        reply->entries: FileInfo[]
*//**
    @brief Implements the RPC listdir handler.
******************************************************************************/
static void
listdir_handler(void *call_frame, void *reply_frame, StatusEnum *status)
{
    fsapi_Callset *call_msg = (fsapi_Callset *)call_frame;
    fsapi_Callset *reply_msg = (fsapi_Callset *)reply_frame;
    fsapi_ListDir_call *call = &call_msg->msg.listdir_call;
    fsapi_ListDir_reply *reply = &reply_msg->msg.listdir_reply;
    uint32_t total = 0;
    int num;
    int k;

    LOG_DBG("In listdir handler");

    reply_msg->which_msg = fsapi_Callset_listdir_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    memset(reply, 0, sizeof(*reply));
    num = FsApi_listDir(call->path, call->start_idx, listdir_entries,
        LISTDIR_MAX_ENTRIES, &total);

    reply->result = num;
    reply->total = total;
    for (k = 0; k < num; k++)
    {
        fill_info(&reply->entries[k], &listdir_entries[k]);
    }
    reply->entries_count = (num > 0) ? num : 0;
}

/******************************************************************************
    open_handler

    Call params:
        call->path: string
        call->flags: uint32
    Reply params:
        reply->result: sint32
*//**
    @brief Implements the RPC open handler.
******************************************************************************/
static void
open_handler(void *call_frame, void *reply_frame, StatusEnum *status)
{
    fsapi_Callset *call_msg = (fsapi_Callset *)call_frame;
    fsapi_Callset *reply_msg = (fsapi_Callset *)reply_frame;
    fsapi_Open_call *call = &call_msg->msg.open_call;
    fsapi_Open_reply *reply = &reply_msg->msg.open_reply;

    LOG_DBG("In open handler");

    reply_msg->which_msg = fsapi_Callset_open_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    reply->result = FsApi_open(call->path, map_open_flags(call->flags));
}

/******************************************************************************
    close_handler

    Call params:
        call->fd: sint32
    Reply params:
        reply->result: sint32
*//**
    @brief Implements the RPC close handler.
******************************************************************************/
static void
close_handler(void *call_frame, void *reply_frame, StatusEnum *status)
{
    fsapi_Callset *call_msg = (fsapi_Callset *)call_frame;
    fsapi_Callset *reply_msg = (fsapi_Callset *)reply_frame;
    fsapi_Close_call *call = &call_msg->msg.close_call;
    fsapi_Close_reply *reply = &reply_msg->msg.close_reply;

    LOG_DBG("In close handler");

    reply_msg->which_msg = fsapi_Callset_close_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    reply->result = FsApi_close(call->fd);
}

/******************************************************************************
    closeall_handler

    Call params:
    Reply params:
        reply->result: sint32
*//**
    @brief Implements the RPC closeall handler.
******************************************************************************/
static void
closeall_handler(void *call_frame, void *reply_frame, StatusEnum *status)
{
    fsapi_Callset *reply_msg = (fsapi_Callset *)reply_frame;
    fsapi_CloseAll_reply *reply = &reply_msg->msg.closeall_reply;

    (void)call_frame;

    LOG_DBG("In closeall handler");

    reply_msg->which_msg = fsapi_Callset_closeall_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    reply->result = FsApi_closeAll();
}

/******************************************************************************
    read_handler

    Call params:
        call->fd: sint32
        call->size: uint32
        call->use_offset: bool
        call->offset: uint32
    Reply params:
        reply->result: sint32
        reply->data: bytes
*//**
    @brief Implements the RPC read handler.
******************************************************************************/
static void
read_handler(void *call_frame, void *reply_frame, StatusEnum *status)
{
    fsapi_Callset *call_msg = (fsapi_Callset *)call_frame;
    fsapi_Callset *reply_msg = (fsapi_Callset *)reply_frame;
    fsapi_Read_call *call = &call_msg->msg.read_call;
    fsapi_Read_reply *reply = &reply_msg->msg.read_reply;
    ssize_t ret = 0;

    LOG_DBG("In read handler");

    reply_msg->which_msg = fsapi_Callset_read_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;
    reply->data.size = 0;

    if (call->size > sizeof(reply->data.bytes))
    {
        LOG_ERR("Read size %u is larger than %u.", call->size,
            (unsigned int)sizeof(reply->data.bytes));
        reply->result = -EINVAL;
        *status = StatusEnum_RPC_HANDLER_ERROR;
        return;
    }

    if (call->use_offset)
    {
        ret = FsApi_seek(call->fd, call->offset, FS_SEEK_SET);
    }
    if (ret == 0)
    {
        ret = FsApi_read(call->fd, reply->data.bytes, call->size);
    }

    reply->result = ret;
    reply->data.size = (ret > 0) ? ret : 0;
}

/******************************************************************************
    write_handler

    Call params:
        call->fd: sint32
        call->data: bytes
        call->use_offset: bool
        call->offset: uint32
    Reply params:
        reply->result: sint32
*//**
    @brief Implements the RPC write handler.
******************************************************************************/
static void
write_handler(void *call_frame, void *reply_frame, StatusEnum *status)
{
    fsapi_Callset *call_msg = (fsapi_Callset *)call_frame;
    fsapi_Callset *reply_msg = (fsapi_Callset *)reply_frame;
    fsapi_Write_call *call = &call_msg->msg.write_call;
    fsapi_Write_reply *reply = &reply_msg->msg.write_reply;
    ssize_t ret = 0;

    LOG_DBG("In write handler");

    reply_msg->which_msg = fsapi_Callset_write_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    if (call->use_offset)
    {
        ret = FsApi_seek(call->fd, call->offset, FS_SEEK_SET);
    }
    if (ret == 0)
    {
        ret = FsApi_write(call->fd, call->data.bytes, call->data.size);
    }

    reply->result = ret;
}

/******************************************************************************
    seek_handler

    Call params:
        call->fd: sint32
        call->offset: sint32
        call->whence: Whence
    Reply params:
        reply->result: sint32
*//**
    @brief Implements the RPC seek handler.
******************************************************************************/
static void
seek_handler(void *call_frame, void *reply_frame, StatusEnum *status)
{
    fsapi_Callset *call_msg = (fsapi_Callset *)call_frame;
    fsapi_Callset *reply_msg = (fsapi_Callset *)reply_frame;
    fsapi_Seek_call *call = &call_msg->msg.seek_call;
    fsapi_Seek_reply *reply = &reply_msg->msg.seek_reply;
    int whence;
    int ret;

    LOG_DBG("In seek handler");

    reply_msg->which_msg = fsapi_Callset_seek_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    switch (call->whence)
    {
    case fsapi_Whence_SEEK_SET:
        whence = FS_SEEK_SET;
        break;
    case fsapi_Whence_SEEK_CUR:
        whence = FS_SEEK_CUR;
        break;
    case fsapi_Whence_SEEK_END:
        whence = FS_SEEK_END;
        break;
    default:
        reply->result = -EINVAL;
        *status = StatusEnum_RPC_HANDLER_ERROR;
        return;
    }

    ret = FsApi_seek(call->fd, call->offset, whence);
    reply->result = (ret < 0) ? ret : (int32_t)FsApi_tell(call->fd);
}

/******************************************************************************
    size_handler

    Call params:
        call->fd: sint32
    Reply params:
        reply->result: sint32
*//**
    @brief Implements the RPC size handler.
******************************************************************************/
static void
size_handler(void *call_frame, void *reply_frame, StatusEnum *status)
{
    fsapi_Callset *call_msg = (fsapi_Callset *)call_frame;
    fsapi_Callset *reply_msg = (fsapi_Callset *)reply_frame;
    fsapi_Size_call *call = &call_msg->msg.size_call;
    fsapi_Size_reply *reply = &reply_msg->msg.size_reply;

    LOG_DBG("In size handler");

    reply_msg->which_msg = fsapi_Callset_size_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    reply->result = (int32_t)FsApi_size(call->fd);
}

/******************************************************************************
    remove_handler

    Call params:
        call->path: string
    Reply params:
        reply->result: sint32
*//**
    @brief Implements the RPC remove handler.
******************************************************************************/
static void
remove_handler(void *call_frame, void *reply_frame, StatusEnum *status)
{
    fsapi_Callset *call_msg = (fsapi_Callset *)call_frame;
    fsapi_Callset *reply_msg = (fsapi_Callset *)reply_frame;
    fsapi_Remove_call *call = &call_msg->msg.remove_call;
    fsapi_Remove_reply *reply = &reply_msg->msg.remove_reply;

    LOG_DBG("In remove handler");

    reply_msg->which_msg = fsapi_Callset_remove_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    reply->result = FsApi_remove(call->path);
}

/******************************************************************************
    rename_handler

    Call params:
        call->src: string
        call->dst: string
    Reply params:
        reply->result: sint32
*//**
    @brief Implements the RPC rename handler.
******************************************************************************/
static void
rename_handler(void *call_frame, void *reply_frame, StatusEnum *status)
{
    fsapi_Callset *call_msg = (fsapi_Callset *)call_frame;
    fsapi_Callset *reply_msg = (fsapi_Callset *)reply_frame;
    fsapi_Rename_call *call = &call_msg->msg.rename_call;
    fsapi_Rename_reply *reply = &reply_msg->msg.rename_reply;

    LOG_DBG("In rename handler");

    reply_msg->which_msg = fsapi_Callset_rename_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    reply->result = FsApi_rename(call->src, call->dst);
}

/******************************************************************************
    mkdir_handler

    Call params:
        call->path: string
    Reply params:
        reply->result: sint32
*//**
    @brief Implements the RPC mkdir handler.
******************************************************************************/
static void
mkdir_handler(void *call_frame, void *reply_frame, StatusEnum *status)
{
    fsapi_Callset *call_msg = (fsapi_Callset *)call_frame;
    fsapi_Callset *reply_msg = (fsapi_Callset *)reply_frame;
    fsapi_Mkdir_call *call = &call_msg->msg.mkdir_call;
    fsapi_Mkdir_reply *reply = &reply_msg->msg.mkdir_reply;

    LOG_DBG("In mkdir handler");

    reply_msg->which_msg = fsapi_Callset_mkdir_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    reply->result = FsApi_mkdir(call->path);
}

/******************************************************************************
    format_handler

    Call params:
        call->path: string
    Reply params:
        reply->result: sint32
*//**
    @brief Implements the RPC format handler.
******************************************************************************/
static void
format_handler(void *call_frame, void *reply_frame, StatusEnum *status)
{
    fsapi_Callset *call_msg = (fsapi_Callset *)call_frame;
    fsapi_Callset *reply_msg = (fsapi_Callset *)reply_frame;
    fsapi_Format_call *call = &call_msg->msg.format_call;
    fsapi_Format_reply *reply = &reply_msg->msg.format_reply;

    LOG_DBG("In format handler");

    reply_msg->which_msg = fsapi_Callset_format_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    reply->result = FsApi_format(call->path);
}

static ProtoRpc_Handler_Entry handlers[] = {
    PROTORPC_ADD_HANDLER(fsapi_Callset_getfsinfo_call_tag, getfsinfo_handler),
    PROTORPC_ADD_HANDLER(fsapi_Callset_stat_call_tag, stat_handler),
    PROTORPC_ADD_HANDLER(fsapi_Callset_listdir_call_tag, listdir_handler),
    PROTORPC_ADD_HANDLER(fsapi_Callset_open_call_tag, open_handler),
    PROTORPC_ADD_HANDLER(fsapi_Callset_close_call_tag, close_handler),
    PROTORPC_ADD_HANDLER(fsapi_Callset_closeall_call_tag, closeall_handler),
    PROTORPC_ADD_HANDLER(fsapi_Callset_read_call_tag, read_handler),
    PROTORPC_ADD_HANDLER(fsapi_Callset_write_call_tag, write_handler),
    PROTORPC_ADD_HANDLER(fsapi_Callset_seek_call_tag, seek_handler),
    PROTORPC_ADD_HANDLER(fsapi_Callset_size_call_tag, size_handler),
    PROTORPC_ADD_HANDLER(fsapi_Callset_remove_call_tag, remove_handler),
    PROTORPC_ADD_HANDLER(fsapi_Callset_rename_call_tag, rename_handler),
    PROTORPC_ADD_HANDLER(fsapi_Callset_mkdir_call_tag, mkdir_handler),
    PROTORPC_ADD_HANDLER(fsapi_Callset_format_call_tag, format_handler),
    PROTORPC_ADD_HANDLER(fsapi_Callset_listmounts_call_tag, listmounts_handler),
};

#define NUM_HANDLERS    PROTORPC_ARRAY_LENGTH(handlers)

/******************************************************************************
    [docimport FsApiRpc_resolver]
*//**
    @brief Resolver function for FsApiRpc.
    @param[in] call_frame  Pointer to the unpacked call frame object.
    @param[out] which_msg  Output which_msg was requested.
******************************************************************************/
ProtoRpc_handler *
FsApiRpc_resolver(void *call_frame, uint32_t *which_msg)
{
    fsapi_Callset *this = (fsapi_Callset *)call_frame;
    unsigned int i;

    *which_msg = this->which_msg;

    /** @brief Handler lookup */
    for (i = 0; i < NUM_HANDLERS; i++)
    {
        ProtoRpc_Handler_Entry *entry = &handlers[i];
        if (entry->tag == this->which_msg)
        {
            return entry->handler;
        }
    }

    return NULL;
}
