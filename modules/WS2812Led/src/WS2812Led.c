/*******************************************************************************
 *  @file: WS2812Led.c
 *  
 *  @brief: Utilities for WS2812 led strip.
*******************************************************************************/
#include <stdlib.h>
#include <string.h>

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/logging/log.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/led_strip.h>

#include "WS2812Led.h"
#include "Random.h"

/** @brief Initialize the logging module. */
LOG_MODULE_REGISTER(WS2812Led, CONFIG_WS2812LED_LOG_LEVEL);

#define GET_RANDOM_HSV()        (CHSV){ RANDOM_U8(), RANDOM_U8(), RANDOM_U8() }
#define GET_RANDOM_HUE(s, v)    (CHSV){ RANDOM_U8(), (s), (v) }
#define GET_RANDOM_VAL(h, s)    (CHSV){ (h), (s), RANDOM_U8() }
#define GET_RANDOM_SAT(h, v)    (CHSV){ (h), RANDOM_U8(), (v) }
#define GET_RANDOM_SATVAL(h)    (CHSV){ (h), RANDOM_U8(), RANDOM_U8() }

/** @brief Computes u[8 0]*u[8 8] -> u[8 0] */
#define SCALE8(x, scale)        ((uint8_t)(((uint16_t)(x) * (scale)) >> 8))

/** @brief Computes u[8 0]*u[8 8] -> u[8 0] but guarantees the output is nonzero
  if both inputs are nonzero (referred to as 'video' dimming) */
#define SCALE8_NZ(x, scale)\
    ((uint8_t)(((uint16_t)(x) * (uint16_t)(scale)) >> 8) + (((x) && (scale)) ? 1 : 0))

/** @brief Perform a - b but clamp result at 0. */
#define SUB_SAFE(a, b)    (((b) > (a)) ? 0 : (a) - (b))

/** @brief Perform a + b, do not allow overflow. */
#define ADD8_SAFE(a, b)\
    (((uint16_t)(a) + (uint16_t)(b) > 255) ? 255 : (a) + (b))

#define BLANK_STRIP(seg)            \
do {                                \
    CHSV off = { 0, 0, 0 };         \
    fill_solid(seg, &off);           \
} while (0);

#define BLANK_STRIP_RGB(seg)        \
do {                                \
    CRGB off = { 0, 0, 0 };         \
    fill_solid_rgb(seg, &off);      \
} while (0);

/** @brief Gamma correction array. */
const uint8_t gammaArray[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255
};

/** @brief Gamma correction lookup. */
static inline uint8_t
gamma8(uint8_t idx)
{
    return gammaArray[idx];
}

/** @brief Scale an RGB value. */
static inline void
scale8_rgb(CRGB *rgb, uint8_t scale)
{
    rgb->r = SCALE8(rgb->r, scale);
    rgb->g = SCALE8(rgb->g, scale);
    rgb->b = SCALE8(rgb->b, scale);
}

#if 0
static uint16_t
random_walk(uint16_t pos, uint16_t limit)
{
    if (RANDOM_BIN())
    {
        return (pos + 1 > limit) ? 0 : pos + 1;
    }
    else
    {
        return (pos == 0) ? limit-1 : pos - 1;
    }
}
#endif

/******************************************************************************
    is_seg_blank_rgb
*//**
    @brief Test that a segment is blank (using rgb_pixels).
    @param[in] seg  Pointer to active segment.
    @return Returns true if the segment is blank, false otherwise.
******************************************************************************/
static bool
is_seg_blank_rgb(WS2812Led_Segment *seg)
{
    CRGB *pix;
    for (int i = 0; i < seg->numPixels; i++)
    {
        pix = &seg->rgb_pixels[i];
        if (pix->r > 0 || pix->g > 0 || pix->b > 0)
        {
            return false;
        }
    }
    return true;
}

/******************************************************************************
    get_heatcolor
*//**
    @brief Approximates a black body radiadion spectrum for a given temperature
  level. Used for fire animations. Ported from FastLED's HeatColor function.
******************************************************************************/
static void
get_heatcolor(uint8_t temperature, CRGB *heatcolor)
{
    uint8_t t192;
    uint8_t heatramp;

    /* Scale temperature from 0-255 to 0-191. This can easily be divided into
        three equal thirds of 64 each. */
    t192 = SCALE8_NZ(temperature, 191);
    //LOG_INF("temp = %u; t192 = %u",
    //    (unsigned int)temperature,
    //    (unsigned int)t192);

    /* Calculate a value that ramps up from 0 to 255 in each third of the scale
    */
    heatramp = t192 & 0x3f;   /* 0..63 */
    heatramp <<= 2;           /* Scale up to 0..252 */

    //LOG_INF("heatramp = %u", (unsigned int)heatramp);

    /* Hottest 1/3 */
    if (t192 & 0x80)
    {
        heatcolor->r = 255;
        heatcolor->g = 255;
        heatcolor->b = heatramp;
    }
    /* Middle 1/3 */
    else if (t192 & 0x40)
    {
        heatcolor->r = 255;
        heatcolor->g = heatramp;
        heatcolor->b = 0;
    }
    /* Coolest 1/3 */
    else
    {
        heatcolor->r = heatramp;
        heatcolor->g = 0;
        heatcolor->b = 0;
    }
}

