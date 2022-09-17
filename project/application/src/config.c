#include "config.h"
#include "bsp_crc.h"
#include <string.h>

#define FLASH_SECTOR_CFG_ADDR   (0x08060000)

uint8_t config_save(struct flash_config *config)
{
    if (!config)
        return RES_INVALID_PAR;

    // CRC calculation
    uint32_t crc = 0;
    uint8_t res = bsp_crc_calc((uint8_t*)config, sizeof(struct flash_config) - sizeof(uint32_t), &crc);

    if (res != RES_OK)
        return RES_NOK;

    config->crc = crc;

    // Erase necessary sectors of INT FLASH
    uint32_t sec_error = 0;

    FLASH_EraseInitTypeDef erase_init;
    erase_init.Banks = FLASH_BANK_1;
    erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase_init.Sector = FLASH_SECTOR_7;
    erase_init.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase_init.NbSectors = 1;

    HAL_StatusTypeDef hal_res = HAL_FLASH_Unlock();

    if(hal_res != HAL_OK)
        return RES_NOK;

    do {
        hal_res = HAL_FLASHEx_Erase(&erase_init, &sec_error);

        if(hal_res != HAL_OK) {
            res = RES_NOK;
            break;
        }

        // Write into INT FLASH
        uint32_t size = sizeof(struct flash_config);
        uint8_t size_tail = size % sizeof(uint32_t);
        uint8_t *p_config = (uint8_t*)config;
        uint32_t int_addr = FLASH_SECTOR_CFG_ADDR;

        for(uint32_t i = 0; i < (size - size_tail); i += sizeof(uint32_t)){
            hal_res = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, int_addr, *((uint32_t*)p_config));

            if (hal_res != HAL_OK)
                break;

            p_config += sizeof(uint32_t);
            int_addr += sizeof(uint32_t);
        }

        if (hal_res != HAL_OK) {
            res = RES_NOK;
            break;
        }

        for(uint32_t i = 0; i < size_tail; i++){
            hal_res = HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, int_addr, *p_config);

            if (hal_res != HAL_OK)
                break;

            p_config += sizeof(uint8_t);
            int_addr += sizeof(uint8_t);
        }

        if (hal_res != HAL_OK) {
            res = RES_NOK;
            break;
        }
    } while(0);

    HAL_FLASH_Lock();

    return res;
}

uint8_t config_read(struct flash_config *config)
{
    if (!config)
        return RES_INVALID_PAR;

    // Read from flash
    uint32_t int_addr = FLASH_SECTOR_CFG_ADDR;
    memcpy((uint8_t*)config, (uint8_t*)int_addr, sizeof(struct flash_config));

    // CRC check
    uint32_t crc = 0;
    uint8_t res = bsp_crc_calc((uint8_t*)config, sizeof(struct flash_config) - sizeof(uint32_t), &crc);

    if (res != RES_OK)
        return RES_NOK;

    if (config->crc != crc)
        return RES_NOK;

    return RES_OK;
}