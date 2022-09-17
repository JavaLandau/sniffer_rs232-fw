#ifndef __BSP_CRC_H__
#define __BSP_CRC_H__

#include <stdint.h> 
#include "stm32f4xx_hal.h"

uint8_t bsp_crc_init(void);
uint8_t bsp_crc_deinit(void);
uint8_t bsp_crc_calc(uint8_t *data, uint32_t len, uint32_t *result);

#endif //__BSP_RCC_H__