/*******************************************************************************
 *  @file: TraceModule.h
 *   
 *  @brief: Module-specific tracing extension.
*******************************************************************************/
#ifndef TRACEMODULE_H
#define TRACEMODULE_H

#include <zephyr/kernel.h>
#include "ctf_top.h"

/** @brief Entrypoint ID into ctf_top events (see ctf_top.h). */
#define TRACEMODULE_EVENT_ID    0x63

#define TRACEMODULE_MAX_STRING_LEN  20

typedef struct tracemodule_bounded_str
{
    char buf[TRACEMODULE_MAX_STRING_LEN];
} tracemodule_bounded_str;

typedef enum tracemodule_levels
{
    TRACEMODULE_LEVEL_CRITICAL = 0,
    TRACEMODULE_LEVEL_ERROR    = 1,
    TRACEMODULE_LEVEL_WARNING  = 2,
    TRACEMODULE_LEVEL_INFO     = 3,
    TRACEMODULE_LEVEL_DEBUG    = 4,
    TRACEMODULE_LEVEL_VERBOSE  = 5
} tracemodule_levels;

/** @brief Macro for tracing module-level events.
    The event will be traced for any event level that is <= the module level.
    @param[in] modlevel  The module tracing level setting.
    @param[in] evlevel  The event level.
    @param[in] module  The module ID.
    @param[in] event  The event ID.
*/
#define TRACEMODULE_EVENT(modlevel, evlevel, module, event, ...)                            \
do {                                                                                        \
    if ((evlevel) <= (modlevel)) {                                                          \
        CTF_EVENT(CTF_LITERAL(uint8_t, TRACEMODULE_EVENT_ID), module, event, __VA_ARGS__);  \
    }                                                                                       \
} while (0)

#endif
