/**
\file
\author JavaLandau
\copyright MIT License
\brief Header of BSP CRC module
*/

#ifndef __BSP_CRC_H__
#define __BSP_CRC_H__

#include <stdint.h> 
#include "stm32f4xx_hal.h"

/** 
 * \addtogroup bsp_crc
 * @{
*/

/** BSP CRC initialization
 * 
 * \return \ref RES_OK on success error otherwise
*/
uint8_t bsp_crc_init(void);

/** BSP CRC deinitialization
 * 
 * \return \ref RES_OK on success error otherwise
*/
uint8_t bsp_crc_deinit(void);

/** BSP CRC calculation
 * 
 * \param[in] data data over which CRC is calculated
 * \param[in] len size of data within which CRC is calculated
 * \param[out] result calculated CRC value
 * \return \ref RES_OK on success error otherwise
*/
uint8_t bsp_crc_calc(uint8_t *data, uint32_t len, uint32_t *result);

/** @} */

#endif //__BSP_CRC_H__