/******************************************************************************
    hsv2rgb
*//**
    @brief Convert an HSV color to RGB.
******************************************************************************/
static void
hsv2rgb(const CHSV *hsv, CRGB *rgb)
{
    unsigned char region, remainder, p, q, t;
    
    if (hsv->s == 0)
    {
        rgb->r = gamma8(hsv->v);
        rgb->g = gamma8(hsv->v);
        rgb->b = gamma8(hsv->v);
        return;
    }
    
    region = hsv->h / 43;
    remainder = (hsv->h - (region * 43)) * 6; 
    
    p = (hsv->v * (255 - hsv->s)) >> 8;
    q = (hsv->v * (255 - ((hsv->s * remainder) >> 8))) >> 8;
    t = (hsv->v * (255 - ((hsv->s * (255 - remainder)) >> 8))) >> 8;
    
    switch (region)
    {
        case 0:
            rgb->r = hsv->v;
            rgb->g = t;
            rgb->b = p;
            break;
        case 1:
            rgb->r = q;
            rgb->g = hsv->v;
            rgb->b = p;
            break;
        case 2:
            rgb->r = p;
            rgb->g = hsv->v;
            rgb->b = t;
            break;
        case 3:
            rgb->r = p;
            rgb->g = q;
            rgb->b = hsv->v;
            break;
        case 4:
            rgb->r = t;
            rgb->g = p;
            rgb->b = hsv->v;
            break;
        default:
            rgb->r = hsv->v;
            rgb->g = p; 
            rgb->b = q;
            break;
    }

    rgb->r = gamma8(rgb->r);
    rgb->g = gamma8(rgb->g);
    rgb->b = gamma8(rgb->b);
}

/******************************************************************************
    fadeColor
*//**
    @brief Fades CRGB by factor 0-255 (0=no fade, 255=max fade).
    Modifies RGB object in place.

    @param[in] rgb  CRGB color to affect.
    @param[in] factor  Factor controlling amount to fade by.
******************************************************************************/
static void
fadeColor(CRGB *rgb, uint8_t factor)
{
    scale8_rgb(rgb, 255-factor);
}

/******************************************************************************
    fadePixel
*//**
    @brief Fades a pixel by a factor (internal use only).

    @param[in] seg  Pointer to the active segment.
    @param[in] idx  The segment pixel index to fade.
    @param[in] factor  Factor controlling amount to fade by.
******************************************************************************/
static void
fadePixel(WS2812Led_Segment *seg, uint16_t idx, uint8_t factor)
{
    CRGB *pixel_rgb = &seg->rgb_pixels[idx];
    /* Fade the RGB value by the provided factor. */
    fadeColor(pixel_rgb, factor);
}

/******************************************************************************
    get_gradient_iter
*//**
    @brief Gets a gradient iterator.

    @param[in] startColor  The gradient start color.
    @param[in] endColor  The gradient end color.
    @param[in] dir  Gradient direction (usually GRAD_LONGEST).
    @param[in] numSteps  Number of steps to use
    @param[out] gradIter  Returned initialized iterator.
******************************************************************************/
static void
get_gradient_iter(
    CHSV *startColor,
    CHSV *endColor,
    GradientDir dir,
    uint16_t numSteps,
    WS2812Led_GradientIter *gradIter)
{
    /* s[8 7] values for distances. */
    int16_t hueDist_7;
    int16_t satDist_7;
    int16_t valDist_7;

    satDist_7 = (int16_t)(endColor->s - startColor->s) << 7;
    valDist_7 = (int16_t)(endColor->v - startColor->v) << 7;

    uint8_t hueDelta = endColor->h - startColor->h;

    switch (dir)
    {
    case GRAD_SHORTEST:
        dir = GRAD_FWD;
        if (hueDelta > 127)
        {
            dir = GRAD_BWD;
        }
        break;

    case GRAD_LONGEST:
        dir = GRAD_FWD;
        if (hueDelta < 128)
        {
            dir = GRAD_BWD;
        }
        break;

    default:
        break;
    }

    if (dir == GRAD_FWD)
    {
        hueDist_7 = (uint16_t)hueDelta << 7;
    }
    else /* GRAD_BWD */
    {
        hueDist_7 = (uint8_t)(256 - hueDelta) << 7;
        hueDist_7 = -hueDist_7;
    }

    int16_t hueDelta_7 = hueDist_7 / numSteps;
    int16_t satDelta_7 = satDist_7 / numSteps;
    int16_t valDelta_7 = valDist_7 / numSteps;

    gradIter->numSteps = numSteps;
    gradIter->stepIdx = 0;

    /* Adjust to 8-bits of fraction */
    gradIter->hueDelta_8 = hueDelta_7 * 2;
    gradIter->satDelta_8 = satDelta_7 * 2;
    gradIter->valDelta_8 = valDelta_7 * 2;

    gradIter->hueAccum_8 = (uint16_t)startColor->h << 8;
    gradIter->satAccum_8 = (uint16_t)startColor->s << 8;
    gradIter->valAccum_8 = (uint16_t)startColor->v << 8;

    gradIter->hueStart_8 = gradIter->hueAccum_8;
    gradIter->satStart_8 = gradIter->satAccum_8;
    gradIter->valStart_8 = gradIter->valAccum_8;
}

/****************** METHODS ***************************************************/

/******************************************************************************
    fill_solid_rgb
*//**
    @brief Method which fills all pixels with an HSV color.

    @param[in] self  Pointer to self segment.
    @param[in] color  CHSV pointer to color to use.
******************************************************************************/
static void
fill_solid(void *self, const CHSV *color)
{
    WS2812Led_Segment *seg = (WS2812Led_Segment *)self;
    uint16_t i;

    for (i = 0; i < seg->numPixels; i++)
    {
        seg->pixels[i].h = color->h;
        seg->pixels[i].s = color->s;
        seg->pixels[i].v = color->v;
    }
    seg->mode = MODE_STATIC;
}

