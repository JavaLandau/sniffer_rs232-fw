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

    struct flash_config config = {0};

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

    cli_menu_start(&config);

    res = sniffer_rs232_init(&config.alg_config);

    if (res != RES_OK) {
        bsp_lcd1602_cprintf("ALG ERROR", NULL);
        internal_error(LED_EVENT_COMMON_ERROR);
    }

    bsp_lcd1602_cprintf("ALG PROCESS...", NULL);

    //struct uart_init_ctx uart_params = {0};
    //uint8_t sniffer_rs232_calc(enum rs232_channel_type channel_type, &uart_params);

    return 0;
}
