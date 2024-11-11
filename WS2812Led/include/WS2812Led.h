/*******************************************************************************
 *  @file: WS2812Led.h
 *   
 *  @brief: Header for WS2812 component.
*******************************************************************************/
#ifndef WS2812LED_H
#define WS2812LED_H

#include <stdint.h>
#include <zephyr/drivers/led_strip.h>
#include "CList.h"
#include "SwTimer.h"
#include "RtosUtils.h"

/** @brief RGB Color type.
*/
typedef struct led_rgb CRGB;

/** @brief HSV Color type.
*/
typedef struct CHSV
{
    uint8_t h;
    uint8_t s;
    uint8_t v;
} CHSV;

/** @brief Predefined Hue colors for CHSV objects. */
typedef enum {
    HUE_RED = 0,
    HUE_ORANGE = 32,
    HUE_YELLOW = 64,
    HUE_GREEN = 96,
    HUE_AQUA = 128,
    HUE_BLUE = 160,
    HUE_PURPLE = 192,
    HUE_PINK = 224
} HSVHue;

enum State {
    SEG_OFF = 0,
    SEG_ON,
};

typedef enum {
    GRAD_FWD = 0,
    GRAD_BWD,
    GRAD_SHORTEST,
    GRAD_LONGEST
} GradientDir;

#define OBJ_INIT_CODE           0x12580976
#define IS_INITIALIZED(obj)    ((obj)->initialized == OBJ_INIT_CODE)

#define WS2812LED_COLOR(hue, sat, val)   (CHSV){ (hue), (sat), (val) }

/** @brief State object for iterating from one color to another is a gradient
      manner.
*/
typedef struct WS2812Led_GradientIter
{
    uint32_t initialized;
    /** @brief Number of steps and current step index. */
    uint16_t numSteps;
    uint16_t stepIdx;
    /** @brief Starting values for h,s,v as u[8 8]. */
    uint16_t hueStart_8;
    uint16_t satStart_8;
    uint16_t valStart_8;
    /** @brief Current accumulator values for h,s,v as u[8 8]. */
    uint16_t hueAccum_8;
    uint16_t satAccum_8;
    uint16_t valAccum_8;
    /** @brief Delta amount per step. */
    uint16_t hueDelta_8;
    uint16_t satDelta_8;
    uint16_t valDelta_8;
    
} WS2812Led_GradientIter;

enum Mode {
    MODE_STATIC = 0,
    MODE_BLINK,
    MODE_BLEND,
    MODE_TWINKLE,
    MODE_SPARKLE,
    MODE_METEOR,
    MODE_DISSOLVE,
    MODE_FIRE,
};

