/*******************************************************************************
 *  @file: SystemRpc.c
 *
 *  @brief: Handlers for SystemRpc.
*******************************************************************************/
#include <zephyr/logging/log.h>
#include "SystemRpc.h"
#include "SystemRpc.pb.h"
#include "pb.h"
#include "TraceRam.h"

LOG_MODULE_REGISTER(SystemRpc, CONFIG_SYSTEMRPC_LOG_LEVEL);

#if defined(CONFIG_TRACING_BACKEND_RAM)
#define RAM_TRACEBUFFER_SIZE    CONFIG_RAM_TRACING_BUFFER_SIZE
extern uint8_t ram_tracing[];
#else
#define RAM_TRACEBUFFER_SIZE    0
#endif

/******************************************************************************
    dumpmem

    Call params:
        call->address: uint32 
        call->size: uint32 
    Reply params:
        reply->mem: bytes 
*//**
    @brief Implements the RPC dumpmem handler.
******************************************************************************/
static void
dumpmem(void *call_frame, void *reply_frame, StatusEnum *status)
{
    system_SystemCallset *call_msg = (system_SystemCallset *)call_frame;
    system_SystemCallset *reply_msg = (system_SystemCallset *)reply_frame;
    system_DumpMem_call *call = &call_msg->msg.dumpmem_call;
    system_DumpMem_reply *reply = &reply_msg->msg.dumpmem_reply;
    uint8_t *bytearray = reply->mem.bytes;
    uint8_t *memory = (uint8_t *)call->address;

    (void)call;
    (void)reply;

    LOG_DBG("In dumpmem handler");

    reply_msg->which_msg = system_SystemCallset_dumpmem_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    LOG_DBG("reply->mem.size = %u", sizeof(reply->mem.bytes));
    if (call->size <= sizeof(reply->mem.bytes))
    {
        LOG_DBG("Copying %u bytes from 0x%08x.", call->size, call->address);
        memcpy(bytearray, memory, call->size);
        reply->mem.size = call->size;
    }
    else
    {
        LOG_ERR("Request to copy %u bytes from 0x%08x is too large.",
            call->size, call->address);
        *status = StatusEnum_RPC_HANDLER_ERROR;
    }
}

/******************************************************************************
    gettraceramstatus

    Call params:
    Reply params:
        reply->state: bool 
        reply->count: uint32 
*//**
    @brief Implements the RPC gettraceramstatus handler.
******************************************************************************/
static void
gettraceramstatus(void *call_frame, void *reply_frame, StatusEnum *status)
{
    system_SystemCallset *call_msg = (system_SystemCallset *)call_frame;
    system_SystemCallset *reply_msg = (system_SystemCallset *)reply_frame;
    system_GetTraceRamStatus_call *call = &call_msg->msg.gettraceramstatus_call;
    system_GetTraceRamStatus_reply *reply = &reply_msg->msg.gettraceramstatus_reply;

    (void)call;
    (void)reply;

    LOG_DBG("In gettraceramstatus handler");

    reply_msg->which_msg = system_SystemCallset_gettraceramstatus_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    reply->state = TraceRam_getState();
    reply->count = TraceRam_getCount();
}

/******************************************************************************
    enabletraceram

    Call params:
    Reply params:
        reply->state: bool 
*//**
    @brief Implements the RPC enabletraceram handler.
******************************************************************************/
static void
enabletraceram(void *call_frame, void *reply_frame, StatusEnum *status)
{
    system_SystemCallset *call_msg = (system_SystemCallset *)call_frame;
    system_SystemCallset *reply_msg = (system_SystemCallset *)reply_frame;
    system_EnableTraceRam_call *call = &call_msg->msg.enabletraceram_call;
    system_EnableTraceRam_reply *reply = &reply_msg->msg.enabletraceram_reply;

    (void)call;
    (void)reply;

    LOG_DBG("In enabletraceram handler");

    reply_msg->which_msg = system_SystemCallset_enabletraceram_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    TraceRam_enable();
    reply->state = TraceRam_getState();
}

/******************************************************************************
    disabletraceram

    Call params:
    Reply params:
        reply->state: bool 
*//**
    @brief Implements the RPC disabletraceram handler.
******************************************************************************/
static void
disabletraceram(void *call_frame, void *reply_frame, StatusEnum *status)
{
    system_SystemCallset *call_msg = (system_SystemCallset *)call_frame;
    system_SystemCallset *reply_msg = (system_SystemCallset *)reply_frame;
    system_DisableTraceRam_call *call = &call_msg->msg.disabletraceram_call;
    system_DisableTraceRam_reply *reply = &reply_msg->msg.disabletraceram_reply;

    (void)call;
    (void)reply;

    LOG_DBG("In disabletraceram handler");

    reply_msg->which_msg = system_SystemCallset_disabletraceram_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    TraceRam_disable();
    reply->state = TraceRam_getState();
}

/******************************************************************************
    getnexttraceram

    Call params:
        call->max_size: uint32 
    Reply params:
        reply->empty_on_read: bool 
        reply->data: bytes 
*//**
    @brief Implements the RPC getnexttraceram handler.
******************************************************************************/
static void
getnexttraceram(void *call_frame, void *reply_frame, StatusEnum *status)
{
    system_SystemCallset *call_msg = (system_SystemCallset *)call_frame;
    system_SystemCallset *reply_msg = (system_SystemCallset *)reply_frame;
    system_GetNextTraceRam_call *call = &call_msg->msg.getnexttraceram_call;
    system_GetNextTraceRam_reply *reply = &reply_msg->msg.getnexttraceram_reply;
    int num_read = 0;

    (void)call;
    (void)reply;

    LOG_DBG("In getnexttraceram handler");

    reply_msg->which_msg = system_SystemCallset_getnexttraceram_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    reply->empty_on_read = false;

    if (TraceRam_getCount() == 0)
    {
        LOG_DBG("TraceRam is empty.");
        reply->data.size = 0;
        return;
    }

    num_read = TraceRam_read(reply->data.bytes, call->max_size);
    if (num_read <= 0)
    {
        LOG_ERR("TraceRam error: %d", num_read);
        *status = StatusEnum_RPC_HANDLER_ERROR;
        reply->data.size = 0;
        return;
    }

    reply->data.size = num_read;
    reply->empty_on_read = (TraceRam_getCount() == 0) ? true : false;
    LOG_DBG("Total read: %u; empty_on_read: %u", num_read, reply->empty_on_read);
}



static ProtoRpc_Handler_Entry handlers[] = {
    PROTORPC_ADD_HANDLER(system_SystemCallset_dumpmem_call_tag, dumpmem),
    PROTORPC_ADD_HANDLER(system_SystemCallset_gettraceramstatus_call_tag, gettraceramstatus),
    PROTORPC_ADD_HANDLER(system_SystemCallset_enabletraceram_call_tag, enabletraceram),
    PROTORPC_ADD_HANDLER(system_SystemCallset_disabletraceram_call_tag, disabletraceram),
    PROTORPC_ADD_HANDLER(system_SystemCallset_getnexttraceram_call_tag, getnexttraceram),
};

#define NUM_HANDLERS    PROTORPC_ARRAY_LENGTH(handlers)

/******************************************************************************
    [docimport SystemRpc_resolver]
*//**
    @brief Resolver function for SystemRpc.
    @param[in] call_frame  Pointer to the unpacked call frame object.
    @param[out] which_msg  Output which_msg was requested.
******************************************************************************/
ProtoRpc_handler *
SystemRpc_resolver(void *call_frame, uint32_t *which_msg)
{
    system_SystemCallset *this = (system_SystemCallset *)call_frame;
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
