#include "common.h"
#include "stm32f4xx_hal.h"
#include "app_led.h"
#include "bsp_rcc.h"
#include "bsp_lcd1602.h"
#include "bsp_uart.h"
#include "bsp_crc.h"
#include "sniffer_rs232.h"
#include "config.h"
#include "cli.h"
#include <stdbool.h>

static char uart_parity_sym[] = {'N', 'E', 'O'};
static struct {
    uint32_t error;
    bool overflow;
} uart_flags[BSP_UART_TYPE_MAX] = {0};

static void uart_overflow_cb(enum uart_type type, void *params)
{
    if (!UART_TYPE_VALID(type))
        return;

    uart_flags[type].overflow = true;
}

static void uart_error_cb(enum uart_type type, uint32_t error, void *params)
{
    if (!UART_TYPE_VALID(type))
        return;

    uart_flags[type].error = error;
    bsp_uart_start(type);
}

static void internal_error(enum led_event led_event)
{
    app_led_set(led_event);

    while(1){}
}

int main()
{
    HAL_Init();

    if (bsp_rcc_main_config_init() != RES_OK)
        HAL_NVIC_SystemReset();

    uint8_t res = app_led_init();

    if (res != RES_OK)
        HAL_NVIC_SystemReset();

    res = bsp_crc_init();

    if (res != RES_OK)
        internal_error(LED_EVENT_CRC_ERROR);

    struct flash_config config;

    if (config_read(&config) != RES_OK) {
        struct flash_config flash_config_default = FLASH_CONFIG_DEFAULT();
        config = flash_config_default;
        res = config_save(&config);

        if (res != RES_OK)
            internal_error(LED_EVENT_FLASH_ERROR);
    }

    struct lcd1602_settings settings = {
        .num_line = LCD1602_NUM_LINE_2,
        .font_size = LCD1602_FONT_SIZE_5X8,
        .type_move_cursor = LCD1602_CURSOR_MOVE_RIGHT,
        .shift_entire_disp = LCD1602_SHIFT_ENTIRE_PERFORMED,
        .type_interface = LCD1602_INTERFACE_8BITS,
        .disp_state = LCD1602_DISPLAY_ON,
        .cursor_state = LCD1602_CURSOR_OFF,
        .cursor_blink_state = LCD1602_CURSOR_BLINK_OFF
    };

    res = bsp_lcd1602_init(&settings);

    if (res != RES_OK)
        internal_error(LED_EVENT_LCD1602_ERROR);

    res = cli_init();

    if (res != RES_OK) {
        bsp_lcd1602_cprintf("CLI ERROR", NULL);
        internal_error(LED_EVENT_COMMON_ERROR);
    }

    bsp_lcd1602_cprintf("CONFIGURATION", NULL);

    res = cli_menu_start(&config);

    if (res != RES_OK) {
        bsp_lcd1602_cprintf("MENU ERROR", NULL);
        internal_error(LED_EVENT_COMMON_ERROR);
    }

    struct uart_init_ctx uart_params = {0};

    if (!config.presettings.enable) {
        res = sniffer_rs232_init(&config.alg_config);

        if (res != RES_OK) {
            bsp_lcd1602_cprintf("ALG ERROR", NULL);
            internal_error(LED_EVENT_COMMON_ERROR);
        }

        app_led_set(LED_EVENT_IN_PROCESS);
        bsp_lcd1602_cprintf("ALG PROCESS...", NULL);

        res = sniffer_rs232_calc(&uart_params);
    } else {
        uart_params.baudrate = config.presettings.baudrate;
        uart_params.wordlen = config.presettings.wordlen;
        uart_params.parity = config.presettings.parity;
        uart_params.stopbits = config.presettings.stopbits;
    }

    if (res == RES_OK) {
        app_led_set(LED_EVENT_SUCCESS);
        bsp_lcd1602_cprintf("%c: %u,%1u%c%1u", NULL, config.presettings.enable ? 'P' : 'S', uart_params.baudrate,
                                                     uart_params.wordlen, uart_parity_sym[uart_params.parity], 
                                                     uart_params.stopbits);

        uart_params.tx_size = 256;
        uart_params.rx_size = 256;
        uart_params.overflow_isr_cb = uart_overflow_cb;
        uart_params.error_isr_cb = uart_error_cb;

        //res = bsp_uart_init(type, &uart_params);
    } else {
        app_led_set(LED_EVENT_FAILED);
        bsp_lcd1602_cprintf("ALG FAILED", NULL);
    }

    return 0;
}
