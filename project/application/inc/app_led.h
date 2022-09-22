#ifndef __APP_LED_H
#define __APP_LED_H

#include "common.h"
#include <stdint.h>
#include <stddef.h>

enum led_event {
    LED_EVENT_NONE = 0,
    LED_EVENT_COMMON_ERROR,
    LED_EVENT_CRC_ERROR,
    LED_EVENT_FLASH_ERROR,
    LED_EVENT_LCD1602_ERROR,
    LED_EVENT_IN_PROCESS,
    LED_EVENT_SUCCESS,
    LED_EVENT_FAILED,
    LED_EVENT_UART_ERROR,
    LED_EVENT_UART_OVERFLOW,
    LED_EVENT_MAX
};

#define LED_EVENT_IS_VALID(X)          (((uint32_t)(X)) < LED_EVENT_MAX)

uint8_t app_led_init(void);
uint8_t app_led_deinit(void);
uint8_t app_led_set(enum led_event led_event);

#endif /* __APP_LED_H */