/******************************************************************************
    fill_solid_rgb
*//**
    @brief Method which fills all pixels with an RBG color.

    @param[in] self  Pointer to self segment.
    @param[in] color  CRGB pointer to color to use.
******************************************************************************/
static void
fill_solid_rgb(void *self, const CRGB *color)
{
    WS2812Led_Segment *seg = (WS2812Led_Segment *)self;
    uint16_t i;

    for (i = 0; i < seg->numPixels; i++)
    {
        seg->rgb_pixels[i].r = color->r;
        seg->rgb_pixels[i].g = color->g;
        seg->rgb_pixels[i].b = color->b;
    }
    seg->mode = MODE_STATIC;
}

/******************************************************************************
    single_random
*//**
    @brief Method which populates a single pixel with an HSV color.

    @param[in] self  Pointer to self segment.
    @param[in] color  CHSB pointer to color to use..
    @param[in] idx  Index of pixel.
******************************************************************************/
static void
single(
    void *self,
    const CHSV *color,
    uint16_t idx)
{
    WS2812Led_Segment *seg = (WS2812Led_Segment *)self;

    if (!(idx >= seg->startIdx && idx <= seg->endIdx))
    {
        LOG_ERR("single: idx out of range. (%u not in %u to %u)",
            idx, seg->startIdx, seg->endIdx);
        return;
    }

    seg->pixels[idx].h = color->h;
    seg->pixels[idx].s = color->s;
    seg->pixels[idx].v = color->v;
    seg->mode = MODE_STATIC;
}

/******************************************************************************
    single_random
*//**
    @brief Method which populates a single pixel with an RGB color.

    @param[in] self  Pointer to self segment.
    @param[in] color  CRBG pointer to color to use..
    @param[in] idx  Index of pixel.
******************************************************************************/
static void
single_rgb(
    void *self,
    const CRGB *color,
    uint16_t idx)
{
    WS2812Led_Segment *seg = (WS2812Led_Segment *)self;

    if (!(idx >= seg->startIdx && idx <= seg->endIdx))
    {
        LOG_ERR("single_rgb: idx out of range. (%u not in %u to %u)",
            idx, seg->startIdx, seg->endIdx);
        return;
    }

    seg->rgb_pixels[idx].r = color->r;
    seg->rgb_pixels[idx].g = color->g;
    seg->rgb_pixels[idx].b = color->b;
    seg->mode = MODE_STATIC;
}

/******************************************************************************
    single_random
*//**
    @brief Method which populates a single pixel with a random color.

    @param[in] self  Pointer to self segment.
    @param[in] sat  HSV saturation.
    @param[in] val  HSV value.
    @param[in] idx  Index of pixel.
******************************************************************************/
static void
single_random(
    void *self,
    uint8_t sat,
    uint8_t val,
    uint16_t idx)
{
    WS2812Led_Segment *seg = (WS2812Led_Segment *)self;

    if (!(idx >= seg->startIdx && idx <= seg->endIdx))
    {
        LOG_ERR("single_random: idx out of range. (%u not in %u to %u)",
            idx, seg->startIdx, seg->endIdx);
        return;
    }

    seg->pixels[idx] = GET_RANDOM_HUE(sat, val);
    seg->mode = MODE_STATIC;
}


/******************************************************************************
    fill_random
*//**
    @brief Method which fills all pixels with random colors.

    @param[in] self  Pointer to self segment.
    @param[in] sat  HSV saturation.
    @param[in] val  HSV value.
******************************************************************************/
static void
fill_random(void *self, uint8_t sat, uint8_t val)
{
    WS2812Led_Segment *seg = (WS2812Led_Segment *)self;
    uint16_t i;

    for (i = 0; i < seg->numPixels; i++)
    {
        seg->pixels[i] = GET_RANDOM_HUE(sat, val);
    }
    seg->mode = MODE_STATIC;
}


/******************************************************************************
    fill_gradient
*//**
    @brief Method which fills all pixels from a gradient.
    (ported from FastLED's fill_gradiant() function.)

    @param[in] self  Pointer to self segment.
    @param[in] startColor  The gradient start color.
    @param[in] endColor  The gradient end color.
    @param[in] dir  Gradient direction (usually GRAD_LONGEST).
******************************************************************************/
static void
fill_gradient(
    void *self,
    CHSV *startColor,
    CHSV *endColor,
    GradientDir dir)
{
    WS2812Led_Segment *seg = (WS2812Led_Segment *)self;
    WS2812Led_GradientIter iter;
    uint16_t numSteps = (seg->endIdx - seg->startIdx) + 1;

    get_gradient_iter(startColor, endColor, dir, numSteps, &iter);

    for (unsigned int i = 0; i < iter.numSteps; i++)
    {
        seg->pixels[i].h = iter.hueAccum_8 >> 8;
        seg->pixels[i].s = iter.satAccum_8 >> 8;
        seg->pixels[i].v = iter.valAccum_8 >> 8;

        iter.hueAccum_8 += iter.hueDelta_8;
        iter.satAccum_8 += iter.satDelta_8;
        iter.valAccum_8 += iter.valDelta_8;

        //CRGB rgb;
        //hsv2rgb(&seg->pixels[i], &rgb);
        //LOG_INF("[%u]: h=%u, s=%u, v=%u (r=%u, g=%u, b=%u)",
        //    i,
        //    (unsigned int)seg->pixels[i].h,
        //    (unsigned int)seg->pixels[i].s,
        //    (unsigned int)seg->pixels[i].v,
        //    (unsigned int)rgb.r,
        //    (unsigned int)rgb.g,
        //    (unsigned int)rgb.b);
    }

    seg->mode = MODE_STATIC;
}

