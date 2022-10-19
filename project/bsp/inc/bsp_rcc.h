/**
\file
\author JavaLandau
\copyright MIT License
\brief Header of BSP RCC module
*/

#ifndef __BSP_RCC_H__
#define __BSP_RCC_H__

#include <stdint.h> 
#include "stm32f4xx_hal.h"

/** 
 * \addtogroup bsp_rcc
 * @{
*/

/** MACRO number of APB source clock for timers
 * 
 * \note The macro is designed to used for chips STM32F446xx
 * 
 * \param[in] INSTANCE TIM instance
 * \return 1 for APB1, 2 for APB2, 0 if error
*/
#define TIM_APB_NUM_CLOCK_GET(INSTANCE)  \
            ((IS_TIM_INSTANCE(INSTANCE)) ? (\
            (((INSTANCE) == TIM1) || \
            ((INSTANCE) == TIM8) || \
            ((INSTANCE) == TIM9) || \
            ((INSTANCE) == TIM10) || \
            ((INSTANCE) == TIM11)) ? 2 : 1) : 0)

/** Configuration of main clocks
 * 
 * The function executes configuration of main MPU clocks
 * 
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_rcc_main_config_init(void);

/** Get frequency of TIM internal clock
 * 
 * \param[in] instance STM32 HAL TIM instance
 * \return frequency in Hz
 */
uint32_t bsp_rcc_apb_timer_freq_get(TIM_TypeDef *instance);

/** @} */

#endif //__BSP_RCC_H__