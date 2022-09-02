#ifndef __BSP_RCC_H__
#define __BSP_RCC_H__

#include <stdint.h> 
#include "stm32f4xx_hal.h"

#if defined(STM32F446xx)
#define TIM_APB_NUM_CLOCK_GET(INSTANCE)  \
            ((IS_TIM_INSTANCE(INSTANCE)) ? (\
            (((INSTANCE) == TIM1) || \
            ((INSTANCE) == TIM8) || \
            ((INSTANCE) == TIM9) || \
            ((INSTANCE) == TIM10) || \
            ((INSTANCE) == TIM11)) ? 2 : 1) : 0)
#endif

uint8_t rcc_main_config_init(void);
uint32_t rcc_apb_timer_freq_get(TIM_TypeDef *instance);

#endif //__BSP_RCC_H__