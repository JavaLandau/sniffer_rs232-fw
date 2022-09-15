#ifndef __CLI_H__
#define __CLI_H__

#include "common.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

uint8_t cli_init(void);
uint8_t cli_menu_start(void);
void cli_trace(const char *format, ...);

#endif //__CLI_H__