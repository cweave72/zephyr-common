/*******************************************************************************
 *  @file: SwTimer.h
 *   
 *  @brief: Header for SwTimer
*******************************************************************************/
#ifndef SWTIMER_H
#define SWTIMER_H

#include <stdint.h>
#include <stdbool.h>
#include <zephyr/kernel.h>

typedef enum SwTimerType
{
    SWTIMER_TYPE_ONE_SHOT = 0,
    SWTIMER_TYPE_PERIODIC
} SwTimerType;

typedef enum SwTimerState
{
    STATE_IDLE = 0,
    STATE_RUNNING,
    STATE_EXPIRED
    
} SwTimerState;

typedef struct SwTimer
{
    /** @brief Internal state variables. */
    SwTimerState state;
    uint32_t capture;
    uint32_t delay_us;
    const char *name;
    void (*expire_cb)(struct k_timer *t);
    void (*stop_cb)(struct k_timer *t);
    SwTimerType type;
    /* internal */
    struct k_timer timer;
} SwTimer;

/** @brief Millisecond version of functions. */
#define SwTimer_setMs(swt, ms)      SwTimer_setUs((swt), (ms)*1000)
#define SwTimer_sleepMs(ms)         SwTimer_sleepUs((ms)*1000)

#define SwTimer_start_s(swt, type, s) SwTimer_start_ms((swt), (type), (s)*1000)

/******************************************************************************
    [docexport SwTimer_getCount]
*//**
    @brief Gets the current timer value.
    @return Returns the current count
******************************************************************************/
uint32_t
SwTimer_getCount(void);

/******************************************************************************
    [docexport SwTimer_tic]
*//**
    @brief Start a elapsed time measurement.
    @param[in] swt  Pointer to a SwTimer object.
    @return Returns the current count.
******************************************************************************/
uint32_t
SwTimer_tic(SwTimer *swt);

/******************************************************************************
    [docexport SwTimer_toc]
*//**
    @brief Finishes a elapsed time measurement.
    @param[in] swt  Pointer to a SwTimer object.
    @return Returns the delta, in microseconds.
******************************************************************************/
uint32_t
SwTimer_toc(SwTimer *swt);

/******************************************************************************
    [docexport SwTimer_setUs]
*//**
    @brief Initializes a timer test duration, us.
    @param[in] swt  Pointer to a SwTimer object.
    @param[in] us  Microseconds.
******************************************************************************/
void
SwTimer_setUs(SwTimer *swt, uint64_t us);

/******************************************************************************
    [docexport SwTimer_sleepUs]
*//**
    @brief Sleeps for provided us.
    @param[in] us  Microseconds.
******************************************************************************/
void
SwTimer_sleepUs(uint64_t us);

/******************************************************************************
    [docexport SwTimer_test]
*//**
    @brief Tests a SwTimer object for elapsed time.
    Call SwTimer_setUs first.
    @param[in] swt  Pointer to a SwTimer object.
    @return Returns true if the timer has elapsed.
******************************************************************************/
bool
SwTimer_test(SwTimer *swt);

/******************************************************************************
    [docexport SwTimer_create]
*//**
    @brief Creates a timer with callback.
    @param[in] swt  Pointer to initialized SwTimer object.
******************************************************************************/
void
SwTimer_create(SwTimer *swt);

/******************************************************************************
    [docexport SwTimer_start]
*//**
    @brief Starts a previously created SwTimer object.
    @param[in] swt  Pointer to initialized SwTimer object.
    @param[in] duration_ms  Duration in ms.
******************************************************************************/
void
SwTimer_start_ms(SwTimer *swt, uint32_t duration_ms);

/******************************************************************************
    [docexport SwTimer_stop]
*//**
    @brief Stops a previously started SwTimer object.
******************************************************************************/
void
SwTimer_stop(SwTimer *swt);
#endif
