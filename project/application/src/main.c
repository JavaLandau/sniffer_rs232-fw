/**
\file
\author JavaLandau
\copyright MIT License
\brief Main project file

The file includes main routine of the firmware: start menu of configuration,  
algorithm usage, error handlers and etc.
*/

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

/** 
 * \defgroup application Application
 * \brief Application layer of the firmware
*/

/** 
 * \defgroup main Main
 * \brief Firmware main routine
 * \ingroup application
 * @{
*/

/// Firmware version
#define APP_VERSION             "1.0-RC4"

/// Size of RX buffer to store data received from \ref bsp_uart
#define UART_RX_BUFF            (256)

/** MACRO Flag whether UART errors occured
 * 
 * \param[in] X type of UART, see \ref uart_type
 * \result true if some errors took place, false otherwise
*/
#define IS_UART_ERROR(X)        (uart_flags[X].error || uart_flags[X].overflow)

/// Array of char aliases for \ref uart_parity for output purposes
static const char uart_parity_sym[] = {'N', 'E', 'O'};

/// Array of string aliases for \ref uart_type for output purposes
static const char *display_uart_type_str[] = {"CLI", "TX", "RX"};

/// Flag whether press event on the button is occured
static bool press_event = false;

///UART flags
static struct {
    uint32_t error;                     ///< Mask of UART errors
    bool overflow;                      ///< Flag whether UART RX buffer is overflown before call \ref bsp_uart_read
    bool lin_break;                     ///< Flag whether LIN break detection is occured
} uart_flags[BSP_UART_TYPE_MAX] = {0};

/** Callback for UART LIN break detection
 * 
 * Callback is called from \ref bsp_uart when LIN break is detected
 * 
 * \param[in] type UART type
 * \param[in] params optional parameters
 */
static void uart_lin_break_cb(enum uart_type type, void *params)
{
    if (!UART_TYPE_VALID(type))
        return;

    uart_flags[type].lin_break = true;
}

/** Callback for UART overflow
 * 
 * Callback is called from \ref bsp_uart when overflow of RX buffer is occured
 * 
 * \param[in] type UART type
 * \param[in] params optional parameters
 */
static void uart_overflow_cb(enum uart_type type, void *params)
{
    if (!UART_TYPE_VALID(type))
        return;

    uart_flags[type].overflow = true;
}

/** Callback for UART errors
 * 
 * Callback is called from \ref bsp_uart when UART errors are occured
 * 
 * \param[in] type UART type
 * \param[in] error mask of occured UART errors
 * \param[in] params optional parameters
 */
static void uart_error_cb(enum uart_type type, uint32_t error, void *params)
{
    if (!UART_TYPE_VALID(type))
        return;

    uart_flags[type].error |= error;
}

/** Callback for button actions
 * 
 * Callback is called from \ref bsp_button when actions on  
 * the button are occured. Algorithm of the callback is the following:
 * 
 * 1. Action \ref BUTTON_PRESSED occured:  
 * If menu is started or waiting to be started - skip menu start  
 * otherwise - start/stop toggle of UART reception from RS-232 channels
 * 
 * 2. Action \ref BUTTON_LONG_PRESSED  
 * Software reset of the chip in any cases
 * 
 * \param[in] action BUTTON action, see \ref button_action
 */
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

/** Wait for press event
 * 
 * The function waits when \ref BUTTON_PRESSED occured, using \ref press_event  
 * \ref press_event is cleared if was set
 * 
 * \param[in] tmt timeout in ms for event waiting
 * \return true if event occured, false otherwise
 */
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

/** Routine for internal error
 * 
 * The function calls when occured errors on the firmware  
 * do not let it working further. The function uses politics of LED signaling
 * \warning The function is firmware dead end and reset is needed to start firmware
 * 
 * \param[in] led_event politics of LED signaling
 */
static void internal_error(enum led_event led_event)
{
    app_led_set(led_event);

    while(1);
}

