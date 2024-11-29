#include <stdio.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include "SwTimer.h"

LOG_MODULE_REGISTER(SwTimer, LOG_LEVEL_DBG);


/** @brief Macro which calculates the circular delta from a reference count to
      the current count */
#define DELTA(cnt, ref) \
    (((cnt) > (ref)) ? (cnt) - (ref) : (0xffffffff - (ref)) + (cnt) + 1)

#define RETURN_ON_ERR(ret, msg)                            \
do {                                                       \
    if ((ret) != ESP_OK) {                                 \
        LOG_ERR("%s : 0x%08x", msg, ret);                  \
        return -1;                                         \
    }                                                      \
} while (0)

#define CYCLES_PER_US   (sys_clock_hw_cycles_per_sec() / 1000000)

/******************************************************************************
    [docimport SwTimer_getCount]
*//**
    @brief Gets the current timer value.
    @return Returns the current count
******************************************************************************/
uint32_t
SwTimer_getCount(void)
{
    return k_cycle_get_32();
}

/******************************************************************************
    [docimport SwTimer_tic]
*//**
    @brief Start a elapsed time measurement.
    @param[in] swt  Pointer to a SwTimer object.
    @return Returns the current count.
******************************************************************************/
uint32_t
SwTimer_tic(SwTimer *swt)
{
    swt->capture = k_cycle_get_32();
    return swt->capture;
}

/******************************************************************************
    [docimport SwTimer_toc]
*//**
    @brief Finishes a elapsed time measurement.
    @param[in] swt  Pointer to a SwTimer object.
    @return Returns the delta, in microseconds.
******************************************************************************/
uint32_t
SwTimer_toc(SwTimer *swt)
{
    uint32_t now = k_cycle_get_32();
    uint32_t delta = DELTA(now, swt->capture);
    return delta / CYCLES_PER_US;
}

/******************************************************************************
    [docimport SwTimer_setUs]
*//**
    @brief Initializes a timer test duration, us.
    @param[in] swt  Pointer to a SwTimer object.
    @param[in] us  Microseconds.
******************************************************************************/
void
SwTimer_setUs(SwTimer *swt, uint64_t us)
{
    swt->capture = k_cycle_get_32();
    swt->delay_us = us * CYCLES_PER_US;
    swt->state = STATE_RUNNING;
}

/******************************************************************************
    [docimport SwTimer_sleepUs]
*//**
    @brief Sleeps for provided us.
    @param[in] us  Microseconds.
******************************************************************************/
void
SwTimer_sleepUs(uint64_t us)
{
    SwTimer t;
    SwTimer_setUs(&t, us);
    do { } while (!SwTimer_test(&t));
}

/******************************************************************************
    [docimport SwTimer_test]
*//**
    @brief Tests a SwTimer object for elapsed time.
    Call SwTimer_setUs first.
    @param[in] swt  Pointer to a SwTimer object.
    @return Returns true if the timer has elapsed.
******************************************************************************/
bool
SwTimer_test(SwTimer *swt)
{
    uint32_t now = k_cycle_get_32();

    if (swt->state == STATE_EXPIRED)
    {
        return true;
    }
    else if (swt->state != STATE_RUNNING)
    {
        LOG_ERR("SwTimer not running, call SwTimer_setUs first.\r\n");
        return true;
    }

    if (DELTA(now, swt->capture) >= swt->delay_us)
    {
        swt->state = STATE_EXPIRED;
        return true;
    }

    return false;
}

/******************************************************************************
    [docimport SwTimer_create]
*//**
    @brief Creates a timer with callback.
    @param[in] swt  Pointer to initialized SwTimer object.
******************************************************************************/
void
SwTimer_create(SwTimer *swt)
{
    k_timer_init(&swt->timer, swt->expire_cb, swt->stop_cb);
}

/******************************************************************************
    [docimport SwTimer_start]
*//**
    @brief Starts a previously created SwTimer object.
    @param[in] swt  Pointer to initialized SwTimer object.
    @param[in] duration_ms  Duration in ms.
******************************************************************************/
void
SwTimer_start_ms(SwTimer *swt, uint32_t duration_ms)
{
    k_timeout_t timeout = K_MSEC(duration_ms);
    k_timeout_t period = (swt->type == SWTIMER_TYPE_ONE_SHOT) ? K_NO_WAIT : 
        timeout;
    k_timer_start(&swt->timer, timeout, period);
}

/******************************************************************************
    [docimport SwTimer_stop]
*//**
    @brief Stops a previously started SwTimer object.
******************************************************************************/
void
SwTimer_stop(SwTimer *swt)
{
    k_timer_stop(&swt->timer);
}
