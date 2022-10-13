#ifndef __BSP_LED_RGB_H__
#define __BSP_LED_RGB_H__

#include <stdint.h> 
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "bsp_gpio.h"
#include "common.h"

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

#define BSP_LED_RGB_HARDFAULT() \
    do {\
        BSP_GPIO_FORCE_OUTPUT_MODE(GPIOA, 8);\
        BSP_GPIO_FORCE_OUTPUT_MODE(GPIOA, 9);\
        BSP_GPIO_FORCE_OUTPUT_MODE(GPIOA, 10);\
        \
        BSP_GPIO_PORT_WRITE(GPIOA, GPIO_PIN_8, false);\
        BSP_GPIO_PORT_WRITE(GPIOA, GPIO_PIN_10, false);\
        \
        while (true) {\
            BSP_GPIO_PORT_WRITE(GPIOA, GPIO_PIN_9, false);\
            INSTR_DELAY_US(100000);\
            BSP_GPIO_PORT_WRITE(GPIOA, GPIO_PIN_9, true);\
            INSTR_DELAY_US(100000);\
        }\
    } while (0)

#endif //__BSP_LED_RGB_H__