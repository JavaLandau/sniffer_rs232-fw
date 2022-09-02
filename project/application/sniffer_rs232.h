#ifndef __SNIFFER_RS232_H__
#define __SNIFFER_RS232_H__

#include "common.h"
#include "stm32f4xx_hal.h"
#include "bsp_uart.h"

enum rs232_calc_type {
    RS232_CALC_TX = 0,
    RS232_CALC_RX,
    RS232_CALC_ANY,
    RS232_CALC_ALL,
    RS232_CALC_MAX
};

#define RS232_CALC_TYPE_VALID(TYPE)  (((uint32_t)(TYPE)) < RS232_CALC_MAX)

uint8_t sniffer_rs232_init(void);
uint8_t sniffer_rs232_deinit(void);
uint8_t sniffer_rs232_calc(enum rs232_calc_type calc_type, struct uart_init_ctx *uart_params);

#endif //__SNIFFER_RS232_H__