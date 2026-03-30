/*******************************************************************************
 *  @file: RtosUtilsRpc.c
 *
 *  @brief: Handlers for RtosUtilsRpc.
*******************************************************************************/
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "RtosUtilsRpc.h"
#include "RtosUtils.h"

LOG_MODULE_DECLARE(RtosUtils, CONFIG_RTOSUTILS_LOG_LEVEL);

CallsetInfo rtosutils_Callset_info = {
    .ver_major = rtosutils_CallsetVersion_MAJOR,
    .ver_minor = rtosutils_CallsetVersion_MINOR,
    .ver_patch = rtosutils_CallsetVersion_PATCH,
    .name = "rtosutils",
};

struct ctx_object {
    uint8_t loop_idx;
    uint8_t start_thread_idx;
    uint8_t curr_thread_idx;
    uint8_t max_reportable;
    uint8_t name_max;
    uint8_t thread_cnt;
    int ret;
    rtosutils_GetSystemThreads_reply *reply;
};

/******************************************************************************
    [docimport thread_info_cb]
*//**
    @brief Description.
******************************************************************************/
static void
thread_info_cb(const struct k_thread *cthread, void *data)
{
    struct ctx_object *ctx = (struct ctx_object *)data;
    struct k_thread *thread = (struct k_thread *)cthread;
    const char *tname;
    k_thread_runtime_stats_t rt_stats_thread;
    size_t unused;

    ctx->thread_cnt++;

    /* If we've hit our reportable limit, signal with ret = -1 and return. */
    if (ctx->loop_idx == ctx->max_reportable)
    {
        ctx->ret = -1;
        return;
    }

    if (ctx->curr_thread_idx >= ctx->start_thread_idx)
    {
        rtosutils_ThreadInfo *tinfo = &ctx->reply->thread_info[ctx->loop_idx];
        char *name = ctx->reply->thread_info[ctx->loop_idx].name;

        tname = k_thread_name_get(thread);
        strncpy(name, tname, ctx->name_max-1);
        tinfo->name[ctx->name_max-1] = '\0';

        tinfo->tid = (uint32_t)thread;
        tinfo->state = thread->base.thread_state;
        tinfo->prio = thread->base.prio;

#ifdef CONFIG_THREAD_RUNTIME_STATS
	if (k_thread_runtime_stats_get(thread, &rt_stats_thread) < 0)
        {
	    tinfo->current_cycles = 0;
	    tinfo->peak_cycles = 0;
	    tinfo->avg_cycles = 0;
	    tinfo->total_cycles = 0;
	}
#ifdef CONFIG_SCHED_THREAD_USAGE_ANALYSIS
        else
        {
	    tinfo->current_cycles = rt_stats_thread.current_cycles;
	    tinfo->peak_cycles = rt_stats_thread.peak_cycles;
	    tinfo->avg_cycles = rt_stats_thread.average_cycles;
	    tinfo->total_cycles = rt_stats_thread.execution_cycles;
        }
#else
        else
        {
	    tinfo->current_cycles = 0;
	    tinfo->peak_cycles = 0;
	    tinfo->avg_cycles = 0;
	    tinfo->total_cycles = rt_stats_thread.execution_cycles;
        }
#endif
#else
        tinfo->current_cycles = 0;
        tinfo->peak_cycles = 0;
        tinfo->avg_cycles = 0;
        tinfo->total_cycles = 0;
#endif
        tinfo->stack_size = thread->stack_info.size;
        if (k_thread_stack_space_get(thread, &unused) < 0)
        {
            tinfo->unused_stack = 0;
        }
        else
        {
            tinfo->unused_stack = unused;
        }
    }

    /* Iterate the current thread index. */
    ctx->curr_thread_idx++;

    /* Iteration the loop index. */
    ctx->loop_idx++;

    ctx->ret = 0;
}

/******************************************************************************
    getSystemThreads

    Call params:
    Reply params:
        reply->run_time: uint64 
        reply->thread_info: message [repeated]
*//**
    @brief Implements the RPC getSystemTasks handler.
******************************************************************************/
static void
getSystemThreads(void *call_frame, void *reply_frame, StatusEnum *status)
{
    rtosutils_Callset *call_msg = (rtosutils_Callset *)call_frame;
    rtosutils_Callset *reply_msg = (rtosutils_Callset *)reply_frame;
    rtosutils_GetSystemThreads_call *call = &call_msg->msg.getSystemThreads_call;
    rtosutils_GetSystemThreads_reply *reply = &reply_msg->msg.getSystemThreads_reply;
    int max_threads_reportable = PROTORPC_ARRAY_LENGTH(reply->thread_info);
    int name_max = PROTORPC_ARRAY_LENGTH(reply->thread_info[0].name);
    struct ctx_object ctx;
    k_thread_runtime_stats_t rt_stats_all;

    LOG_DBG("In getSystemTasks handler");

    reply_msg->which_msg = rtosutils_Callset_getSystemThreads_reply_tag;
    *status = StatusEnum_RPC_SUCCESS;

    ctx.loop_idx = 0;
    ctx.thread_cnt = 0;
    ctx.curr_thread_idx = call->idx_start;
    ctx.start_thread_idx = call->idx_start;
    ctx.max_reportable = max_threads_reportable;
    ctx.name_max = name_max;
    ctx.reply = reply;

    k_thread_foreach_unlocked(thread_info_cb, (void *)&ctx);
    reply->thread_info_count = ctx.loop_idx;
    LOG_DBG("loop_idx=%u", ctx.loop_idx);

    if (k_thread_runtime_stats_all_get(&rt_stats_all) < 0) {
        reply->total_cycles = 0;
    }
    else
    {
        reply->total_cycles = rt_stats_all.execution_cycles;
    }

    reply->idx_start = call->idx_start;
    reply->num_threads = ctx.thread_cnt;
}


static ProtoRpc_Handler_Entry handlers[] = {
    PROTORPC_ADD_HANDLER(rtosutils_Callset_getSystemThreads_call_tag, getSystemThreads),
};

#define NUM_HANDLERS    PROTORPC_ARRAY_LENGTH(handlers)

/******************************************************************************
    [docimport RtosUtilsRpc_resolver]
*//**
    @brief Resolver function for RtosUtilsRpc.
    @param[in] call_frame  Pointer to the unpacked call frame object.
    @param[out] which_msg  Output which_msg was requested.
******************************************************************************/
ProtoRpc_handler *
RtosUtilsRpc_resolver(void *call_frame, uint32_t *which_msg)
{
    rtosutils_Callset *this = (rtosutils_Callset *)call_frame;
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
