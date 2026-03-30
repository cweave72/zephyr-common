/*******************************************************************************
 *  @file: SystemRpc.c
 *
 *  @brief: Handlers for SystemRpc.
*******************************************************************************/
#include <zephyr/logging/log.h>
#include "SystemRpc.h"
#include "SystemRpc.pb.h"
#include "pb.h"

LOG_MODULE_REGISTER(SystemRpc, CONFIG_SYSTEMRPC_LOG_LEVEL);

CallsetInfo system_Callset_info = {
    .ver_major = system_CallsetVersion_MAJOR,
    .ver_minor = system_CallsetVersion_MINOR,
    .ver_patch = system_CallsetVersion_PATCH,
    .name = "system",
};

#if defined(CONFIG_TRACERAM)
#include "TraceRam.h"
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
    system_Callset *call_msg = (system_Callset *)call_frame;
    system_Callset *reply_msg = (system_Callset *)reply_frame;
    system_DumpMem_call *call = &call_msg->msg.dumpmem_call;
    system_DumpMem_reply *reply = &reply_msg->msg.dumpmem_reply;
    uint8_t *bytearray = reply->mem.bytes;
    uint8_t *memory = (uint8_t *)(uintptr_t)call->address;

    (void)call;
    (void)reply;

    LOG_DBG("In dumpmem handler");

    reply_msg->which_msg = system_Callset_dumpmem_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    LOG_DBG("reply->mem.size = %u", (unsigned int)sizeof(reply->mem.bytes));
    if (call->size <= sizeof(reply->mem.bytes))
    {
        LOG_DBG("Copying %u bytes from 0x%08x.", call->size,
            (unsigned int)call->address);
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
    system_Callset *call_msg = (system_Callset *)call_frame;
    system_Callset *reply_msg = (system_Callset *)reply_frame;
    system_GetTraceRamStatus_call *call = &call_msg->msg.gettraceramstatus_call;
    system_GetTraceRamStatus_reply *reply = &reply_msg->msg.gettraceramstatus_reply;

    (void)call;
    (void)reply;

    LOG_DBG("In gettraceramstatus handler");

    reply_msg->which_msg = system_Callset_gettraceramstatus_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

#if defined(CONFIG_TRACERAM)
    reply->state = TraceRam_getState();
    reply->count = TraceRam_getCount();
#else
    reply->state = 0;
    reply->count = 0;
#endif
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
    system_Callset *call_msg = (system_Callset *)call_frame;
    system_Callset *reply_msg = (system_Callset *)reply_frame;
    system_EnableTraceRam_call *call = &call_msg->msg.enabletraceram_call;
    system_EnableTraceRam_reply *reply = &reply_msg->msg.enabletraceram_reply;

    (void)call;
    (void)reply;

    LOG_DBG("In enabletraceram handler");

    reply_msg->which_msg = system_Callset_enabletraceram_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

#if defined(CONFIG_TRACERAM)
    TraceRam_enable();
    reply->state = TraceRam_getState();
#else
    reply->state = 0;
#endif
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
    system_Callset *call_msg = (system_Callset *)call_frame;
    system_Callset *reply_msg = (system_Callset *)reply_frame;
    system_DisableTraceRam_call *call = &call_msg->msg.disabletraceram_call;
    system_DisableTraceRam_reply *reply = &reply_msg->msg.disabletraceram_reply;

    (void)call;
    (void)reply;

    LOG_DBG("In disabletraceram handler");

    reply_msg->which_msg = system_Callset_disabletraceram_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

#if defined(CONFIG_TRACERAM)
    /* Sleep 50 ms to allow tracing thread to run and clear any pending writes
        to TraceRam prior to disabling. Note:
        CONFIG_TRACING_THREAD_WAIT_THRESHOLD should be less than 50 ms.
    */
    k_msleep(50);
    TraceRam_disable();
    reply->state = TraceRam_getState();
#else
    reply->state = 0;
#endif
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
    system_Callset *call_msg = (system_Callset *)call_frame;
    system_Callset *reply_msg = (system_Callset *)reply_frame;
    system_GetNextTraceRam_call *call = &call_msg->msg.getnexttraceram_call;
    system_GetNextTraceRam_reply *reply = &reply_msg->msg.getnexttraceram_reply;

    (void)call;
    (void)reply;

    LOG_DBG("In getnexttraceram handler");

    reply_msg->which_msg = system_Callset_getnexttraceram_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    reply->empty_on_read = false;

#if defined(CONFIG_TRACERAM)
    if (TraceRam_getCount() == 0)
    {
        LOG_DBG("TraceRam is empty.");
        reply->data.size = 0;
        return;
    }

    int num_read = TraceRam_read(reply->data.bytes, call->max_size);
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
#else
    *status = StatusEnum_RPC_HANDLER_ERROR;
    reply->data.size = 0;
#endif
}



static ProtoRpc_Handler_Entry handlers[] = {
    PROTORPC_ADD_HANDLER(system_Callset_dumpmem_call_tag, dumpmem),
    PROTORPC_ADD_HANDLER(system_Callset_gettraceramstatus_call_tag, gettraceramstatus),
    PROTORPC_ADD_HANDLER(system_Callset_enabletraceram_call_tag, enabletraceram),
    PROTORPC_ADD_HANDLER(system_Callset_disabletraceram_call_tag, disabletraceram),
    PROTORPC_ADD_HANDLER(system_Callset_getnexttraceram_call_tag, getnexttraceram),
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
    system_Callset *this = (system_Callset *)call_frame;
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
