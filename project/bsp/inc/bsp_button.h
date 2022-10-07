#ifndef __BSP_BUTTON_H__
#define __BSP_BUTTON_H__

#include <stdint.h>
#include "stm32f4xx_hal.h"

enum button_action {
    BUTTON_NONE = 0,
    BUTTON_PRESSED,
    BUTTON_LONG_PRESSED,
    BUTTON_ACTION_MAX
};

struct button_init_ctx {
    uint32_t press_delay_ms;
    uint32_t press_min_dur_ms;
    uint32_t long_press_dur_ms;
    void (*button_isr_cb)(enum button_action action);
};

uint8_t bsp_button_init(struct button_init_ctx *init_ctx);
uint8_t bsp_button_deinit(void);


#endif //__BSP_BUTTON_H__