/******************************************************************************
    twinkle
*//**
    @brief Method which provides a twinkling effect. Each pixel is given a
    random color.

    @param[in] self  Pointer to self segment.
    @param[in] init  Flag indicating the first call.
    @param[in] numToLight  Count of number of pixels to light (random
    locations across the segment) until the process is restarted..
    @param[in] delay_ms  Loop delay, ms.
******************************************************************************/
static void
twinkle(
    void *self,
    bool init,
    uint16_t numToLight,
    uint32_t delay_ms)
{
    WS2812Led_Segment *seg = (WS2812Led_Segment *)self;
    static int num;
    static int count = 0;
    uint16_t idx;

    if (init)
    {
        seg->timer_period_ms = delay_ms;
        seg->use_rgb_pixels = false;
        num = numToLight;
        count = 0;
        SwTimer_setMs(&seg->timer, delay_ms);
        BLANK_STRIP(seg);
    }

    if (SwTimer_test(&seg->timer))
    {
        SwTimer_setMs(&seg->timer, seg->timer_period_ms);
        idx = RANDOM_UINT(uint16_t, seg->numPixels);
        seg->pixels[idx] = GET_RANDOM_HSV();
        
        count++;
        if (count == num)
        {
            BLANK_STRIP(seg);
            count = 0;
        }
    }
    seg->mode = MODE_TWINKLE;
}

/******************************************************************************
    twinkle
*//**
    @brief Method which provides a sparkle effect. Each pixel is given the
    color provided at init.

    Example:
        CHSV color = WS2812LED_HSV_COLOR(HUE_AQUA, 255, 240);
        segment0.sparkle(&segment0, true, &color, 1, 10);
        RTOS_TASK_SLEEP_s(10);

    @param[in] self  Pointer to self segment.
    @param[in] init  Flag indicating the first call.
    @param[in] color  HSV color to use.
    @param[in] numToLight  Count of number of pixels to light (random
    locations across the segment) until the process is restarted..
    @param[in] delay_ms  Loop delay, ms.
******************************************************************************/
static void
sparkle(
    void *self,
    bool init,
    CHSV *color,
    uint16_t numToLight,
    uint32_t delay_ms)
{
    WS2812Led_Segment *seg = (WS2812Led_Segment *)self;
    static CHSV *c;
    static int num;
    uint16_t idx;

    if (init)
    {
        seg->timer_period_ms = delay_ms;
        seg->use_rgb_pixels = false;
        c = color;
        num = numToLight;
        SwTimer_setMs(&seg->timer, delay_ms);
        BLANK_STRIP(seg);
    }

    if (SwTimer_test(&seg->timer))
    {
        SwTimer_setMs(&seg->timer, seg->timer_period_ms);
        BLANK_STRIP(seg);

        for (int i = 0; i < num; i++)
        {
            idx = RANDOM_UINT(uint16_t, seg->numPixels);
            if (c)
            {
                seg->pixels[idx] = GET_RANDOM_SATVAL(c->h);
            }
            else
            {
                seg->pixels[idx] = GET_RANDOM_HSV();
            }
        }
    }
    seg->mode = MODE_SPARKLE;
}


/******************************************************************************
    fire
*//**
    @brief Method which provides a fire effect. 

    Example:
        segment0.fire(&segment0, true, 120, 100, 10);
        RTOS_TASK_SLEEP_s(20);

    @param[in] self  Pointer to self segment.
    @param[in] init  Flag indicating the first call.
    @param[in] cooling  Cooling factor (play around with it).
    @param[in] sparking  Sparking factor
    locations across the segment) until the process is restarted..
    @param[in] delay_ms  Loop delay, ms.
******************************************************************************/
static void
fire(
    void *self,
    bool init,
    uint8_t cooling,
    uint8_t sparking,
    uint32_t delay_ms)
{
    WS2812Led_Segment *seg = (WS2812Led_Segment *)self;
    uint8_t *heat = seg->workBuf;
    static uint8_t cool;
    static uint8_t spark;

    if (init)
    {
        seg->timer_period_ms = delay_ms;
        seg->use_rgb_pixels = true;
        cool = cooling;
        spark = sparking;
        SwTimer_setMs(&seg->timer, delay_ms);
        memset(heat, 0, seg->numPixels);
        BLANK_STRIP_RGB(seg);
    }

    if (SwTimer_test(&seg->timer))
    {
        int i;
        uint8_t randu8;
        uint8_t randrange;
        CRGB color;

        SwTimer_setMs(&seg->timer, seg->timer_period_ms);

        /* Cool down every pixel a little */
        for (i = 0; i < seg->numPixels; i++)
        {
            randu8 = RANDOM_UINT(uint8_t, ((cool*10)/seg->numPixels) + 2);
            heat[i] = SUB_SAFE(heat[i], randu8);
            //LOG_INF("randu8 = %u, heat[%u]=%u",
            //    (unsigned int)randu8, i, (unsigned int)heat[i]);
        }

        /* Heat from each cell drifts up and diffuses a little */
        for (i = seg->numPixels-1; i >= 2; i--)
        {
            heat[i] = (heat[i-1] + heat[i-2] + heat[i-2])/3;
        }

        /* Randomly ignite new 'sparks' of heat near the bottom */
        if (RANDOM_U8() < spark)
        {
            /** @todo Change 7 to be a function of numPixels, this will break if
                  segment has less than 8 leds. */
            randu8 = RANDOM_UINT(uint8_t, 7); //!!!!!!!!!!
            randrange = RANDOM_U8_RANGE(160, 255);
            heat[randu8] = ADD8_SAFE(heat[randu8], randrange);
            //LOG_INF("randrange = %u, heat[%u]=%u",
            //    (unsigned int)randrange, (unsigned int)randu8,
            //    (unsigned int)heat[randu8]);
        }
        
        /* Map from heat cells to LED colors. */
        for (i = 0; i < seg->numPixels; i++)
        {
            get_heatcolor(heat[i], &color);
            seg->rgb_pixels[i] = color;
            //LOG_INF("[%u] heat=%u; r=%u, g=%u, b=%u",
            //    i,
            //    (unsigned int)heat[i],
            //    (unsigned int)color.r,
            //    (unsigned int)color.g,
            //    (unsigned int)color.b);
        }
    }
    seg->mode = MODE_FIRE;
}

