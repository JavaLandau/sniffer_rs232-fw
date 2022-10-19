/**
\file
\author JavaLandau
\copyright MIT License
\brief Application layer of RGB LED

The file includes implementation of application layer of RGB LED
*/

#include "common.h"
#include "app_led.h"
#include "bsp_led_rgb.h"

/** 
 * \defgroup app_led Application layer of RGB LED
 * \brief Module of application layer of RGB LED
 * \ingroup application
 * @{
*/

/// Settings for disabled LED
static const struct bsp_led_rgb led_disabled = {.r = 0, .g = 0, .b = 0};

/// Settings for LED with RED color
static const struct bsp_led_rgb led_red = {.r = 255, .g = 0, .b = 0};

/// Settings for LED with GREEN color
static const struct bsp_led_rgb led_green = {.r = 0, .g = 255, .b = 0};

/// Settings for LED with YELLOW color
static const struct bsp_led_rgb led_yellow = {.r = 255, .g = 255, .b = 0};

/// Settings for LED with MAGENTA color
static const struct bsp_led_rgb led_magenta = {.r = 100, .g = 0, .b = 50};

/// Settings to LED blinking with short enabled phase
static const struct bsp_led_pwm blink_rare_on = {.width_on_ms = 150, .width_off_ms = 1000};

/// Settings to LED fastly blinking with equaled enabled & disabled phases
static const struct bsp_led_pwm blink_fast = {.width_on_ms = 250, .width_off_ms = 250};

/// Settings to LED blinking with short disabled phase
static const struct bsp_led_pwm blink_rare_off = {.width_on_ms = 1000, .width_off_ms = 150};

/* Initialization of application layer of RGB LED, see header file for details */
uint8_t app_led_init(void)
{
    uint8_t res = bsp_led_rgb_init();

    if (res != RES_OK)
        return res;

    const struct bsp_led_rgb rgb_coef = {.r = 255, .g = 75, .b = 12};
    res = bsp_led_rgb_calibrate(&rgb_coef);

    return res;
}

/* Deinitialization of application layer of RGB LED, see header file for details */
uint8_t app_led_deinit(void)
{
    return bsp_led_rgb_deinit();
}

/* Set RGB LED behaivour, see header file for details */
uint8_t app_led_set(enum led_event led_event)
{
    switch (led_event) {
    case LED_EVENT_NONE:
        bsp_led_rgb_blink_disable();
        bsp_led_rgb_set(&led_disabled);
        break;

    case LED_EVENT_COMMON_ERROR:
        bsp_led_rgb_blink_disable();
        bsp_led_rgb_set(&led_red);
        break;

    case LED_EVENT_CRC_ERROR:
        bsp_led_rgb_blink_enable(&blink_fast);
        bsp_led_rgb_set(&led_red);
        break;

    case LED_EVENT_FLASH_ERROR:
        bsp_led_rgb_blink_enable(&blink_rare_off);
        bsp_led_rgb_set(&led_red);
        break;

    case LED_EVENT_LCD1602_ERROR:
        bsp_led_rgb_blink_enable(&blink_fast);
        bsp_led_rgb_set(&led_magenta);
        break;

    case LED_EVENT_IN_PROCESS:
        bsp_led_rgb_blink_enable(&blink_rare_on);
        bsp_led_rgb_set(&led_green);
        break;

    case LED_EVENT_SUCCESS:
        bsp_led_rgb_blink_disable();
        bsp_led_rgb_set(&led_green);
        break;

    case LED_EVENT_FAILED:
        bsp_led_rgb_blink_disable();
        bsp_led_rgb_set(&led_magenta);
        break;

    case LED_EVENT_UART_ERROR:
        bsp_led_rgb_blink_enable(&blink_fast);
        bsp_led_rgb_set(&led_yellow);
        break;

    case LED_EVENT_UART_OVERFLOW:
        bsp_led_rgb_blink_disable();
        bsp_led_rgb_set(&led_yellow);
        break;

    default:
        return RES_INVALID_PAR;
    }

    return RES_OK;
}

/** @} */