#ifndef __BSP_LED_RGB_H__
#define __BSP_LED_RGB_H__

#include <stdint.h> 
#include "stm32f4xx_hal.h"

struct bsp_led_rgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
};

struct bsp_led_pwm {
    uint32_t width_on_ms;
    uint32_t width_off_ms;
};

uint8_t bsp_led_rgb_calibrate(const struct bsp_led_rgb *coef_rgb);
uint8_t bsp_led_rgb_set(const struct bsp_led_rgb *rgb);
uint8_t bsp_led_rgb_deinit(void);
uint8_t bsp_led_rgb_init(void);

uint8_t bsp_led_rgb_blink_disable(void);
uint8_t bsp_led_rgb_blink_enable(const struct bsp_led_pwm *pwm);

#endif //__BSP_LED_RGB_H__