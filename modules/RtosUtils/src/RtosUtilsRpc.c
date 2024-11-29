/*******************************************************************************
 *  @file: RtosUtilsRpc.c
 *
 *  @brief: Handlers for RtosUtilsRpc.
*******************************************************************************/
#include <stdlib.h>
#include "RtosUtilsRpc.h"
#include "RtosUtils.h"
#include "ProtoRpc.pb.h"
#include "RtosUtilsRpc.pb.h"
#include "LogPrint.h"
#include "LogPrint_local.h"

static const char *TAG = "RtosUtilsRpc";

#define MAX(a, b)   ((a) > (b)) ? (a) : (b)

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
    UBaseType_t numTasks;
    TaskStatus_t *tasks;
    int max_tasks = PROTORPC_ARRAY_LENGTH(reply->task_info);
    int name_max = PROTORPC_ARRAY_LENGTH(reply->task_info[0].name);
    int i;
    unsigned long runtime;

    (void)call;

    LOGPRINT_DEBUG("In getSystemTasks handler");

    reply_msg->which_msg = rtos_RtosUtilsCallset_getSystemTasks_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    numTasks = MAX(uxTaskGetNumberOfTasks(), max_tasks);

    LOGPRINT_DEBUG("Allocating for %u tasks (%u max).", numTasks, max_tasks);
    tasks = (TaskStatus_t *)malloc(numTasks*sizeof(TaskStatus_t));
    if (!tasks)
    {
        LOGPRINT_ERROR("Error allocating memory.");
        reply->task_info_count = 0;
        *status = StatusEnum_RPC_HANDLER_ERROR;
        return;
    }

    numTasks = uxTaskGetSystemState(tasks, numTasks, &runtime);
    if (numTasks == 0)
    {
        LOGPRINT_ERROR("No tasks written.");
        reply->task_info_count = 0;
        *status = StatusEnum_RPC_HANDLER_ERROR;
        goto ret;
    }

    for (i = 0; i < numTasks; i++)
    {
        rtos_TaskInfo *info = &reply->task_info[i];
        TaskStatus_t *task = &tasks[i];

        LOGPRINT_DEBUG("task %u: %u %s", i, task->xTaskNumber, task->pcTaskName);
        strncpy(info->name, task->pcTaskName, name_max);
        info->number          = task->xTaskNumber;
        info->state           = task->eCurrentState;
        info->prio            = task->uxCurrentPriority;
        info->rtc             = task->ulRunTimeCounter;
        info->stack_remaining = task->usStackHighWaterMark;

        UBaseType_t coreId = xTaskGetCoreID(task->xHandle);
        if (coreId == tskNO_AFFINITY)
        {
            /* Task was not pinned to a core when created. */
            info->core_num = -1; 
        }
        else
        {
            info->core_num = coreId; 
        }
    }

    reply->task_info_count = numTasks;
    reply->run_time = runtime;

ret:
    free(tasks);

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
