#include "cli.h"
#include "menu.h"
#include "bsp_uart.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>

#define UART_BUFF_SIZE     256

static struct {
    bool uart_error;
    bool uart_overflow;
} cli_state = {0};

static uint8_t *__menu_rx_buff = NULL;

void __cli_uart_overflow_cb(enum uart_type type, void *params)
{
    cli_state.uart_overflow = true;
}

void __cli_uart_error_cb(enum uart_type type, uint32_t error, void *params)
{
    cli_state.uart_error = true;
    bsp_uart_start(type);
}

uint8_t __menu_write_cb(char *data)
{
    if (!data)
        return RES_INVALID_PAR;

    if (strnlen(data, UART_BUFF_SIZE) == UART_BUFF_SIZE)
        return RES_INVALID_PAR;

    return bsp_uart_write(BSP_UART_TYPE_CLI, (uint8_t*)data, strlen(data), 1000);
}

uint8_t __menu_read_cb(char **data)
{
    if (!data)
        return RES_INVALID_PAR;

    uint16_t len = 0;
    uint8_t res = bsp_uart_read(BSP_UART_TYPE_CLI, __menu_rx_buff, &len, 1000);

    if (res == RES_OK)
        __menu_rx_buff[len] = 0;

    *data = (char*)__menu_rx_buff;

    return res;
}

uint8_t cli_init(void)
{
    memset(&cli_state, 0, sizeof(cli_state));

    __menu_rx_buff = (uint8_t*)malloc(UART_BUFF_SIZE + 1);

    if (!__menu_rx_buff)
        return RES_MEMORY_ERR;

    struct uart_init_ctx uart_init;
    uart_init.baudrate = 921600;
    uart_init.wordlen = BSP_UART_WORDLEN_8;
    uart_init.parity = BSP_UART_PARITY_NONE;
    uart_init.stopbits = BSP_UART_STOPBITS_1;
    uart_init.rx_size = UART_BUFF_SIZE;
    uart_init.tx_size = UART_BUFF_SIZE;
    uart_init.params = NULL;
    uart_init.error_isr_cb = __cli_uart_error_cb;
    uart_init.overflow_isr_cb = __cli_uart_overflow_cb;

    return bsp_uart_init(BSP_UART_TYPE_CLI, &uart_init);
}


void cli_trace(const char *format, ...)
{
    static char buffer[UART_BUFF_SIZE] = {0};

    va_list args;
    va_start(args, format);

    uint32_t len = vsnprintf(buffer, UART_BUFF_SIZE - 1, format, args);

    if (len > 0)
        bsp_uart_write(BSP_UART_TYPE_CLI, (uint8_t*)buffer, len, 1000);

    va_end(args);
}

static uint8_t __cli_menu_entry(char *input, void *param)
{
    menu_entry(menu_current_item_get()->menu_entry);
    return RES_OK;
}

static uint8_t __cli_menu_set_value(char *input, void *param)
{
    menu_item_value_set(menu_current_item_get(), input);
    return RES_OK;
}

static uint8_t __cli_menu_certain_set_value(char *input, void *param)
{
    struct menu *menu = (struct menu*)param;
    menu_item_value_set(menu_item_by_label_get(menu, "Menu third"), menu_item_label_get(menu_current_item_get()));
    menu_entry(menu_current_item_get()->menu_entry);
    return RES_OK;
}

static uint8_t __cli_menu_exit(char *input, void *param)
{
    menu_exit();
    return RES_OK;
}

uint8_t cli_menu_start(void)
{
    struct menu_config config = {
        .is_looped = true,
        .num_delim = '.',
        .width = 64,
        .num_type = MENU_NUM_DIGITAL,
        .pass_type = MENU_PASS_WITH_PROMPT,
        .read_callback = __menu_read_cb,
        .write_callback = __menu_write_cb
    };

    //struct menu_color_config color_config_select = MENU_COLOR_CONFIG_DEFAULT();
    //struct menu_color_config color_config_choose = {.active = {.foreground = MENU_COLOR_YELLOW, .background = MENU_COLOR_RED},
    //                                                .active = {.foreground = MENU_COLOR_WHITE, .background = MENU_COLOR_BLUE},};

    struct menu *menu1 = menu_create("CONFIGURATION", '*', NULL);

    struct menu *menu11 = menu_create("SUBMENU ONE", '~', NULL);
    struct menu *menu12 = menu_create("SUBMENU SECOND", '=', NULL);
    struct menu *menu13 = menu_create("SUBMENU THIRD", '+', NULL);
    struct menu *menu14 = menu_create("SUBMENU ENORMOUS", ' ', NULL);

    menu_item_add(menu1, "Menu one", NULL, NULL, __cli_menu_entry, NULL, menu11);
    menu_item_add(menu1, "Menu second", NULL, NULL, __cli_menu_entry, NULL, menu12);
    menu_item_add(menu1, "Menu third", NULL, "[]", __cli_menu_entry, NULL, menu13);
    menu_item_add(menu1, "Menu enormous", NULL, NULL, __cli_menu_entry, NULL, menu14);
    menu_item_add(menu1, "Menu exit", NULL, NULL, __cli_menu_exit, NULL, NULL);

    menu_item_add(menu11, "Submenu one", "baudrate: ", "[[]]", __cli_menu_set_value, NULL, NULL);
    menu_item_add(menu11, "Submenu second", "level: ", "{}", __cli_menu_set_value, NULL, NULL);
    menu_item_add(menu11, "Submenu third", "something else: ", "\'\'", __cli_menu_set_value, NULL, NULL);
    menu_item_add(menu11, "Submenu1 exit", NULL, "\'\'", __cli_menu_entry, NULL, menu1);

    menu_item_add(menu12, "Submenu fourth", "math power: ", "\\", __cli_menu_set_value, NULL, NULL);
    menu_item_add(menu12, "Submenu fifth", "not changable value: ", NULL, __cli_menu_set_value, NULL, NULL);
    menu_item_add(menu12, "Submenu2 exit", NULL, "\'\'", __cli_menu_entry, NULL, menu1);

    menu_item_add(menu13, "TX", NULL, NULL, __cli_menu_certain_set_value, menu1, menu1);
    menu_item_add(menu13, "RX", NULL, NULL, __cli_menu_certain_set_value, menu1, menu1);
    menu_item_add(menu13, "TX/RX", NULL, NULL, __cli_menu_certain_set_value, menu1, menu1);
    menu_item_add(menu13, "Submenu3 exit", NULL, NULL, __cli_menu_entry, NULL, menu1);

    for (uint32_t i = 0; i < 32; i++)
        menu_item_add(menu14, "test", NULL, NULL, __cli_menu_entry, NULL, menu1);

    menu_start(&config, menu1);

    return RES_OK;
}