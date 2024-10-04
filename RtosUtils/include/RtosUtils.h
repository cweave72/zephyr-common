/*******************************************************************************
 *  @file: RtosUtils.h
 *   
 *  @brief: Header for RtosUtils.
*******************************************************************************/
#ifndef RTOSUTILS_H
#define RTOSUTILS_H

#include <zephyr/kernel.h>
#include <zephyr/sys/time_units.h>
#include <zephyr/logging/log.h>

/** @brief Define a task's stack. */
#define RTOS_TASK_STACK_DEFINE(name, size)  K_THREAD_STACK_DEFINE((name), (size))
/** @brief Task type. */
#define RTOS_TASK   struct k_thread

/** @brief Task creation.  Returns 0 on success, -1 on error. */
#define RTOS_TASK_CREATE(func, name, stack, params, prio, handle)    \
({                                                                   \
    k_tid_t tid;                                                     \
    int ret = 0;                                                     \
    tid = k_thread_create(                                           \
        (handle),                                                    \
        (stack),                                                     \
        K_THREAD_STACK_SIZEOF((stack)),                              \
        (func),                                                      \
        (params),                                                    \
        NULL,                                                        \
        NULL,                                                        \
        (prio),                                                      \
        0,                                                           \
        K_NO_WAIT);                                                  \
    ret = k_thread_name_set(tid, (name));                            \
    if (ret != 0)                                                    \
    {                                                                \
        LOG_ERR("Error naming task (%s): (%d).", (name), ret);       \
        ret = -1;                                                    \
    }                                                                \
    ret;                                                             \
})

/** @brief Task creation pinned to core.  Returns 0 on success, -1 on error. */
#define RTOS_TASK_CREATE_PINNED(func, name, stack, params, prio, handle, core)\
({                                                                            \
    k_tid_t tid;                                                     \
    int ret = 0;                                                     \
    tid = k_thread_create(                                           \
        (handle),                                                    \
        (stack),                                                     \
        K_THREAD_STACK_SIZEOF((stack)),                              \
        (func),                                                      \
        (params),                                                    \
        NULL,                                                        \
        NULL,                                                        \
        (prio),                                                      \
        0,                                                           \
        K_NO_WAIT);                                                  \
    ret = k_thread_cpu_pin(tid, (core));                             \
    if (ret != 0)                                                    \
    {                                                                \
        LOG_ERR("Error pinning task (%s): (%d).", (name), ret);      \
    }                                                                \
    ret = k_thread_name_set(tid, (name));                            \
    if (ret != 0)                                                    \
    {                                                                \
        LOG_ERR("Error naming task (%s): (%d).", (name), ret);       \
    }                                                                \
    ret;                                                             \
})

#define RTOS_MS_TO_TICKS(ms)        k_ms_to_ticks_ceil32((ms))
#define RTOS_SEC_TO_TICKS(s)        k_ms_to_ticks_ceil32((s)*1000)

/** @brief Macro wrapper for task sleep. */
#define RTOS_TASK_SLEEP_ms(ms)      k_msleep((ms))
#define RTOS_TASK_SLEEP_s(s)        k_msleep((s)*1000)

/** @brief Event Flag Macros */
/* Generic flag type. */
#define RTOS_FLAGS                      struct k_event
/* Generic flag runtime initializer.
Example:
    RTOS_FLAGS my_flags;
    RTOS_FLAGS_INIT(&my_flags);
*/
#define RTOS_FLAGS_INIT(grp)            k_event_init((grp))
/* Declare and init an event object at compile-time. */
#define RTOS_FLAGS_CREATE(name)         K_EVENT_DEFINE(name)

/*  Wait on all flags forever, do not clear.
    This macro will only return if all event flags are set.
*/
#define RTOS_PEND_ALL_FLAGS(grp, eflags)                        \
({                                                              \
  uint32_t flags;                                               \
  flags = k_event_wait_all((grp), (eflags), false, K_FOREVER);  \
  flags;                                                        \
})

/*  Wait on any flags forever, do not clear.
    This macro will only return if all event flags are set.
*/
#define RTOS_PEND_ANY_FLAGS(grp, eflags)                    \
({                                                          \
  uint32_t flags;                                           \
  flags = k_event_wait((grp), (eflags), false, K_FOREVER);  \
  flags;                                                    \
})

#define RTOS_PEND_ALL_FLAGS_MS(grp, eflags, ms)                    \
({                                                                 \
  uint32_t flags;                                                  \
  flags = k_event_wait_all((grp), (eflags), false, K_MSEC((ms)));  \
  flags;                                                           \
})

/*  Wait on any flags with timeout, do not clear.
    This macro will only return if all event flags are set.
*/
#define RTOS_PEND_ANY_FLAGS_MS(grp, eflags, ms)                \
({                                                             \
  uint32_t flags;                                              \
  flags = k_event_wait((grp), (eflags), false, K_MSEC((ms)));  \
  flags;                                                       \
})

