#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "common.h"
#include "stm32f4xx_hal.h"
#include "sniffer_rs232.h"

#define RS232_TRACE_TYPE_VALID(X)       ((X) < RS232_TRACE_MAX)
#define RS232_INTERSPACE_TYPE_VALID(X)  ((X) < RS232_INTERSPCACE_MAX)

enum rs232_trace_type {
    RS232_TRACE_HEX = 0,
    RS232_TRACE_HYBRID,
    RS232_TRACE_MAX
};

enum rs232_interspace_type {
    RS232_INTERSPCACE_NONE = 0,
    RS232_INTERSPCACE_SPACE,
    RS232_INTERSPCACE_NEW_LINE,
    RS232_INTERSPCACE_MAX
};

struct uart_presettings {
    bool enable;
    uint32_t baudrate;
    enum uart_wordlen wordlen;
    enum uart_parity parity;
    enum uart_stopbits stopbits;
    bool lin_enabled;
};

#pragma pack(1)
struct flash_config {
    struct sniffer_rs232_config alg_config;
    struct uart_presettings presettings;
    enum rs232_trace_type trace_type;
    enum rs232_interspace_type idle_presence;
    enum rs232_interspace_type txrx_delimiter;
    bool save_to_presettings;
    uint32_t crc;
};
#pragma pack()

#define UART_PRESETTINGS_DEFAULT()  {\
    .enable = false,\
    .parity = BSP_UART_PARITY_NONE,\
    .baudrate = 0,\
    .stopbits = BSP_UART_STOPBITS_1,\
    .wordlen = BSP_UART_WORDLEN_8,\
    .lin_enabled = false\
}

#define FLASH_CONFIG_DEFAULT()  {\
    .alg_config = SNIFFER_RS232_CONFIG_DEFAULT(),\
    .presettings = UART_PRESETTINGS_DEFAULT(),\
    .trace_type = RS232_TRACE_HEX,\
    .idle_presence = RS232_INTERSPCACE_NONE,\
    .txrx_delimiter = RS232_INTERSPCACE_NONE,\
    .save_to_presettings = true\
}

uint8_t config_save(struct flash_config *config);
uint8_t config_read(struct flash_config *config);

#endif //__CONFIG_H__