/******************************************************************************
    dissolve
*//**
    @brief Method which provides a pixel dissolving effect. 

    Example:
        color = WS2812LED_HSV_COLOR(HUE_AQUA, 255, 240);
        segment0.dissolve(&segment0, true, &color, 80, 20, 50);
        RTOS_TASK_SLEEP_s(20);

    @param[in] self  Pointer to self segment.
    @param[in] init  Flag indicating the first call.
    @param[in] color  HSV color to use.
    @param[in] decayFactor  Decaying factor (experiment with this).
    @param[in] decayProb  Probability a pixel will be chosen for decay.
    @param[in] delay_ms  Loop delay, ms.
******************************************************************************/
static void
dissolve(
    void *self,
    bool init,
    CHSV *color,
    uint8_t decayFactor,
    uint8_t decayProb,
    uint32_t delay_ms)
{
    WS2812Led_Segment *seg = (WS2812Led_Segment *)self;
    static CRGB c;
    static uint8_t decay;
    static uint8_t prob;
    int j;

    if (init)
    {
        seg->timer_period_ms = delay_ms;
        seg->use_rgb_pixels = true;
        hsv2rgb(color, &c);
        decay = decayFactor;
        prob = (decayProb > 100) ? 100 : decayProb;
        SwTimer_setMs(&seg->timer, delay_ms);

        BLANK_STRIP_RGB(seg);
        for (j = 0; j < seg->numPixels; j++)
        {
            seg->rgb_pixels[j] = c;
        }
    }

    if (SwTimer_test(&seg->timer))
    {
        SwTimer_setMs(&seg->timer, seg->timer_period_ms);

        /*  Fade all pixels by the decay factor. */
        for (j = 0; j < seg->numPixels; j++)
        {
            if (RANDOM_UINT(uint8_t, 100) > (100 - prob))
            {
                fadePixel(seg, (uint16_t)j, decay);
            }
        }

        /* When entire segment has decayed, restore. */
        if (is_seg_blank_rgb(seg))
        {
            for (j = 0; j < seg->numPixels; j++)
            {
                seg->rgb_pixels[j] = c;
            }
        }
    }
    seg->mode = MODE_DISSOLVE;
}

/******************************************************************************
    meteor
*//**
    @brief Method which provides a shooting start effect. 

    Example:
        color = WS2812LED_HSV_COLOR(HUE_BLUE, 255, 240);
        segment0.meteor(&segment0, true, &color, 1, 80, true, 20);
        RTOS_TASK_SLEEP_s(20);

    @param[in] self  Pointer to self segment.
    @param[in] init  Flag indicating the first call.
    @param[in] color  HSV color to use.
    @param[in] meteorSize  Relative size of the meteor pixel group.
    @param[in] meteorDecay  Decay factor.
    @param[in] decayRandom  Randomize decay factors.
    @param[in] delay_ms  Loop delay, ms.
******************************************************************************/
static void
meteor(
    void *self,
    bool init,
    CHSV *color,
    uint8_t meteorSize,
    uint8_t meteorDecay,
    bool decayRandom,
    uint32_t delay_ms)
{
    WS2812Led_Segment *seg = (WS2812Led_Segment *)self;
    static CRGB c;
    static uint8_t msize;
    static uint8_t mdecay;
    static bool decayrandom;
    static int count;

    if (init)
    {
        seg->timer_period_ms = delay_ms;
        seg->use_rgb_pixels = true;
        hsv2rgb(color, &c);
        msize = meteorSize;
        mdecay = meteorDecay;
        decayrandom = decayRandom;
        count = 0;
        SwTimer_setMs(&seg->timer, delay_ms);
        BLANK_STRIP_RGB(seg);
    }

    if (SwTimer_test(&seg->timer))
    {
        SwTimer_setMs(&seg->timer, seg->timer_period_ms);


        /*  Fade all pixels by the decay factor. If meteorDecay boolean is false,
            this happens smoothly, otherwise some randomness is built into the
            decay.
        */
        for (int j = 0; j < seg->numPixels; j++)
        {
            if ((!decayrandom) || (RANDOM_UINT(uint8_t, 100) > 60))
            {
                fadePixel(seg, (uint16_t)j, mdecay);
            }
        }

        /* Draw the meteor */
        for (int j = 0; j < msize; j++)
        {
            int pixelPos = count - j;
            if (pixelPos < 0) continue;

            if (pixelPos < seg->numPixels)
            {
                seg->rgb_pixels[pixelPos] = c;
            }
        }

        count++;
        if (count == 2*seg->numPixels)
        {
            count = 0;
        }
    }
    seg->mode = MODE_METEOR;
}

