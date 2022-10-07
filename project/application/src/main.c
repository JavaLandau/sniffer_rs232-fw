#include "common.h"
#include "stm32f4xx_hal.h"
#include "app_led.h"
#include "bsp_rcc.h"
#include "bsp_lcd1602.h"
#include "bsp_uart.h"
#include "bsp_crc.h"
#include "bsp_button.h"
#include "sniffer_rs232.h"
#include "config.h"
#include "cli.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#define UART_RX_BUFF            (256)
#define IS_UART_ERROR(X)        (uart_flags[X].error || uart_flags[X].overflow)

static char uart_parity_sym[] = {'N', 'E', 'O'};
static char *display_uart_type_str[] = {"CLI", "TX", "RX"};

static bool press_event = false;

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

static void button_cb(enum button_action action)
{
    switch(action) {
    case BUTTON_PRESSED:
        if (!cli_menu_is_started())
            press_event = true;
        else
            cli_menu_exit();
        break;

    case BUTTON_LONG_PRESSED:
        HAL_NVIC_SystemReset();

    default:
        break;
    }
}

static bool button_wait_event(uint32_t tmt)
{
    bool __press_event = false;
    uint32_t tick_start = HAL_GetTick();

    do {
        if (press_event) {
            __press_event = true;
            press_event = false;
            break;
        }
    } while((HAL_GetTick() - tick_start) < tmt);

    return __press_event;
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

    struct button_init_ctx button_init_ctx = {
        .press_delay_ms = 500,
        .press_min_dur_ms = 70,
        .long_press_dur_ms = 1000,
        .button_isr_cb = button_cb,
    };

    res = bsp_button_init(&button_init_ctx);

    if (res != RES_OK) {
        bsp_lcd1602_cprintf("BUTTON ERR %u", NULL, res);
        internal_error(LED_EVENT_COMMON_ERROR);
    }

    res = cli_init();

    if (res != RES_OK) {
        bsp_lcd1602_cprintf("CLI ERR %u", NULL, res);
        internal_error(LED_EVENT_COMMON_ERROR);
    }

    bsp_lcd1602_cprintf("CONFIGURATION", NULL);

    res = cli_menu_start(&config);

    if (res != RES_OK) {
        bsp_lcd1602_cprintf("MENU ERR %u", NULL, res);
        internal_error(LED_EVENT_COMMON_ERROR);
    }

    if (!config.presettings.enable) {
        res = sniffer_rs232_init(&config.alg_config);

        if (res != RES_OK) {
            bsp_lcd1602_cprintf("ALG INIT ERR %u", NULL, res);
            internal_error(LED_EVENT_COMMON_ERROR);
        }
    }

    struct uart_init_ctx uart_params = {0};

    while (!uart_params.baudrate) {
        if (!config.presettings.enable) {
            app_led_set(LED_EVENT_IN_PROCESS);
            bsp_lcd1602_cprintf("ALG PROCESS...", NULL);

            res = sniffer_rs232_calc(&uart_params);

            if (res != RES_OK) {
                bsp_lcd1602_cprintf("ALG ERR %u", NULL, res);
                internal_error(LED_EVENT_COMMON_ERROR);
            }
        } else {
            uart_params.baudrate = config.presettings.baudrate;
            uart_params.wordlen = config.presettings.wordlen;
            uart_params.parity = config.presettings.parity;
            uart_params.stopbits = config.presettings.stopbits;
        }

        if (!uart_params.baudrate) {
            app_led_set(LED_EVENT_FAILED);
            bsp_lcd1602_cprintf("ALG FAILED", NULL);

            while(!button_wait_event(0));
        }
    }

    if (uart_params.baudrate) {
        app_led_set(LED_EVENT_SUCCESS);
        bsp_lcd1602_cprintf("%c: %u,%1u%c%1u", NULL, config.presettings.enable ? 'P' : 'S', uart_params.baudrate,
                                                     uart_params.wordlen, uart_parity_sym[uart_params.parity], 
                                                     uart_params.stopbits);

        uart_params.rx_size = UART_RX_BUFF;
        uart_params.overflow_isr_cb = uart_overflow_cb;
        uart_params.error_isr_cb = uart_error_cb;

        res = bsp_uart_init(BSP_UART_TYPE_RS232_TX, &uart_params);

        if (res != RES_OK) {
            bsp_lcd1602_cprintf("%s INIT ERR %u", NULL, display_uart_type_str[BSP_UART_TYPE_RS232_TX], res);
            internal_error(LED_EVENT_COMMON_ERROR);
        }

        res = bsp_uart_init(BSP_UART_TYPE_RS232_RX, &uart_params);

        if (res != RES_OK) {
            bsp_lcd1602_cprintf("%s INIT ERR %u", NULL, display_uart_type_str[BSP_UART_TYPE_RS232_RX], res);
            internal_error(LED_EVENT_COMMON_ERROR);
        }


        bool error_displayed = false;
        enum uart_type uart_type = BSP_UART_TYPE_RS232_TX;
        enum uart_type prev_uart_type = uart_type;
        uint8_t rx_buff[UART_RX_BUFF] = {0};
        uint16_t rx_len = 0;
        bool started = true;

        bsp_lcd1602_cprintf(NULL, "%s", started ? "STARTED" : "STOPPED");

        while (true) {
            if (button_wait_event(0)) {
                if (error_displayed) {
                    error_displayed = false;
                    memset(uart_flags, 0, sizeof(uart_flags));
                    app_led_set(LED_EVENT_SUCCESS);
                } else {
                    started = !started;

                    if (!started)
                        bsp_uart_stop(BSP_UART_TYPE_CLI);
                    else
                        bsp_uart_start(BSP_UART_TYPE_CLI);
                }

                bsp_lcd1602_cprintf(NULL, "%s", started ? "STARTED" : "STOPPED");
            }

            if (!started)
                continue;

            if (bsp_uart_read(uart_type, rx_buff, &rx_len, 0) == RES_OK) {
                if (uart_type != prev_uart_type) {
                    if (config.txrx_delimiter == RS232_INTERSPCACE_SPACE)
                        cli_trace(" ");
                    else if (config.txrx_delimiter == RS232_INTERSPCACE_NEW_LINE)
                        cli_trace("\r\n");

                    prev_uart_type = uart_type;
                } else {
                    if (config.idle_presence == RS232_INTERSPCACE_SPACE)
                        cli_trace(" ");
                    else if (config.idle_presence == RS232_INTERSPCACE_NEW_LINE)
                        cli_trace("\r\n");
                }

                cli_rs232_trace(uart_type, config.trace_type, rx_buff, rx_len);
            }

            if (!error_displayed) {
                error_displayed = true;
                char err_tx_str[3] = {0};
                char err_rx_str[3] = {0};

                if (uart_flags[BSP_UART_TYPE_RS232_TX].overflow)
                    snprintf(err_tx_str, sizeof(err_tx_str), "OF");
                else if (uart_flags[BSP_UART_TYPE_RS232_TX].error)
                    snprintf(err_tx_str, sizeof(err_tx_str), "%u", uart_flags[BSP_UART_TYPE_RS232_TX].error);

                if (uart_flags[BSP_UART_TYPE_RS232_RX].overflow)
                    snprintf(err_rx_str, sizeof(err_rx_str), "OF");
                else if (uart_flags[BSP_UART_TYPE_RS232_RX].error)
                    snprintf(err_rx_str, sizeof(err_rx_str), "%u", uart_flags[BSP_UART_TYPE_RS232_RX].error);

                if (IS_UART_ERROR(BSP_UART_TYPE_RS232_TX) && IS_UART_ERROR(BSP_UART_TYPE_RS232_RX)) {
                    bsp_lcd1602_cprintf(NULL, "%s%s ERR %s/%s", display_uart_type_str[BSP_UART_TYPE_RS232_TX],
                                                                display_uart_type_str[BSP_UART_TYPE_RS232_RX],
                                                                err_tx_str, err_rx_str);
                } else if (IS_UART_ERROR(BSP_UART_TYPE_RS232_TX) || IS_UART_ERROR(BSP_UART_TYPE_RS232_RX)) {
                    enum uart_type err_uart_type = IS_UART_ERROR(BSP_UART_TYPE_RS232_TX) ? BSP_UART_TYPE_RS232_TX : BSP_UART_TYPE_RS232_RX;

                    bsp_lcd1602_cprintf(NULL, "%s ERR %s", display_uart_type_str[err_uart_type], 
                                                           (err_uart_type == BSP_UART_TYPE_RS232_TX) ? err_tx_str : err_rx_str);
                } else {
                    error_displayed = false;
                }

                if (error_displayed) {
                    if (uart_flags[BSP_UART_TYPE_RS232_TX].overflow || uart_flags[BSP_UART_TYPE_RS232_RX].overflow)
                        app_led_set(LED_EVENT_UART_OVERFLOW);
                    else
                        app_led_set(LED_EVENT_UART_ERROR);
                }
            }

            uart_type = (uart_type == BSP_UART_TYPE_RS232_TX) ? BSP_UART_TYPE_RS232_RX : BSP_UART_TYPE_RS232_TX;
        }

    } else {
        app_led_set(LED_EVENT_FAILED);
        bsp_lcd1602_cprintf("ALG FAILED", NULL);
    }

    return 0;
}
