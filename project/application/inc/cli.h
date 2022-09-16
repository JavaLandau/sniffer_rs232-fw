#ifndef __CLI_H__
#define __CLI_H__

#include "common.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

enum cli_trace_type {
    CLI_TRACE_HEX = 0,
    CLI_TRACE_HYBRID,
    CLI_TRACE_MAX
};

enum cli_interspace {
    CLI_INTERSPCACE_NONE = 0,
    CLI_INTERSPCACE_SPACE,
    CLI_INTERSPCACE_NEW_LINE,
    CLI_INTERSPCACE_MAX
};

uint8_t cli_init(void);
uint8_t cli_menu_start(void);
void cli_trace(const char *format, ...);

#endif //__CLI_H__