/** Main routine of the firmware 
 * 
 * \return NOT used
*/
int main()
{
    /* Basic initialization */
    HAL_Init();

    if (bsp_rcc_main_config_init() != RES_OK)
        HAL_NVIC_SystemReset();

    /* BSP LED & CRC initialization */
    uint8_t res = app_led_init();

    if (res != RES_OK)
        HAL_NVIC_SystemReset();

    res = bsp_crc_init();

    if (res != RES_OK)
        internal_error(LED_EVENT_CRC_ERROR);

    /* Read/initialization of the device configuration from internal flash */
    struct flash_config config;

    if (config_read(&config) != RES_OK) {
        struct flash_config flash_config_default = FLASH_CONFIG_DEFAULT();
        config = flash_config_default;
        res = config_save(&config);

        if (res != RES_OK)
            internal_error(LED_EVENT_FLASH_ERROR);
    }

    /* LCD1602 initialization */
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

    /* Button initialization */
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

    /* Initialization of CLI */
    res = cli_init();

    if (res != RES_OK) {
        bsp_lcd1602_cprintf("CLI ERR %u", NULL, res);
        internal_error(LED_EVENT_COMMON_ERROR);
    }

    cli_terminal_reset();

    /* Welcome stage */
    bsp_lcd1602_cprintf("SNIFFER RS-232", "V.%s", APP_VERSION);

    cli_trace("**********************************************************\r\n");
    cli_trace("**********************SNIFFER RS-232**********************\r\n");
    cli_trace("**********************************************************\r\n");
    cli_trace("Version: %s\r\n", APP_VERSION);
    cli_trace("Build date: %s %s\r\n", __DATE__, __TIME__);

    bool is_pressed = false;
    cli_welcome("Press any key to start menu or push button to skip", 5, &press_event, &is_pressed);
    cli_terminal_reset();

    press_event = false;
    bsp_lcd1602_display_clear();

    /* Stage of configuration menu */
    if (is_pressed) {
        bsp_lcd1602_cprintf("CONFIGURATION", NULL);

        res = cli_menu_start(&config);

        if (res != RES_OK) {
            bsp_lcd1602_cprintf("MENU ERR %u", NULL, res);
            internal_error(LED_EVENT_COMMON_ERROR);
        }
    }

    if (!config.presettings.enable) {
        struct sniffer_rs232_config alg_config = config.alg_config;
        res = sniffer_rs232_init(&alg_config);

        if (res != RES_OK) {
            bsp_lcd1602_cprintf("ALG INIT ERR %u", NULL, res);
            internal_error(LED_EVENT_COMMON_ERROR);
        }
    }

    struct uart_init_ctx uart_params = {0};
    bool presettings_enabled = config.presettings.enable;

    /* Algorithm stage */
    while (!uart_params.baudrate) {
        if (!config.presettings.enable) {
            app_led_set(LED_EVENT_IN_PROCESS);
            cli_trace("Algorithm is in process...\r\n");
            bsp_lcd1602_cprintf("ALG PROCESS...", NULL);

            res = sniffer_rs232_calc(&uart_params);

            if (res != RES_OK) {
                bsp_lcd1602_cprintf("ALG ERR %u", NULL, res);
                cli_trace("Algorithm error %u\r\n", res);
                internal_error(LED_EVENT_COMMON_ERROR);
            } else if (uart_params.baudrate) {
                if (config.save_to_presettings) {
                    config.presettings.enable = true;
                    config.presettings.lin_enabled = uart_params.lin_enabled;
                    config.presettings.baudrate = uart_params.baudrate;
                    config.presettings.wordlen = uart_params.wordlen;
                    config.presettings.parity = uart_params.parity;
                    config.presettings.stopbits = uart_params.stopbits;

                    res = config_save(&config);

                    if (res != RES_OK) {
                        cli_trace("Failed to save to presettings: %u\r\n", res);
                        bsp_lcd1602_cprintf("FLASH ERR %u", NULL, res);
                        internal_error(LED_EVENT_FLASH_ERROR);
                    }
                }
            }
        } else {
            uart_params.lin_enabled = config.presettings.lin_enabled;
            uart_params.baudrate = config.presettings.baudrate;
            uart_params.wordlen = config.presettings.wordlen;
            uart_params.parity = config.presettings.parity;
            uart_params.stopbits = config.presettings.stopbits;
        }

        if (!uart_params.baudrate) {
            app_led_set(LED_EVENT_FAILED);
            cli_trace("Algorithm failed, waiting for button action\r\n");
            bsp_lcd1602_cprintf("ALG FAILED", NULL);

            while(!button_wait_event(0));
        }
    }

    /* Monitoring stage */
    app_led_set(LED_EVENT_SUCCESS);
    cli_trace("Start to monitoring...\r\n");

    if (!uart_params.lin_enabled) {
        bsp_lcd1602_cprintf("%c: %u,%1u%c%1u", NULL, presettings_enabled ? 'P' : 'S', uart_params.baudrate,
                                                     uart_params.wordlen, uart_parity_sym[uart_params.parity], 
                                                     uart_params.stopbits);
    } else {
        bsp_lcd1602_cprintf("%c: %u,LIN", NULL, presettings_enabled ? 'P' : 'S', uart_params.baudrate);
    }

    uart_params.rx_size = UART_RX_BUFF;
    uart_params.overflow_isr_cb = uart_overflow_cb;
    uart_params.error_isr_cb = uart_error_cb;
    uart_params.lin_break_isr_cb = uart_lin_break_cb;

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
    uint16_t rx_buff[UART_RX_BUFF] = {0};
    uint16_t rx_len = 0;
    bool started = true;

    uint8_t prev_rs232_tx_error = 0;
    uint8_t prev_rs232_rx_error = 0;

    bsp_lcd1602_cprintf(NULL, "%s", started ? "STARTED" : "STOPPED");

    /* Routine of the monitoring */
    while (true) {
        if (button_wait_event(0)) {
            if (error_displayed) {
                error_displayed = false;
                memset(uart_flags, 0, sizeof(uart_flags));
                app_led_set(LED_EVENT_SUCCESS);
            } else {
                started = !started;

                if (!started) {
                    bsp_uart_stop(BSP_UART_TYPE_RS232_TX);
                    bsp_uart_stop(BSP_UART_TYPE_RS232_RX);
                } else {
                    bsp_uart_start(BSP_UART_TYPE_RS232_TX);
                    bsp_uart_start(BSP_UART_TYPE_RS232_RX);
                }
            }

            bsp_lcd1602_cprintf(NULL, "%s", started ? "STARTED" : "STOPPED");
        }

        bsp_uart_read(BSP_UART_TYPE_CLI, NULL, NULL, 0);

        if (!started)
            continue;

        for (enum uart_type type = BSP_UART_TYPE_CLI; type < BSP_UART_TYPE_MAX; type++) {
            if (!bsp_uart_is_started(type) && bsp_uart_rx_buffer_is_empty(type))
                bsp_uart_start(type);
        }

        if (bsp_uart_read(uart_type, rx_buff, &rx_len, 0) == RES_OK) {
            bool lin_break = uart_flags[uart_type].lin_break ? 1 : 0;

            if (lin_break)
                uart_flags[uart_type].lin_break = false;

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

            cli_rs232_trace(uart_type, config.trace_type, &rx_buff[lin_break], rx_len - lin_break, lin_break);
        }

        bool error_changed = (prev_rs232_tx_error != uart_flags[BSP_UART_TYPE_RS232_TX].error) || 
                             (prev_rs232_rx_error != uart_flags[BSP_UART_TYPE_RS232_RX].error);

        prev_rs232_tx_error = uart_flags[BSP_UART_TYPE_RS232_TX].error;
        prev_rs232_rx_error = uart_flags[BSP_UART_TYPE_RS232_RX].error;

        if (!error_displayed || error_changed) {
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
}

/** @} */