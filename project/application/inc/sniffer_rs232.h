#ifndef __SNIFFER_RS232_H__
#define __SNIFFER_RS232_H__

#include "common.h"
#include "stm32f4xx_hal.h"
#include "bsp_uart.h"
#include <stdbool.h>

enum rs232_channel_type {
    RS232_CHANNEL_TX = 0,
    RS232_CHANNEL_RX,
    RS232_CHANNEL_ANY,
    RS232_CHANNEL_ALL,
    RS232_CHANNEL_MAX
};

#define RS232_CHANNEL_TYPE_VALID(TYPE)          (((uint32_t)(TYPE)) < RS232_CHANNEL_MAX)

struct sniffer_rs232_config {
    uint32_t valid_packets_count;
    uint32_t uart_error_count;
    uint8_t baudrate_tolerance;
    uint32_t min_detect_bits;
    uint32_t exec_timeout;
    uint32_t calc_attempts;
};

#define SNIFFER_RS232_CFG_PARAM_MIN(X)          sniffer_rs232_config_item_range((uint32_t)&((struct sniffer_rs232_config*)0)->X, true)
#define SNIFFER_RS232_CFG_PARAM_MAX(X)          sniffer_rs232_config_item_range((uint32_t)&((struct sniffer_rs232_config*)0)->X, false)

#define SNIFFER_RS232_CFG_PARA_IS_VALID(X, V)   (((V) >= SNIFFER_RS232_CFG_PARAM_MIN(X)) && ((V) <= SNIFFER_RS232_CFG_PARAM_MAX(X)))

#define SNIFFER_RS232_CONFIG_DEFAULT() {\
                .valid_packets_count = 20,\
                .uart_error_count = 2,\
                .baudrate_tolerance = 10,\
                .min_detect_bits = 48,\
                .exec_timeout = 600,\
                .calc_attempts = 3\
            }

uint8_t sniffer_rs232_init(struct sniffer_rs232_config *__config);
uint8_t sniffer_rs232_deinit(void);
uint8_t sniffer_rs232_calc(enum rs232_channel_type channel_type, struct uart_init_ctx *uart_params);
uint32_t sniffer_rs232_config_item_range(uint32_t shift, bool is_min);
bool sniffer_rs232_config_check(struct sniffer_rs232_config *__config);

#endif //__SNIFFER_RS232_H__