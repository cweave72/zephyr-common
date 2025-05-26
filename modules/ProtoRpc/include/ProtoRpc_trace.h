/*******************************************************************************
 *  @file: ProtoRpc_trace.h
 *   
 *  @brief: Header file defining the module trace events and levels.
*******************************************************************************/
#ifndef PROTORPC_TRACE_H
#define PROTORPC_TRACE_H

#if defined(CONFIG_TRACEMODULE) && defined(CONFIG_PROTORPC_TRACE_MODULE)
#include "TraceModule.h"

#if CONFIG_PROTORPC_TRACE_MODULE_ID == -1
#error "Symbol CONFIG_PROTORPC_TRACE_MODULE_ID (-1) must have a valid value defined."
#else
#define PROTORPC_TRACE_MODULE_ID      CONFIG_PROTORPC_TRACE_MODULE_ID
#endif

/* Events */
#define PROTORPC_EVENT_HEADER           1

/* Event Levels (Compared against CONFIG_PROTORPC_TRACE_MODULE_LEVEL). */
#define PROTORPC_EVENT_HEADER_LEVEL     TRACEMODULE_LEVEL_DEBUG

static inline void _protorpc_trace_header(
    uint32_t seqn,
    uint32_t which_callset,
    uint32_t which_msg)
{
    uint8_t mod = PROTORPC_TRACE_MODULE_ID;
    uint8_t ev = PROTORPC_EVENT_HEADER;

    TRACEMODULE_EVENT(
        CONFIG_PROTORPC_TRACE_MODULE_LEVEL,
        PROTORPC_EVENT_HEADER_LEVEL,
        mod,
        ev,
        seqn, which_callset, which_msg);
}

#define protorpc_trace_header(seqn, which_callset, which_msg)\
    _protorpc_trace_header(seqn, which_callset, which_msg)

#else
#define protorpc_trace_header(seqn, which_callset, which_msg)
#endif



#endif