/******************************************************************************
    blend
*//**
    @brief Segment method to blend from start color to end color over a set
    number of steps.
    @param[in] self  Pointer to self segment.
    @param[in] init  Flag indicating the first initializing call.
    @param[in] startColor  Starting color (hsv)
    @param[in] endColor  Ending color (hsv)
    @param[in] dir  Gradient direction (GRAD_LONGEST is typical)
    @param[in] numSteps  Number of steps from start to end color.
    @param[in] stepInc_ms  Step period, ms.
******************************************************************************/
static void
blend(
    void *self,
    bool init,
    CHSV *startColor,
    CHSV *endColor,
    GradientDir dir,
    uint16_t numSteps,
    uint16_t stepInc_ms)
{
    WS2812Led_Segment *seg = (WS2812Led_Segment *)self;
    WS2812Led_GradientIter *gradIter = &seg->gradIter;

    if (init)
    {
        seg->timer_period_ms = stepInc_ms;
        seg->use_rgb_pixels = false;
        get_gradient_iter(startColor, endColor, dir, numSteps, gradIter);
        gradIter->initialized = OBJ_INIT_CODE;
        SwTimer_setMs(&seg->timer, seg->timer_period_ms);
        fill_solid(self, startColor);
        seg->mode = MODE_BLEND;
        return;
    }

    if (SwTimer_test(&seg->timer))
    {
        SwTimer_setMs(&seg->timer, seg->timer_period_ms);

        gradIter->hueAccum_8 += gradIter->hueDelta_8;
        gradIter->satAccum_8 += gradIter->satDelta_8;
        gradIter->valAccum_8 += gradIter->valDelta_8;

        for (unsigned int i = 0; i < seg->numPixels; i++)
        {
            seg->pixels[i].h = gradIter->hueAccum_8 >> 8;
            seg->pixels[i].s = gradIter->satAccum_8 >> 8;
            seg->pixels[i].v = gradIter->valAccum_8 >> 8;
        }

        gradIter->stepIdx++;

        if (gradIter->stepIdx == gradIter->numSteps)
        {
            CHSV hsv = { .h = gradIter->hueStart_8 >> 8,
                         .s = gradIter->satStart_8 >> 8,
                         .v = gradIter->valStart_8 >> 8 };
            fill_solid(self, &hsv);
            seg->mode = MODE_BLEND;
            gradIter->hueAccum_8 = gradIter->hueStart_8;
            gradIter->satAccum_8 = gradIter->satStart_8;
            gradIter->valAccum_8 = gradIter->valStart_8;
            gradIter->stepIdx = 0;
        }
    }
}

/******************************************************************************
    fill_rainbow
*//**
    @brief Segment method to fill across all pixels from all colors.

    @param[in] self  Pointer to self segment.
    @param[in] initialHue  Starting color for pixel 0.
    @param[in] sat  HSV saturation
    @param[in] val  HSV value
******************************************************************************/
static void
fill_rainbow(
    void *self,
    uint8_t initialHue,
    uint8_t sat,
    uint8_t val)
{
    CHSV start = { .h = initialHue, .s = sat, .v = val };
    CHSV end = { .h = initialHue + 255, .s = sat, .v = val };
    fill_gradient(self, &start, &end, GRAD_LONGEST);
}

/******************************************************************************
    fill_rainbow
*//**
    @brief Segment method to blink all pixels at a certain period.

    @param[in] self  Pointer to self segment.
    @param[in] period_ms  Blink period (on time will be period/2).
******************************************************************************/
static void
blink(
    void *self,
    uint32_t period_ms)
{
    WS2812Led_Segment *seg = (WS2812Led_Segment *)self;
    seg->timer_period_ms = period_ms;

    if (seg->timer.state == STATE_RUNNING)
    {
        if (SwTimer_test(&seg->timer))
        {
            seg->state = (seg->state == SEG_ON) ? SEG_OFF : SEG_ON;
            SwTimer_setMs(&seg->timer, seg->timer_period_ms/2);
        }
    }
    else
    {
        SwTimer_setMs(&seg->timer, period_ms);
        
    }
    seg->mode = MODE_BLINK;
}

/******************************************************************************
    show
*//**
    @brief Segment method to show all pixels.

    @param[in] self  Pointer to self segment.
******************************************************************************/
static void
show(void *self)
{
    WS2812Led_Segment *seg = (WS2812Led_Segment *)self;
    seg->state = SEG_ON;
}

/******************************************************************************
    hide
*//**
    @brief Segment method to hide all pixels (retaining information).

    @param[in] self  Pointer to self segment.
******************************************************************************/
static void
hide(void *self)
{
    WS2812Led_Segment *seg = (WS2812Led_Segment *)self;
    seg->state = SEG_OFF;
}

/******************************************************************************
    off
*//**
    @brief Segment method to blank all pixels.

    @param[in] self  Pointer to self segment.
******************************************************************************/
static void
off(void *self)
{
    WS2812Led_Segment *seg = (WS2812Led_Segment *)self;
    CHSV blank = { .h = 0, .s = 0, .v = 0 };
    fill_solid(seg, &blank);
}

/******************************************************************************
    segment_loop
*//**
    @brief Main task loop for segment effects.
******************************************************************************/
static void
segment_loop(void *p0, void *p1, void *p2)
{
    WS2812Led_Segment *self = (WS2812Led_Segment *)p0;

    (void)p1;
    (void)p2;

    while (1)
    {
        switch (self->mode)
        {
        case MODE_STATIC:
            break;

        case MODE_BLINK:
            blink(self, self->timer_period_ms);
            break;

        case MODE_BLEND:
            blend(self, false, NULL, NULL, 0, 0, 0);
            break;

        case MODE_TWINKLE:
            twinkle(self, false, 0, 0);
            break;

        case MODE_SPARKLE:
            sparkle(self, false, NULL, 0, 0);
            break;

        case MODE_METEOR:
            meteor(self, false, NULL, 0, 0, false, 0);
            break;

        case MODE_DISSOLVE:
            dissolve(self, false, NULL, 0, 0, 0);
            break;

        case MODE_FIRE:
            fire(self, false, 0, 0, 0);
            break;

        default:
            break;
        }

        RTOS_TASK_SLEEP_ms(self->loopDelay_ms);
    }
}

