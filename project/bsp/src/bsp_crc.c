/**
\file
\author JavaLandau
\copyright MIT License
\brief BSP CRC module

The file includes implementation of BSP layer of the CRC
*/

#include "common.h"
#include "bsp_crc.h"
#include <string.h>

/** 
 * \defgroup bsp_crc BSP CRC
 * \brief Module of BSP CRC
 * \ingroup bsp
 * @{
*/

/// STM32 HAL CRC instance
static CRC_HandleTypeDef crc_module = {.Instance = CRC};

/** STM32 HAL MSP CRC initialization
 * 
 * \param[in] hcrc STM32 HAL CRC instance, should equal to \ref crc_module
*/
void HAL_CRC_MspInit(CRC_HandleTypeDef *hcrc)
{
    if (!hcrc || hcrc->Instance != CRC)
        return;

    __HAL_RCC_CRC_CLK_ENABLE();
}

/** STM32 HAL MSP CRC deinitialization
 * 
 * \param[in] hcrc STM32 HAL CRC instance, should equal to \ref crc_module
*/
void HAL_CRC_MspDeInit(CRC_HandleTypeDef *hcrc)
{
    if (!hcrc || hcrc->Instance != CRC)
        return;

    __HAL_RCC_CRC_CLK_DISABLE();
}

/* BSP CRC initialization, see header file for details */
uint8_t bsp_crc_init(void)
{
    return (HAL_CRC_Init(&crc_module) == HAL_OK) ? RES_OK : RES_NOK;
}

/* BSP CRC deinitialization, see header file for details */
uint8_t bsp_crc_deinit(void)
{
    return (HAL_CRC_DeInit(&crc_module) == HAL_OK) ? RES_OK : RES_NOK;
}

/* BSP CRC calculation, see header file for details */
uint8_t bsp_crc_calc(uint8_t *data, uint32_t len, uint32_t *result)
{
    if (!data || !len || !result)
        return RES_INVALID_PAR;

    uint32_t last_word = 0;
    uint8_t len_tail = len % sizeof(uint32_t);
    if (len_tail)
        memcpy(&last_word, &data[len - len_tail], len_tail);

    *result = HAL_CRC_Calculate(&crc_module, (uint32_t*)data, len / sizeof(uint32_t));

    if (len_tail)
        *result = HAL_CRC_Accumulate(&crc_module, &last_word, 1);

    return RES_OK;
}

/** @} */