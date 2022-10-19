/**
\file
\author JavaLandau
\copyright MIT License
\brief Header of BSP GPIO module
*/

#ifndef __BSP_GPIO_H__
#define __BSP_GPIO_H__

#include <stdint.h>
#include "stm32f4xx_hal.h"

/** 
 * \addtogroup bsp_gpio
 * @{
*/

/** GPIO bulk reading
 * 
 * \param[in] gpiox port of read pins
 * \param[in] gpio_pins GPIO pins levels on which should be read
 * \param[out] gpio_states array of levels read on \p gpio_pins
 * \return \ref RES_OK on success error otherwise
*/
uint8_t bsp_gpio_bulk_read(GPIO_TypeDef* gpiox, const uint16_t *gpio_pins, uint16_t *gpio_states);

/** GPIO bulk writing
 * 
 * \param[in] gpiox port of written pins
 * \param[in] gpio_pins GPIO pins levels on which should be set
 * \param[in] gpio_states array of levels set on \p gpio_pins
 * \return \ref RES_OK on success error otherwise
*/
uint8_t bsp_gpio_bulk_write(GPIO_TypeDef* gpiox, const uint16_t *gpio_pins, const uint16_t gpio_states);


/** MACRO GPIO level get
 * 
 * The macro is used for fast reading of a level on a pin
 * 
 * \param[in] GPIOX port of read pin
 * \param[in] GPIO_PIN read pin
 * \return read level on the pin
*/
#define BSP_GPIO_PORT_READ(GPIOX, GPIO_PIN)             (!!(GPIOX->IDR & GPIO_PIN))

/** MACRO GPIO level set
 * 
 * The macro is used for fast setting of a level to a pin
 * 
 * \param[in] GPIOX port of set pin
 * \param[in] GPIO_PIN set pin
 * \param[in] LEVEL set level, true if active one false otherwise
*/
#define BSP_GPIO_PORT_WRITE(GPIOX, GPIO_PIN, LEVEL)     (GPIOX->BSRR = LEVEL ? GPIO_PIN : ((uint32_t)GPIO_PIN << 16U))

/** MACRO Set output mode to a pin
 * 
 * The macro is used for fast setting of output mode of a pin
 * 
 * \param[in] GPIOX port of a pin
 * \param[in] GPIO_NUM number of GPIO pin (0..15)
*/
#define BSP_GPIO_FORCE_OUTPUT_MODE(GPIOX, GPIO_NUM)  \
            do {\
                GPIOX->MODER = (GPIOX->MODER | (1 << (2 * GPIO_NUM))) & (~(1 << (2 * GPIO_NUM + 1)));\
            } while(0)

/** @} */

#endif //__BSP_GPIO_H__