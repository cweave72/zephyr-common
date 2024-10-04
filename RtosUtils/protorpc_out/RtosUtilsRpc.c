/*******************************************************************************
 *  @file: RtosUtilsRpc.c
 *
 *  @brief: Handlers for RtosUtilsRpc.
*******************************************************************************/
#include "ProtoRpc.h"
#include "LogPrint.h"
#include "ProtoRpc.pb.h"
#include "RtosUtilsRpc.pb.h"

static const char *TAG = "RtosUtilsRpc";

/******************************************************************************
    getSystemTasks

    Call params:
    Reply params:
        reply->run_time: uint64 
        reply->task_info: message [repeated]
*//**
    @brief Implements the RPC getSystemTasks handler.
******************************************************************************/
static void
getSystemTasks(void *call_frame, void *reply_frame, StatusEnum *status)
{
    rtos_RtosUtilsCallset *call_msg = (rtos_RtosUtilsCallset *)call_frame;
    rtos_RtosUtilsCallset *reply_msg = (rtos_RtosUtilsCallset *)reply_frame;
    rtos_GetSystemTasks_call *call = &call_msg->msg.getSystemTasks_call;
    rtos_GetSystemTasks_reply *reply = &reply_msg->msg.getSystemTasks_reply;

    (void)call;
    (void)reply;

    LOGPRINT_DEBUG("In getSystemTasks handler");

    reply_msg->which_msg = rtos_RtosUtilsCallset_getSystemTasks_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    /* TODO: Implement handler */
}



static ProtoRpc_Handler_Entry handlers[] = {
    PROTORPC_ADD_HANDLER(rtos_RtosUtilsCallset_getSystemTasks_call_tag, getSystemTasks),
};

#define NUM_HANDLERS    PROTORPC_ARRAY_LENGTH(handlers)

/******************************************************************************
    [docimport RtosUtilsRpc_resolver]
*//**
    @brief Resolver function for RtosUtilsRpc.
    @param[in] call_frame  Pointer to the unpacked call frame object.
    @param[in] offset  Offset of the callset member within the call_frame.
******************************************************************************/
ProtoRpc_handler *
RtosUtilsRpc_resolver(void *call_frame, uint32_t offset)
{
    uint8_t *frame = (uint8_t *)call_frame;
    rtos_RtosUtilsCallset *this = (rtos_RtosUtilsCallset *)&frame[offset];
    unsigned int i;

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