/*  Wait on all flags forever, self-clear
    This macro will only return if all event flags are set.
*/
#define RTOS_PEND_ALL_FLAGS_CLR(grp, eflags)                   \
({                                                             \
  uint32_t flags;                                              \
  flags = k_event_wait_all((grp), (eflags), true, K_FOREVER);  \
  flags;                                                       \
})

/*  Wait on all flags for x ms, self-clear.
    This macro will return when either all the flags are set or a timeout occurred.
    Test flags on return. If (flags & eflags) != eflags, timeout occurred.
*/
#define RTOS_PEND_ALL_FLAGS_CLR_MS(grp, eflags, ms)                        \
({                                                                         \
  uint32_t flags;                                                          \
  flags = k_event_wait_all((grp), (eflags), true, K_MSEC((ms)));           \
  flags;                                                                   \
})

/*  Wait on any flags forever, self-clear
    This macro will return if any event flags are set. Test returned flags to
    determine which one was set.
*/
#define RTOS_PEND_ANY_FLAGS_CLR(grp, eflags)               \
({                                                         \
  uint32_t flags;                                          \
  flags = k_event_wait((grp), (eflags), true, K_FOREVER);  \
  flags;                                                   \
})

/*  Wait on any flags for x ms, self-clear.
    This macro will return when either any the flags are set or a timeout occurred.
    Test flags on return to determine if any were set. If none are set, a
    timeout occurred.
*/
#define RTOS_PEND_ANY_FLAGS_CLR_MS(grp, eflags, ms)                      \
({                                                                       \
  uint32_t flags;                                                        \
  flags = k_event_wait((grp), (eflags), true, K_MSEC((ms)));             \
  flags;                                                                 \
})

/* Set Flags */
#define RTOS_FLAGS_SET(grp, eflags)     k_event_set((grp), (eflags))

/* Clear Flags */
#define RTOS_FLAGS_CLR(grp, eflags)     k_event_clear((grp), (eflags))

/* Get Current Flags. */
#define RTOS_FLAGS_GET(grp)             k_event_test((grp), 0xffffffff)

/** @brief Mutex Create Helper macros. Returns SemaphoreHandle_t object. */

#define RTOS_MUTEX                          struct k_mutex
/** @brief Declare and init a mutex object at compile-time.
Example:
    RTOS_MUTEX_CREATE(my_mutex);
    {
        // use the mutex
    }
*/
#define RTOS_MUTEX_CREATE(pmut)             K_MUTEX_DEFINE((pmut))

/** @brief Init a mutex object,
Example: 
    RTOS_MUTEX my_mutex;
    ...
    {
        RTOS_INIT(&my_mutex);
    }
*/
#define RTOS_MUTEX_INIT(pmut)               k_mutex_init((pmut))

/** @brief Macro to take a mutex, waiting forever. */
#define RTOS_MUTEX_GET(m)                   k_mutex_lock((m), K_FOREVER) 

/** @brief Macro to take a mutex, waiting a timeout, in ms. Returns pdTrue if
      the mutex is successfully obtained, pdFalse on timeout.*/
#define RTOS_MUTEX_GET_WAIT_ms(m, ms)       k_mutex_lock((m), K_MSEC((ms))) 

/** @brief Macro to put a mutex. */
#define RTOS_MUTEX_PUT(m)                   k_mutex_unlock((m)) 

/** @todo Convert QUEUE macros. */
#if 0
/** @brief Queues. */
#define RTOS_QUEUE                              QueueHandle_t
/*  Create a queue.
    depth: Depth of the queue.
    size: Size of an item.
    Returns a RTOS_QUEUE object
*/
#define RTOS_QUEUE_CREATE(depth, size)\
    xQueueCreate((depth), (size));
/*  Send to queue, wait forever.
    Returns pdTrue on success, errQUEUE_FULL otherwise.
*/
#define RTOS_QUEUE_SEND(xq, pobj)\
    xQueueSendToBack((xq), (void *)(pobj), portMAX_DELAY)
/*  Send to queue, wait ms.
    Returns pdTRUE on success, errQUEUE_FULL otherwise.
*/
#define RTOS_QUEUE_SEND_WAIT(xq, pobj, ms)\
    xQueueSendToBack((xq), (void *)(pobj), RTOS_MS_TO_TICKS((ms)))
/*  Receive from queue, wait forever. Received item copied into prcvbuf.
    Returns pdTRUE on success, pdFALSE otherwise.
*/
#define RTOS_QUEUE_RECV(xq, prcvbuf)\
    xQueueReceive((xq), (void *)(prcvbuf), portMAX_DELAY)
/*  Receive from queue, wait ms. Received item copied into prcvbuf.
    Returns pdTRUE on success, pdFALSE otherwise.
*/
#define RTOS_QUEUE_RECV_WAIT(xq, prcvbuf, ms)\
    xQueueReceive((xq), (void *)(prcvbuf), RTOS_MS_TO_TICKS((ms)))
#endif
#endif