/******************************************************************************
    led_main
*//**
    @brief Main task loop for writing to the led strip.
******************************************************************************/
static void
led_main(void *p0, void *p1, void *p2)
{
    int ret;
    WS2812Led_Strip *strip = (WS2812Led_Strip *)p0;
    CRGB *leds;
    unsigned int led_array_size = strip->numPixels * sizeof(CRGB);

    (void)p1;
    (void)p2;

    /* Initialize the segment list. */
    CList_init(&strip->segments);

    /* Give semaphore indicating that the segment list has been initialized. */
    RTOS_SEM_GIVE(&strip->initialized);

    leds = (CRGB *)malloc(led_array_size);
    if (!leds)
    {
        LOG_ERR("Error allocated memory for led strip.");
        return;
    }

    memset(leds, 0, led_array_size);

    while (1)
    {
        WS2812Led_Segment *segment;
        CHSV *hsv;
        CRGB *rgb;
        CRGB rgbColor;
        int k;
        LOG_DBG("Hello from LED strip %s", strip->taskName);

        /* Iterate through strip segments. */
        CLIST_ITER_ENTRY(segment, &strip->segments)
        {
            for (k = 0; k < segment->numPixels; k++)
            {
                if (segment->state == SEG_OFF)
                {
                    rgbColor.r = 0;
                    rgbColor.g = 0;
                    rgbColor.b = 0;
                    leds[k + segment->startIdx] = rgbColor;
                    continue;
                }

                hsv = segment->pixels + k;
                rgb = segment->rgb_pixels + k;

                if (segment->use_rgb_pixels)
                {
                    leds[k + segment->startIdx] = *rgb;
                }
                else
                {
                    /* Convert pixel HSV to RGB. */
                    hsv2rgb(hsv, &rgbColor);
                    leds[k + segment->startIdx] = rgbColor;
                }

                LOG_DBG("[%u]: h=%u s=%u v=%u --> r=%u g=%u b=%u",
                    k,
                    (unsigned int)hsv->h,
                    (unsigned int)hsv->s,
                    (unsigned int)hsv->v,
                    (unsigned int)rgbColor.r,
                    (unsigned int)rgbColor.g,
                    (unsigned int)rgbColor.b);
            }
        }

        /* Write updated leds to the strip. */
        ret = led_strip_update_rgb(strip->dev, leds, strip->numPixels);
        if (ret != 0)
        {
            LOG_ERR("Error in led_strip_update_rgb: %d", ret);
            RTOS_TASK_SLEEP_ms(1000);
            continue;
        }

        RTOS_TASK_SLEEP_ms(strip->loopDelay_ms);
    }
}

/******************************************************************************
    [docimport WS2812Led_single_update]
*//**
    @brief Update a single LED with a HSV value.
    @param[in] dev  Pointer to device instance.
******************************************************************************/
int
WS2812Led_single_update(const struct device *dev, const CHSV *hsv)
{
    CRGB rgbColor;
    int ret;

    /* Convert pixel HSV to RGB. */
    hsv2rgb(hsv, &rgbColor);

    /* Write updated led. */
    ret = led_strip_update_rgb(dev, &rgbColor, 1);
    if (ret != 0)
    {
        LOG_ERR("Error in led_strip_update_rgb: %d", ret);
        return ret;
    }
    
    return 0;
}

/******************************************************************************
    [docimport WS2812Led_single_off]
*//**
    @brief Turn single LED off.
    @param[in] dev  Pointer to device instance.
******************************************************************************/
void
WS2812Led_single_off(const struct device *dev)
{
    CHSV color = WS2812LED_HSV_COLOR_OFF;
    WS2812Led_single_update(dev, &color);
}

/******************************************************************************
    [docimport WS2812Led_addSegment]
*//**
    @brief Adds a segment to a WS2812Led_Strip object.
******************************************************************************/
int
WS2812Led_addSegment(WS2812Led_Strip *strip, WS2812Led_Segment *segment)
{
    int ret;
    CList *list = &strip->segments;
    WS2812Led_Segment *last_segment_added;
    uint16_t numPixels = (segment->endIdx - segment->startIdx) + 1;

    LOG_INF("Adding LED segment:");
    LOG_INF("    start: %u", segment->startIdx);
    LOG_INF("    end: %u", segment->endIdx);

    /* Allocate memory for the pixel array. */
    segment->pixels = (CHSV *)malloc(numPixels*sizeof(CHSV));
    if (!segment->pixels)
    {
        LOG_ERR("Error allocating memory for segment.");
        return -ENOMEM;
    }

    segment->rgb_pixels = (CRGB *)malloc(numPixels*sizeof(CRGB));
    if (!segment->rgb_pixels)
    {
        LOG_ERR("Error allocating memory for segment.");
        return -ENOMEM;
    }

    segment->workBuf = (uint8_t *)malloc(numPixels);
    if (!segment->workBuf)
    {
        LOG_ERR("Error allocating memory for segment.");
        return -ENOMEM;
    }

    /* Default to static mode. */
    segment->mode = MODE_STATIC;

    segment->numPixels      = numPixels;
    segment->use_rgb_pixels = false;

    /* Indicate the object is not initialized. */
    segment->gradIter.initialized = 0;

    segment->off            = off;
    segment->show           = show;
    segment->hide           = hide;
    segment->single         = single;
    segment->single_rgb     = single_rgb;
    segment->single_random  = single_random;
    segment->fill_solid     = fill_solid;
    segment->fill_solid_rgb = fill_solid_rgb;
    segment->fill_random    = fill_random;
    segment->fill_rainbow   = fill_rainbow;
    segment->fill_gradient  = fill_gradient;
    segment->blink          = blink;
    segment->blend          = blend;
    segment->twinkle        = twinkle;
    segment->sparkle        = sparkle;
    segment->meteor         = meteor;
    segment->dissolve       = dissolve;
    segment->fire           = fire;

    /*  Wait here to make sure that the led strip has been initialized prior to
        adding the segment to the list. */
    LOG_INF("Waiting for strip to be initialized.");
    RTOS_SEM_TAKE(&strip->initialized);
    LOG_INF("Strip is initialized.");

    if (CList_isEmpty(list))
    {
        segment->number = 0;
    }
    else
    {
        last_segment_added = (WS2812Led_Segment *)CList_tail(list);
        if (!last_segment_added)
        {
            LOG_ERR("Error getting last segment.");
            return -EINVAL;
        }
        segment->number = last_segment_added->number + 1;
    }

    /* Add the segment to the segments list. */
    CList_append(list, segment);
    strip->numSegments++;

    ret = RTOS_TASK_CREATE_DYNAMIC(
        &segment->taskHandle,
        segment_loop,
        segment->taskName,
        segment->taskStack,
        segment->taskStackSize,
        (void *)segment,
        segment->taskPrio);
    if (ret != 0)
    {
        LOG_ERR("Failed creating LED segment task (%d)", ret);
        return ret;
    }

    return 0;
}

