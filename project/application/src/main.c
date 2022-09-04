#include "common.h"
#include "stm32f4xx_hal.h"
#include "bsp_rcc.h"
#include "bsp_lcd1602.h"
#include "bsp_uart.h"
#include "bsp_led_rgb.h"
#include "sniffer_rs232.h"
#include <stdbool.h>

void test_hardfault(void)
{
    uint8_t value = 0;

    while(1) {
        bsp_led_rgb_set(255 * value, 0, 0);
        value = 1 - value;
        HAL_Delay(1000);
    }
}

static uint8_t __r = 0, __g = 0, __b = 0;

static void __delay_us(uint32_t delay_us)
{
    __IO uint32_t clock_delay = delay_us * (HAL_RCC_GetSysClockFreq() / 8 / 1000000);
    do {
        __NOP();
    } while (--clock_delay);
}

static void overflow_cb(enum uart_type type, void *params)
{
    static bool overflow = false;
    overflow = true;
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

    //res = lcd1602_init(&settings);

    //if (res != RES_OK)
    //    test_hardfault();

    //lcd1602_printf(" Sladkyh snov", " Yanuska");


    //res = led_rgb_set(255, 70, 0);

    //res = led_rgb_set(127, 127, 127);
    //res = led_rgb_set(10, 10, 10);

    /*uint8_t prev_r = 0, prev_g = 0, prev_b = 0;

    while(1) {
        if(prev_r != __r || prev_g != __g || prev_b != __b) {
            prev_r = __r;
            prev_g = __g;
            prev_b = __b;
            
			led_rgb_set(__r, __g, __b);
        }
        HAL_Delay(100);
    }*/

    sniffer_rs232_init();

    struct uart_init_ctx uart_params= {0};
    res = sniffer_rs232_calc(RS232_CALC_TX, &uart_params);

    struct uart_init_ctx uart_init;
    uart_init.baudrate = 115200;
    uart_init.wordlen = BSP_UART_WORDLEN_8;
    uart_init.parity = BSP_UART_PARITY_NONE;
    uart_init.stopbits = BSP_UART_STOPBITS_1;
    uart_init.rx_size = 128;
    uart_init.tx_size = 128;
    uart_init.params = NULL;
    uart_init.error_isr_cb = NULL;
    uart_init.overflow_isr_cb = overflow_cb;

    res = bsp_uart_init(BSP_UART_TYPE_CLI, &uart_init);
    uint8_t rx_buff[128] = {0};

    uint16_t len = 0;
    uint32_t total_rcv = 0;
    static bool stop_rcv = false;

    if (res == RES_OK) {
        while(true) {
            if (!stop_rcv && bsp_uart_read(BSP_UART_TYPE_CLI, rx_buff, &len, 1000) == RES_OK) {
                total_rcv += len;
                bsp_uart_write(BSP_UART_TYPE_CLI, rx_buff, len, 1000);
            }
        }
    }

    return 0;
}
