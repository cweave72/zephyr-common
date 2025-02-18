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
    gettraceramaddr

    Call params:
    Reply params:
        reply->address: uint32 
        reply->size: uint32 
*//**
    @brief Implements the RPC gettraceramaddr handler.
******************************************************************************/
static void
gettraceramaddr(void *call_frame, void *reply_frame, StatusEnum *status)
{
    system_SystemCallset *call_msg = (system_SystemCallset *)call_frame;
    system_SystemCallset *reply_msg = (system_SystemCallset *)reply_frame;
    system_GetTraceRamAddr_call *call = &call_msg->msg.gettraceramaddr_call;
    system_GetTraceRamAddr_reply *reply = &reply_msg->msg.gettraceramaddr_reply;

    (void)call;
    (void)reply;

    LOG_DBG("In gettraceramaddr handler");

    reply_msg->which_msg = system_SystemCallset_gettraceramaddr_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

#if defined(CONFIG_TRACING_BACKEND_RAM)
    reply->address = (uint32_t)ram_tracing;
    reply->size = RAM_TRACEBUFFER_SIZE;
#else
    reply->address = 0;
    reply->size = 0;
#endif
}


static ProtoRpc_Handler_Entry handlers[] = {
    PROTORPC_ADD_HANDLER(system_SystemCallset_dumpmem_call_tag, dumpmem),
    PROTORPC_ADD_HANDLER(system_SystemCallset_gettraceramaddr_call_tag, gettraceramaddr),
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