/******************************************************************************
    [docimport WS2812_show_all]
*//**
    @brief Shows all segments added to strip.
******************************************************************************/
void
WS2812Led_show_all(WS2812Led_Strip *strip)
{
    CList *iter;
    WS2812Led_Segment *segment;

    /* Iterate through strip segments. */
    CLIST_ITER(iter, &strip->segments)
    {
        segment = (WS2812Led_Segment *)iter;
        segment->show(segment);
    }
}

/******************************************************************************
    [docimport WS2812Led_init_strip]
*//**
    @brief Initializes an LED strip which will have multiple segments.
    Add additional segments with WS2812Led_addSegment.

    @param[in] dev  Pointer to device instance.
    @param[in] strip  Pointer to the LED strip object to initialize.
******************************************************************************/
int
WS2812Led_init_strip( const struct device *dev, WS2812Led_Strip *strip)
{
    int ret;

    strip->dev = dev;

    if (!device_is_ready(dev))
    {
        LOG_ERR("LED strip device is not ready");
        return -ENODEV;
    }

    RTOS_SEM_INIT(&strip->initialized);

    LOG_INF("Creating Led strip task.");
    ret = RTOS_TASK_CREATE_DYNAMIC(
        &strip->taskHandle,
        led_main,
        strip->taskName,
        strip->taskStack,
        strip->taskStackSize,
        (void *)strip,
        strip->taskPrio);
    if (ret != 0)
    {
        LOG_ERR("Failed creating LED strip task (%d)", ret);
        return ret;
    }

    return 0;
}

/******************************************************************************
    [docimport WS2812Led_init]
*//**
    @brief Initializes an LED strip used as a single segment.
    This is the most typical use-case.

    @param[in] dev  Pointer to device instance.
    @param[in] led  Pointer to a WS2812Led object.
    @param[in] name  Name for the strip.
    @param[in] numPixels  Number of pixels in the led strip.
    @param[in] taskStackSize  Stack size for the main strip loop (try 1024).
    @param[in] taskLoop_ms  Task loop delay, ms (try 50).
    @param[in] taskPrio  Main task priority (try 15).
    @param[in] segStackSize  Stack size for the segment effects loop (try 512).
    @param[in] segLoop_ms  Loop delay, ms for the segment effects loop (try 50).
    @param[in] segPrio  Segment task priority (try 15).
    @return 0 on success, negative error code on failure.
******************************************************************************/
int
WS2812Led_init(
    const struct device *dev,
    WS2812Led *led,
    const char *name,
    uint16_t numPixels,
    uint32_t taskStackSize,
    uint32_t taskLoop_ms,
    uint32_t taskPrio,
    uint32_t segStackSize,
    uint32_t segLoop_ms,
    uint32_t segPrio)
{
    WS2812Led_Strip *strip = &led->strip;
    WS2812Led_Segment *seg = &led->seg;
    int ret;

    strip->dev = dev;
    strip->numPixels = numPixels;
    strip->loopDelay_ms = taskLoop_ms;
    strip->taskStackSize = taskStackSize;
    strip->taskPrio = taskPrio;
    strncpy(strip->taskName, name, sizeof(strip->taskName));
    strncat(strip->taskName, "_strip", sizeof(strip->taskName));

    seg->startIdx = 0;
    seg->endIdx = numPixels - 1;
    seg->taskStackSize = segStackSize;
    seg->taskPrio = segPrio;
    strncpy(seg->taskName, name, sizeof(seg->taskName));
    strncat(seg->taskName, "_seg", sizeof(seg->taskName));

    LOG_INF("seg->startIdx = %u", seg->startIdx);

    if (!device_is_ready(dev))
    {
        LOG_ERR("LED strip device is not ready");
        return -ENODEV;
    }

    RTOS_SEM_INIT(&strip->initialized);

    LOG_INF("Creating Led task.");
    ret = RTOS_TASK_CREATE_DYNAMIC(
        &strip->taskHandle,
        led_main,
        strip->taskName,
        strip->taskStack,
        strip->taskStackSize,
        (void *)strip,
        strip->taskPrio);
    if (ret != 0)
    {
        LOG_ERR("Failed creating LED strip task (%d)", ret);
        return ret;
    }

    /* Add the built-in segment. */
    if ((ret = WS2812Led_addSegment(strip, &led->seg)) < 0)
    {
        LOG_ERR("Error adding led segment: %d", ret);
        return ret;
    }

    return 0;
}
