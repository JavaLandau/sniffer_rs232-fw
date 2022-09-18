#ifndef __CLI_H__
#define __CLI_H__

#include "common.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "config.h"

uint8_t cli_init(void);
uint8_t cli_menu_start(struct flash_config *config);
void cli_trace(const char *format, ...);

#endif //__CLI_H__