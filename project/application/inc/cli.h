#ifndef __CLI_H__
#define __CLI_H__

#include "common.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "config.h"

uint8_t cli_init(void);
uint8_t cli_menu_start(struct flash_config *config);
uint8_t cli_menu_exit(void);
bool cli_menu_is_started(void);
void cli_trace(const char *format, ...);
uint8_t cli_rs232_trace(enum uart_type uart_type,
                        enum rs232_trace_type trace_type,
                        uint8_t *data,
                        uint32_t len,
                        bool break_line);
uint8_t cli_welcome(const char *welcome, uint8_t wait_time_s, bool *forced_exit, bool *is_pressed);
void cli_terminal_reset(void);

#endif //__CLI_H__