#ifndef __BSP_UART_H__
#define __BSP_UART_H__

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

#define UART_TYPE_VALID(X)      (((uint32_t)(X) < BSP_UART_TYPE_MAX))
#define UART_WORDLEN_VALID(X)   (((X) == BSP_UART_WORDLEN_8) || ((X) == BSP_UART_WORDLEN_9))
#define UART_PARITY_VALID(X)    (((X) == BSP_UART_PARITY_NONE) || ((X) == BSP_UART_PARITY_EVEN) || ((X) == BSP_UART_PARITY_ODD))
#define UART_STOPBITS_VALID(X)  (((X) == BSP_UART_STOPBITS_1) || ((X) == BSP_UART_STOPBITS_2))

#define BSP_UART_ERROR_PE       HAL_UART_ERROR_PE
#define BSP_UART_ERROR_NE       HAL_UART_ERROR_NE
#define BSP_UART_ERROR_FE       HAL_UART_ERROR_FE
#define BSP_UART_ERROR_ORE      HAL_UART_ERROR_ORE
#define BSP_UART_ERROR_DMA      HAL_UART_ERROR_DMA

#define BSP_UART_ERRORS_ALL     (BSP_UART_ERROR_PE | BSP_UART_ERROR_NE | BSP_UART_ERROR_FE | BSP_UART_ERROR_ORE | BSP_UART_ERROR_DMA)


enum uart_type {
    BSP_UART_TYPE_CLI = 0,
    BSP_UART_TYPE_RS232_TX,
    BSP_UART_TYPE_RS232_RX,
    BSP_UART_TYPE_MAX
};

enum uart_wordlen {
    BSP_UART_WORDLEN_8 = 8,
    BSP_UART_WORDLEN_9 = 9
};

enum uart_parity {
    BSP_UART_PARITY_NONE = 0,
    BSP_UART_PARITY_EVEN = 1,
    BSP_UART_PARITY_ODD = 2
};

enum uart_stopbits {
    BSP_UART_STOPBITS_1 = 1,
    BSP_UART_STOPBITS_2 = 2
};

struct uart_init_ctx {
    uint32_t baudrate;
    uint32_t tx_size;
    uint32_t rx_size;
    bool lin_enabled;
    enum uart_wordlen wordlen;
    enum uart_parity parity;
    enum uart_stopbits stopbits;
    void (*error_isr_cb)(enum uart_type type, uint32_t error, void *params);
    void (*overflow_isr_cb)(enum uart_type type, void *params);
    void (*lin_break_isr_cb)(enum uart_type type, void *params);
    void *params;
};

uint8_t bsp_uart_init(enum uart_type type, struct uart_init_ctx *init);
uint8_t bsp_uart_deinit(enum uart_type type);
uint8_t bsp_uart_read(enum uart_type type, uint8_t *data, uint16_t *len, uint32_t tmt_ms);
uint8_t bsp_uart_write(enum uart_type type, uint8_t *data, uint16_t len, uint32_t tmt_ms);
uint8_t bsp_uart_start(enum uart_type type);
uint8_t bsp_uart_stop(enum uart_type type);
bool bsp_uart_is_started(enum uart_type type);
bool bsp_uart_rx_queue_is_empty(enum uart_type type);

#endif //__BSP_UART_H__