/** @brief Defines an segment of an LED strip.
*/
typedef struct WS2812Led_Segment
{
    /** @brief List anchor so that this object can be added to a list. MUST BE
          FIRST MEMBER
      */
    CLIST_ANCHOR();

    /** @brief Number assigned to segment. */
    uint16_t number;
    /** @brief Start and end pixel indicies */
    uint16_t startIdx;
    uint16_t endIdx;
    /** @brief Total number of pixels. */
    uint16_t numPixels;
    /** @brief Flag indicating that pixels are stored as RGB. */
    bool use_rgb_pixels;
    /** @brief Pixel storage, in HSV. */
    CHSV *pixels;
    /** @brief Pixel storage, in RGB. */
    CRGB *rgb_pixels;
    /** @brief Timer object. */
    SwTimer timer;
    /** @brief Timer period, ms. */
    uint32_t timer_period_ms;
    /** @brief State */
    enum State state;
    /** @brief Segment effect mode. */
    enum Mode mode;
    /** @brief task loop delay (ms) */
    uint32_t loopDelay_ms;
    /** @brief A work buffer of bytes of length numPixels. */
    uint8_t *workBuf;

    /** @brief Task stack size. */
    uint16_t taskStackSize;
    /** @brief Task name. */
    char taskName[16];
    /** @brief Task prio. */
    uint8_t taskPrio;
    /** @brief Loop task handle. */
    RTOS_TASK taskHandle;
    /** @brief Task stack pointer. */
    RTOS_TASK_STACK *taskStack;

    /** @brief Gradient iterator state object. */
    WS2812Led_GradientIter gradIter;

    /** @brief Methods */
    void (*fill_solid)(void *self, const CHSV *color);
    void (*fill_random)(void *self, uint8_t sat, uint8_t val);
    void (*fill_rainbow)(void *self, uint8_t initialHue, uint8_t sat, uint8_t val);
    void (*fill_gradient)(void *self, CHSV *startColor, CHSV *endColor, GradientDir dir);
    void (*twinkle)(void *self, bool init, uint16_t num, uint32_t delay_ms);
    void (*sparkle)(void *self, bool init, CHSV *color, uint16_t num, uint32_t delay_ms);
    void (*meteor)(void *self, bool init, CHSV *color, uint8_t size,
        uint8_t decay, bool rand, uint32_t delay_ms);
    void (*dissolve)(void *self, bool init, CHSV *color, uint8_t decayFactor,
        uint8_t decayProb, uint32_t delay_ms);
    void (*fire)(void *self, bool init, uint8_t cooling, uint8_t sparking, uint32_t delay_ms);
    void (*blend)(void *self, bool init, CHSV *startColor, CHSV *endColor, GradientDir dir, uint16_t numSteps, uint16_t ms);
    void (*blink)(void *self, uint32_t period_ms);
    void (*show)(void *self);
    void (*off)(void *self);
    
} WS2812Led_Segment;

/** @brief Defines an LED strip.
*/
typedef struct WS2812Led_Strip
{
    /** @brief Reference to led strip device. */
    const struct device *dev;

    /** @brief Number of pixels in strip. */
    uint16_t numPixels;

    /** @brief Main thread loop delay (ms) */
    uint32_t loopDelay_ms;

    /** @brief RMT resolution. */
    int rmt_resolution_hz;

    /** @brief Task stack size. */
    uint16_t taskStackSize;
    /** @brief Task name. */
    char taskName[16];
    /** @brief Task prio. */
    uint8_t taskPrio;

    /** @brief Flag group */
    RTOS_FLAGS eventFlagGrp;

    /* Internal Members. */
    /** @brief Loop task handle. */
    RTOS_TASK taskHandle;
    /** @brief Task stack pointer. */
    RTOS_TASK_STACK *taskStack;
    /** @brief List of segments. */
    CList segments;
    /** @brief Number of segments. */
    uint16_t numSegments;

} WS2812Led_Strip;

/******************************************************************************
    [docexport WS2812Led_addSegment]
*//**
    @brief Adds a segment to a WS2812Led_Strip object.
******************************************************************************/
int
WS2812Led_addSegment(WS2812Led_Strip *strip, WS2812Led_Segment *segment);

/******************************************************************************
    [docexport WS2812_show]
*//**
    @brief Shows all segments.
******************************************************************************/
void
WS2812Led_show(WS2812Led_Strip *strip);

/******************************************************************************
    [docexport WS2812Led_init]
*//**
    @brief Initializes an LED strip.

    @param[in] dev  Pointer to device instance.
    @param[in] strip  Pointer to the LED strip object to initialize.
    @param[in] core  Which core to pin the led task to. This is critical if the
    system is also using WiFi which is known to cause random flicker in LEDs due
    to the wifi interrupt handler interrupting the RMT peripheral transmit.
    This can be mitigated by pinning the created led task associated with this
    strip on the other core. Usually wifi uses core 0, so set this to 1 to cause
    the task (and all underlying RMT functionality) to use the other core.
******************************************************************************/
int
WS2812Led_init(const struct device *dev, WS2812Led_Strip *strip, uint8_t core);
#endif
