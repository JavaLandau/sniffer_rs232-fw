#ifndef __BSP_GPIO_H__
#define __BSP_GPIO_H__

#include <stdint.h>
#include "stm32f4xx_hal.h"

uint8_t bsp_gpio_bulk_read(GPIO_TypeDef* gpiox, uint16_t *gpio_pins, uint16_t *gpio_states);
uint8_t bsp_gpio_bulk_write(GPIO_TypeDef* gpiox, uint16_t *gpio_pins, uint16_t gpio_states);

#define BSP_GPIO_PORT_READ(GPIOX, GPIO_PIN)             (!!(GPIOX->IDR & GPIO_PIN))
#define BSP_GPIO_PORT_WRITE(GPIOX, GPIO_PIN, LEVEL)     (GPIOX->BSRR = LEVEL ? GPIO_PIN : ((uint32_t)GPIO_PIN << 16U))

#define BSP_GPIO_FORCE_OUTPUT_MODE(GPIOX, GPIO_NUM)  \
            do {\
                GPIOX->MODER = (GPIOX->MODER | (1 << (2 * GPIO_NUM))) & (~(1 << (2 * GPIO_NUM + 1)));\
            } while(0)

#endif //__BSP_GPIO_H__