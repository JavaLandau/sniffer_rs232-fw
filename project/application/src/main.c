#include "common.h"
#include "stm32f4xx_hal.h"
#include "bsp_rcc.h"
#include "bsp_lcd1602.h"
#include "bsp_uart.h"
#include "bsp_led_rgb.h"
#include "sniffer_rs232.h"
#include "cli.h"
#include <stdbool.h>

static void internal_error(void)
{
    bsp_led_rgb_set(255, 0, 0);
    bsp_led_rgb_blink_enable(250, 250);
}

int main()
{
    HAL_Init();

    if (bsp_rcc_main_config_init() != RES_OK)
        HAL_NVIC_SystemReset();

    uint8_t res = bsp_led_rgb_init();

    if (res != RES_OK)
        HAL_NVIC_SystemReset();

    bsp_led_rgb_calibrate(255, 75, 12);

    if( !(DWT->CTRL & 1) )
    {
        CoreDebug->DEMCR |= 0x01000000;
        DWT->CTRL |= 1; // enable the counter
    }

    //uint32_t t1 = DWT->CYCCNT;
    //HAL_Delay(1);
    //uint32_t t2 = DWT->CYCCNT;

    //uint32_t dir = t2 - t1;

    lcd1602_settings_t settings = {
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
        internal_error();

    bsp_lcd1602_printf("Chmoki", " Chmoki");

    /*sniffer_rs232_init();

    struct uart_init_ctx uart_params= {0};
    res = sniffer_rs232_calc(RS232_CALC_TX, &uart_params);*/

    res = cli_init();

    if (res != RES_OK)
        internal_error();

    cli_menu_start();

    return 0;
}
