#ifndef __BSP_LED_RGB_H__
#define __BSP_LED_RGB_H__

#include <stdint.h> 
#include "stm32f4xx_hal.h"

uint8_t bsp_led_rgb_calibrate(uint8_t _coef_r, uint8_t _coef_g, uint8_t _coef_b);
uint8_t bsp_led_rgb_set(uint8_t r, uint8_t g, uint8_t b);
uint8_t bsp_led_rgb_deinit(void);
uint8_t bsp_led_rgb_init(void);

#endif //__BSP_LED_RGB_H__