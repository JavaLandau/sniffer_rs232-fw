#ifndef __BSP_GPIO_H__
#define __BSP_GPIO_H__

#include <stdint.h> 
#include "stm32f4xx_hal.h"

uint8_t gpio_bulk_read(GPIO_TypeDef* gpiox, uint16_t *gpio_pins, uint16_t *gpio_states);
uint8_t gpio_bulk_write(GPIO_TypeDef* gpiox, uint16_t *gpio_pins, uint16_t gpio_states);

#define GPIO_PORT_READ(GPIOX, GPIO_PIN) !!(GPIOX->IDR & GPIO_PIN)

#endif //__BSP_GPIO_H__