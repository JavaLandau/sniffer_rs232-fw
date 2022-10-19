/**
\file
\author JavaLandau
\copyright MIT License
\brief BSP GPIO module

The file includes implementation of BSP layer of the GPIO
*/

#include "common.h"
#include "bsp_gpio.h"

/** 
 * \defgroup bsp_gpio BSP GPIO
 * \brief Module of BSP GPIO
 * \ingroup bsp
 * @{
*/

/* GPIO bulk reading, see header file for details */
uint8_t bsp_gpio_bulk_read(GPIO_TypeDef* gpiox, const uint16_t *gpio_pins, uint16_t *gpio_states)
{
    if (!gpiox || !gpio_states || !gpio_pins)
        return RES_INVALID_PAR;

    uint32_t read_gpio = gpiox->IDR;

    const uint16_t *loc_gpio_pins = gpio_pins;
    uint8_t state_pos = 0;
    while (*loc_gpio_pins) {
        *gpio_states |= ((read_gpio & *loc_gpio_pins) ? 1 : 0) << state_pos++;
        loc_gpio_pins++;
    }

    return RES_OK;
}

/* GPIO bulk writing, see header file for details */
uint8_t bsp_gpio_bulk_write(GPIO_TypeDef* gpiox, const uint16_t *gpio_pins, const uint16_t gpio_states)
{
    if (!gpiox || !gpio_pins)
        return RES_INVALID_PAR;

    uint32_t bulk_gpio = 0x00;
    uint8_t state_pos = 0;
    const uint16_t *loc_gpio_pins = gpio_pins;
    while (*loc_gpio_pins) {
        if (gpio_states & (1 << state_pos++))
            bulk_gpio |= *loc_gpio_pins;
        else
            bulk_gpio |= *loc_gpio_pins << 16U;

        loc_gpio_pins++;
    }

    gpiox->BSRR = bulk_gpio;

    return RES_OK;
}

/** @} */