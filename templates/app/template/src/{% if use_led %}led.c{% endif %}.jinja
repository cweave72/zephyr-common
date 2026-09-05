/*******************************************************************************
 *  @file: led.c
 *
 *  @brief: Onboard LED via the devicetree "led0" alias. See led.h.
 *
 *  Follows applications/blinky, which is the reference for LED handling in this
 *  workspace: DT_ALIAS(led0) -> GPIO_DT_SPEC_GET -> gpio_is_ready_dt ->
 *  gpio_pin_configure_dt.
 *
 *  Guarded on DT_NODE_EXISTS because not every board defines led0. Some do in
 *  their own devicetree (esp32s3_qtpy, w55rp20_evb_pico); for others the alias
 *  is supplied by this app's boards/<board>.overlay. Without the guard, a build
 *  for a board with neither fails inside GPIO_DT_SPEC_GET with an error that
 *  does not mention the LED at all.
*******************************************************************************/
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/devicetree.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "led.h"

LOG_MODULE_DECLARE(app, CONFIG_APP_LOG_LEVEL);

#define LED0_NODE DT_ALIAS(led0)

#if DT_NODE_EXISTS(LED0_NODE)

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(LED0_NODE, gpios);
static bool led_state;

/******************************************************************************
    [docimport app_led_init]
*//**
    @brief Configures the led0 GPIO as an output.
    @return Returns 0 on success or when no led0 alias exists, negative on error.
******************************************************************************/
int
app_led_init(void)
{
    int ret;

    if (!gpio_is_ready_dt(&led))
    {
        LOG_ERR("LED GPIO device is not ready.");
        return -ENODEV;
    }

    ret = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (ret != 0)
    {
        LOG_ERR("gpio_pin_configure_dt returned %d", ret);
        return ret;
    }

    led_state = false;
    LOG_INF("LED initialized on %s pin %u.", led.port->name, led.pin);
    return 0;
}

/******************************************************************************
    [docimport app_led_set]
*//**
    @brief Drives the LED.
    @param[in] on  true to light the LED.
******************************************************************************/
void
app_led_set(bool on)
{
    led_state = on;
    gpio_pin_set_dt(&led, on ? 1 : 0);
}

/******************************************************************************
    [docimport app_led_toggle]
*//**
    @brief Inverts the LED state.
******************************************************************************/
void
app_led_toggle(void)
{
    app_led_set(!led_state);
}

#else  /* no led0 alias on this board */

int
app_led_init(void)
{
    LOG_WRN("No 'led0' alias in the devicetree; LED support is inactive. "
            "Add one in boards/<board>.overlay to enable it.");
    return 0;
}

void app_led_set(bool on) { ARG_UNUSED(on); }
void app_led_toggle(void) { }

#endif /* DT_NODE_EXISTS(LED0_NODE) */
