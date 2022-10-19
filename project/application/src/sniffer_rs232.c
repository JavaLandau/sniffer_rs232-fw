/**
\file
\author JavaLandau
\copyright MIT License
\brief Algorithm of Sniffer RS-232

The file includes recognizing algorithm of RS-232 parameters
*/

#include "sniffer_rs232.h"
#include "bsp_gpio.h"
#include "bsp_rcc.h"
#include <stdbool.h>
#include <string.h>

/** 
 * \defgroup sniffer_rs232 Algorithm of Sniffer RS-232
 * \brief Module of recognizing algorithm of Sniffer RS-232
 * 
 * Algorithm consists of two parts:  
 * 1. Baudrate part - when baudrate calculated in EXTI mode
 * 2. Parameter part - when other UART parameters (word length, parity type) calculated in UART mode
 * \ingroup application
 * @{
*/

/// Size of buffers \ref tx_buffer & \ref rx_buffer
#define BUFFER_SIZE             (512)

/// Size of receive buffer used in \ref bsp_uart
#define UART_BUFF_SIZE          (128)

/** Minimal ratio between maximum and minimum widths of lower level  
 * on the RS-232 lines to make decision about LIN break existence */
#define LIN_BREAK_MIN_LEN       (10)

/** STM32 HAL TIM instance for timer used to count widths of lower level  
 *  on the RS-232 lines */
static TIM_HandleTypeDef alg_tim = {.Instance = TIM6};

/** STM32 HAL EXTI instance used to detect falling & rising edges  
 * of signals on the RS-232 TX line */
static EXTI_HandleTypeDef hexti1 = {.Line = EXTI_LINE_3};

/** STM32 HAL EXTI instance used to detect falling & rising edges  
 * of signals on the RS-232 RX line */
static EXTI_HandleTypeDef hexti2 = {.Line = EXTI_LINE_5};

/// Current filling level of \ref tx_buffer
static uint32_t tx_cnt = 0;

/// Current filling level of \ref rx_buffer
static uint32_t rx_cnt = 0;

/** Buffer storing timestamps of falling/rising edges of signal  
 * on the RS-232 TX line */
static uint32_t tx_buffer[BUFFER_SIZE] = {0};

/** Buffer storing timestamps of falling/rising edges of signal  
 * on the RS-232 RX line */
static uint32_t rx_buffer[BUFFER_SIZE] = {0};

/** List of baudrates which can be detected by the algorithm */
static const uint32_t baudrates_list[] = {921600, 460800, 230400, 115200, 57600, 38400, 19200, 9600, 4800, 2400};

/** Context of check of hypothesis */
struct hyp_check_ctx {
    uint32_t error_parity_cnt;      ///< Count of UART parity errors, \see BSP_UART_ERROR_PE
    uint32_t error_frame_cnt;       ///< Count of UART frame errors, \see BSP_UART_ERROR_FE
    uint32_t valid_cnt;             ///< Count of successfully received bytes over UART
    bool overflow;                  ///< Flag whether overflow of receive buffer occured
};

/** Context of baudrate calculation */
struct baud_calc_ctx {
    uint32_t    *cnt;               ///< Pointer to \ref tx_cnt or \ref rx_cnt
    uint32_t    *buffer;            ///< Pointer to \ref tx_buffer or \ref rx_buffer
    uint32_t    idx;                ///< Current position of \ref buffer for analysis
    uint32_t    min_len_bit;        ///< Minimum detected width of lower level on RS-232 line, valid over \ref baudrates_list
    uint32_t    max_len_bit;        ///< Maximum detected width of lower level on RS-232 line
    uint32_t    baudrate;           ///< Calculated baudrate in bods
    bool        toggle_bit;         ///< Flag showing current level on RS-232 line: true - upper one, false - lower one
    bool        lin_detected;       ///< Flag whether LIN break is detected
    bool        done;               ///< Flag whether baudrate calculation is finished
};

/** Context of hypothesis */
struct hyp_ctx {
    /** Size of UART frame in bits */
    enum uart_wordlen wordlen;
    /** Parity type */
    enum uart_parity parity;
    /** Next number of hypothesis from \ref hyp_seq if count of  
     * UART frame errors reach sniffer_rs232_config::uart_error_count */
    uint8_t jump;
};

/// Sequence of hypotheses regarding UART parameters of RS-232 channels
static const struct hyp_ctx hyp_seq[] = {
    {BSP_UART_WORDLEN_8, BSP_UART_PARITY_EVEN, 3},
    {BSP_UART_WORDLEN_8, BSP_UART_PARITY_ODD, 3},
    {BSP_UART_WORDLEN_8, BSP_UART_PARITY_NONE, 3},
    {BSP_UART_WORDLEN_9, BSP_UART_PARITY_EVEN, 0},
    {BSP_UART_WORDLEN_9, BSP_UART_PARITY_ODD, 0},
    {BSP_UART_WORDLEN_9, BSP_UART_PARITY_NONE, 0}
};

/// Local copy of algorithm settings
static struct sniffer_rs232_config config;

/** STM32 HAL TIM MSP initialization
 * 
 * \param[in] htim STM32 HAL TIM instance, should equal to \ref alg_tim
 */
static void __sniffer_rs232_tim_msp_init(TIM_HandleTypeDef* htim)
{
    if (htim->Instance != TIM6)
        return;

    __HAL_RCC_TIM6_CLK_ENABLE();
}

/** STM32 HAL TIM MSP deinitialization
 * 
 * \param[in] htim STM32 HAL TIM instance, should equal to \ref alg_tim
 */
static void __sniffer_rs232_tim_msp_deinit(TIM_HandleTypeDef* htim)
{
    if (htim->Instance != TIM6)
        return;

    __HAL_RCC_TIM6_CLK_DISABLE();
}

/** Baudrate calculation by width of a bit
 * 
 * The function calculates whether width of a bit corresponds one of the  
 * baudrate from \ref baudrates_list
 * 
 * \param[in] len_bit width of a bit
 * \return baudrate value in bods on success, 0 otherwise
 */
static uint32_t __sniffer_rs232_baudrate_get(uint32_t len_bit)
{
    const float tolerance = (float)config.baudrate_tolerance / 100.0f;

    if (!len_bit)
        return 0;

    float calc_baud = (float)(1000000 * config.baudrate_tolerance) / (float)len_bit;

    uint32_t i = 0;
    for (; i < ARRAY_SIZE(baudrates_list); i++) {
        float baud = (float)baudrates_list[i];

        if ((calc_baud >= (1 - tolerance) * baud) && ((calc_baud <= (1 + tolerance) * baud)))
            break;
    }

    if (i != ARRAY_SIZE(baudrates_list))
        return baudrates_list[i];

    return 0;
}

/** Initialization of baudrate part of the algorithm
 * 
 * The function makes MSP EXTI initialization and waits for IDLE  
 * state on the appropriate RS-232 line
 * 
 * \param[in] gpiox GPIO port of \a pin used as EXTI
 * \param[in] pin GPIO pin used as EXTI
 * \param[in] irq_type NVIC IRQ type
 * \return \ref RES_OK on success error otherwise
 */
static uint8_t __sniffer_rs232_line_baudrate_calc_init(GPIO_TypeDef* gpiox, uint16_t pin, IRQn_Type irq_type)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = pin;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(gpiox, &GPIO_InitStruct);

    const uint32_t gpio_wait_tmt = 3000;

    uint8_t res = RES_OK;
    uint32_t start_time = HAL_GetTick();
    while(true) {
        if ((HAL_GetTick() - start_time) > gpio_wait_tmt) {
            res = RES_TIMEOUT;
            break;
        }

        if (BSP_GPIO_PORT_READ(gpiox, pin)) {
            HAL_NVIC_ClearPendingIRQ(irq_type);
            HAL_NVIC_EnableIRQ(irq_type);
            break;
        }
    }

    return res;
}

/** Baudrate calculation on the RS-232 line
 * 
 * The function calculates baudrate on one RS-232 line 
 * 
 * \param[in,out] ctx context of baudrate calculation
 */
static void __sniffer_rs232_line_baudrate_calc(struct baud_calc_ctx *ctx)
{
    if (!ctx)
        return;

    uint8_t res = RES_OK;
    const uint32_t uart_idle_tmt = 1000;

    for(; ctx->idx < BUFFER_SIZE; ctx->idx += 2) {
        uint32_t start_time = HAL_GetTick();
        while (*ctx->cnt < (ctx->idx + 2)) {
            if ((HAL_GetTick() - start_time) > uart_idle_tmt) {
                res = RES_TIMEOUT;
                break;
            }
        }

        if (res != RES_OK)
            break;

        if (!ctx->toggle_bit) {
            uint32_t len_bit = (uint16_t)(ctx->buffer[ctx->idx + 1] - ctx->buffer[ctx->idx]);
            if (len_bit < ctx->min_len_bit) {
                uint32_t baudrate = __sniffer_rs232_baudrate_get(len_bit);
                if (baudrate) {
                    ctx->min_len_bit = len_bit;
                    ctx->baudrate = baudrate;
                }
            } else if (len_bit > ctx->max_len_bit) {
                ctx->max_len_bit = len_bit;

                if (!ctx->lin_detected)
                    ctx->lin_detected = (ctx->max_len_bit / ctx->min_len_bit) > LIN_BREAK_MIN_LEN;
            }
        }

        ctx->toggle_bit = !ctx->toggle_bit;
    }

    ctx->done = (ctx->idx >= (4 * config.min_detect_bits));
}

/** Baudrate part of the algorithm
 * 
 * The function calculates baudrate on RS-232 TX/RX lines according to \a channel_type
 * 
 * \param[in] channel_type RS-232 channel detection type
 * \param[out] baudrate calculated baudrate
 * \param[out] lin_detected flag whether LIN protocol is detected
 * \return \ref RES_OK on success error otherwise
 */
static uint8_t __sniffer_rs232_baudrate_calc(enum rs232_channel_type channel_type, uint32_t *baudrate, bool *lin_detected)
{
    if (!baudrate || !lin_detected || !RS232_CHANNEL_TYPE_VALID(channel_type))
        return RES_INVALID_PAR;

    const uint32_t uart_max_exec_tmt = 1000 * config.exec_timeout;

    /* Initialization */
    memset(tx_buffer, 0, sizeof(tx_buffer));
    memset(rx_buffer, 0, sizeof(rx_buffer));
    tx_cnt = rx_cnt = 0;

    struct baud_calc_ctx tx_ctx = {.cnt = &tx_cnt, .buffer = tx_buffer, .idx = 0, .min_len_bit = UINT32_MAX,
                                   .max_len_bit = 0, .baudrate = 0, .toggle_bit = false, .lin_detected = false, .done = false};

    struct baud_calc_ctx rx_ctx = {.cnt = &rx_cnt, .buffer = rx_buffer, .idx = 0, .min_len_bit = UINT32_MAX,
                                   .max_len_bit = 0, .baudrate = 0, .toggle_bit = false, .lin_detected = false, .done = false};

    uint32_t res = RES_OK;

    if (channel_type != RS232_CHANNEL_RX) {
        res = __sniffer_rs232_line_baudrate_calc_init(GPIOA, GPIO_PIN_3, EXTI3_IRQn);

        if (res != RES_OK)
            return res;
    }

    if (channel_type != RS232_CHANNEL_TX) {
        res = __sniffer_rs232_line_baudrate_calc_init(GPIOC, GPIO_PIN_5, EXTI9_5_IRQn);

        if (res != RES_OK)
            return res;
    }

    bool finish_flag = false;
    uint32_t calc_baudrate = 0;
    uint32_t start_exec_time = HAL_GetTick();

    /* Baudrate calculation */
    while (true) {
        if ((HAL_GetTick() - start_exec_time) > uart_max_exec_tmt) {
            res = RES_TIMEOUT;
            break;
        }

        if (res != RES_OK)
            break;

        /* TX line */
        if (channel_type != RS232_CHANNEL_RX && !tx_ctx.done) {
            __sniffer_rs232_line_baudrate_calc(&tx_ctx);
        }

        /* RX line */
        if (channel_type != RS232_CHANNEL_TX && !rx_ctx.done) {
            __sniffer_rs232_line_baudrate_calc(&rx_ctx);
        }

        /* Result processing */
        struct baud_calc_ctx *ctx = &rx_ctx;
        switch (channel_type) {
        case RS232_CHANNEL_TX:
            ctx = &tx_ctx;
        case RS232_CHANNEL_RX:
            if (ctx->done) {
                calc_baudrate = ctx->baudrate;
                finish_flag = true;
            }
            break;

        case RS232_CHANNEL_ANY:
            if (tx_ctx.done || rx_ctx.done) {
                calc_baudrate = tx_ctx.baudrate ? tx_ctx.baudrate : rx_ctx.baudrate;
                finish_flag = (calc_baudrate != 0) || (tx_ctx.done && rx_ctx.done);
            }
            break;

        case RS232_CHANNEL_ALL:
            if (tx_ctx.done && rx_ctx.done) {
                if (tx_ctx.baudrate == rx_ctx.baudrate)
                    calc_baudrate = tx_ctx.baudrate;

                finish_flag = true;
            }
            break;

        default:
            break;
        }

        if (finish_flag)
            break;
    }

    HAL_NVIC_DisableIRQ(EXTI3_IRQn);
    HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);

    *baudrate = calc_baudrate;
    *lin_detected = tx_ctx.lin_detected || rx_ctx.lin_detected;

    return res;
}

/** Callback for UART overflow
 * 
 * Callback is called from \ref bsp_uart when overflow of RX buffer is occured  
 * If call occured the algorithm is terminated with fail
 * 
 * \param[in] type UART type
 * \param[in] params optional parameters, containing check context of the current hypothesis \ref hyp_check_ctx
 */
static void __sniffer_rs232_uart_overflow_cb(enum uart_type type, void *params)
{
    struct hyp_check_ctx *check_ctx = (struct hyp_check_ctx*)params;
    check_ctx->overflow = true;
}

/** Callback for UART errors
 * 
 * Callback is called from \ref bsp_uart when UART errors are occured  
 * Callback counts UART errors into check context of the current hypothesis \ref hyp_check_ctx
 * 
 * \param[in] type UART type
 * \param[in] error mask of occured UART errors
 * \param[in] params optional parameters, containing check context of the current hypothesis \ref hyp_check_ctx
 */
static void __sniffer_rs232_uart_error_cb(enum uart_type type, uint32_t error, void *params)
{
    struct hyp_check_ctx *check_ctx = (struct hyp_check_ctx*)params;

    if (error & BSP_UART_ERROR_PE)
        check_ctx->error_parity_cnt++;

    if (error & BSP_UART_ERROR_FE)
        check_ctx->error_frame_cnt++;

    bsp_uart_start(type);
}

/** Parameter part of the algorithm
 * 
 * The function calculates other parameters of UART on RS-232 lines
 * 
 * \param[in] channel_type RS-232 channel detection type
 * \param[in] baudrate baudrate in bods on RS-232 lines
 * \param[out] hyp_num number of approved hypothesis from \ref hyp_seq on success, -1 otherwise
 * \return \ref RES_OK on success error otherwise
 */
static uint8_t __sniffer_rs232_params_calc(enum rs232_channel_type channel_type, uint32_t baudrate, int8_t *hyp_num)
{
    if (!baudrate || !RS232_CHANNEL_TYPE_VALID(channel_type) || !hyp_num)
        return RES_INVALID_PAR;

    *hyp_num = -1;
    int8_t __hyp_num = 0;
    uint8_t res = RES_OK;

    struct hyp_check_ctx tx_check_ctx = {0};
    struct hyp_check_ctx rx_check_ctx = {0};

    do {
        /* Initialization */
        struct uart_init_ctx init_ctx = {0};
        init_ctx.baudrate = baudrate;
        init_ctx.wordlen = hyp_seq[__hyp_num].wordlen;
        init_ctx.parity = hyp_seq[__hyp_num].parity;
        init_ctx.rx_size = UART_BUFF_SIZE;
        init_ctx.stopbits = BSP_UART_STOPBITS_1;
        init_ctx.error_isr_cb = __sniffer_rs232_uart_error_cb;
        init_ctx.overflow_isr_cb = __sniffer_rs232_uart_overflow_cb;

        if (channel_type != RS232_CHANNEL_RX) {
            memset(&tx_check_ctx, 0, sizeof(tx_check_ctx));
            init_ctx.params = &tx_check_ctx;

            res = bsp_uart_init(BSP_UART_TYPE_RS232_TX, &init_ctx);

            if (res != RES_OK)
                break;
        }

        if (channel_type != RS232_CHANNEL_TX) {
            memset(&rx_check_ctx, 0, sizeof(rx_check_ctx));
            init_ctx.params = &rx_check_ctx;

            res = bsp_uart_init(BSP_UART_TYPE_RS232_RX, &init_ctx);

            if (res != RES_OK)
                break;
        }

        bool finish_flag = false;
        const uint32_t uart_max_exec_tmt = 1000 * config.exec_timeout;
        uint32_t start_exec_time = HAL_GetTick();

        /* Calculation */
        while (true) {
            if ((HAL_GetTick() - start_exec_time) > uart_max_exec_tmt) {
                res = RES_TIMEOUT;
                break;
            }

            uint16_t len = 0;
            if (channel_type != RS232_CHANNEL_RX && bsp_uart_read(BSP_UART_TYPE_RS232_TX, NULL, &len, 0) == RES_OK)
                tx_check_ctx.valid_cnt += len;

            if (channel_type != RS232_CHANNEL_TX && bsp_uart_read(BSP_UART_TYPE_RS232_RX, NULL, &len, 0) == RES_OK)
                rx_check_ctx.valid_cnt += len;

            if (tx_check_ctx.overflow || rx_check_ctx.overflow) {
                res = RES_OVERFLOW;
                break;
            }

            bool error_parity_exceed = (tx_check_ctx.error_parity_cnt >= config.uart_error_count || rx_check_ctx.error_parity_cnt >= config.uart_error_count);
            bool error_frame_exceed = (tx_check_ctx.error_frame_cnt >= config.uart_error_count || rx_check_ctx.error_frame_cnt >= config.uart_error_count);

            if (error_frame_exceed || error_parity_exceed) {
                if (error_frame_exceed)
                    __hyp_num = hyp_seq[__hyp_num].jump;
                else
                    __hyp_num = (__hyp_num == (ARRAY_SIZE(hyp_seq) - 1)) ? 0 : (__hyp_num + 1);
                break;
            }

            /* Result processing */
            switch (channel_type) {
            case RS232_CHANNEL_TX:
                finish_flag = (tx_check_ctx.valid_cnt >= config.valid_packets_count);
                break;

            case RS232_CHANNEL_RX:
                finish_flag = (rx_check_ctx.valid_cnt >= config.valid_packets_count);
                break;

            case RS232_CHANNEL_ANY:
                finish_flag = (tx_check_ctx.valid_cnt >= config.valid_packets_count || rx_check_ctx.valid_cnt >= config.valid_packets_count);
                break;

            case RS232_CHANNEL_ALL:
                finish_flag = (tx_check_ctx.valid_cnt >= config.valid_packets_count && rx_check_ctx.valid_cnt >= config.valid_packets_count);
                break;

            default:
                break;
            }

            if (finish_flag)
                break;
        }

        if (res != RES_OK)
            break;

        if (channel_type != RS232_CHANNEL_RX) {
            res = bsp_uart_stop(BSP_UART_TYPE_RS232_TX);

            if (res != RES_OK)
                break;
        }

        if (channel_type != RS232_CHANNEL_TX) {
            res = bsp_uart_stop(BSP_UART_TYPE_RS232_RX);

            if (res != RES_OK)
                break;
        }

        if (finish_flag) {
            *hyp_num = __hyp_num;
            break;
        }
    } while(__hyp_num);

    /* Deinitialization */
    if (channel_type != RS232_CHANNEL_RX) {
        res = bsp_uart_deinit(BSP_UART_TYPE_RS232_TX);

        if (res != RES_OK)
            return res;
    }

    if (channel_type != RS232_CHANNEL_TX) {
        res = bsp_uart_deinit(BSP_UART_TYPE_RS232_RX);

        if (res != RES_OK)
            return res;
    }

    return res;
}

/* Valid value range of items from algorithm settings, see header file for details */
uint32_t sniffer_rs232_config_item_range(uint32_t shift, bool is_min)
{
    struct sniffer_rs232_config *__config = 0;
    if (shift == (uint32_t)&__config->valid_packets_count)
        return is_min ? 1 : UINT32_MAX;
    else if (shift == (uint32_t)&__config->uart_error_count)
        return is_min ? 1 : UINT32_MAX;
    else if (shift == (uint32_t)&__config->baudrate_tolerance)
        return is_min ? 1 : 100;
    else if (shift == (uint32_t)&__config->min_detect_bits)
        return is_min ? 1 : (BUFFER_SIZE / 4);
    else if (shift == (uint32_t)&__config->exec_timeout)
        return is_min ? 1 : UINT32_MAX;
    else if (shift == (uint32_t)&__config->calc_attempts)
        return is_min ? 1 : UINT32_MAX;

    return 0;
}

/* Check algorithm settings, see header file for details */
bool sniffer_rs232_config_check(struct sniffer_rs232_config *__config)
{
    if (!__config)
        return false;

    if (!RS232_CHANNEL_TYPE_VALID(__config->channel_type))
        return false;

    if (!SNIFFER_RS232_CFG_PARAM_IS_VALID(valid_packets_count, __config->valid_packets_count))
        return false;

    if (!SNIFFER_RS232_CFG_PARAM_IS_VALID(uart_error_count, __config->uart_error_count))
        return false;

    if (!SNIFFER_RS232_CFG_PARAM_IS_VALID(baudrate_tolerance, __config->baudrate_tolerance))
        return false;

    if (!SNIFFER_RS232_CFG_PARAM_IS_VALID(min_detect_bits, __config->min_detect_bits))
        return false;

    if (!SNIFFER_RS232_CFG_PARAM_IS_VALID(exec_timeout, __config->exec_timeout))
        return false;

    if (!SNIFFER_RS232_CFG_PARAM_IS_VALID(calc_attempts, __config->calc_attempts))
        return false;

    return true;
}

/* Algorithm initialization, see header file for details */
uint8_t sniffer_rs232_init(struct sniffer_rs232_config *__config)
{
    if (!sniffer_rs232_config_check(__config))
        return RES_INVALID_PAR;

    config = *__config;

    /* RCC GPIO init */
    if (__HAL_RCC_GPIOA_IS_CLK_DISABLED())
        __HAL_RCC_GPIOA_CLK_ENABLE();

    if (__HAL_RCC_GPIOC_IS_CLK_DISABLED())
        __HAL_RCC_GPIOC_CLK_ENABLE();

    /* EXTI configuration */
    EXTI_ConfigTypeDef exti_config = {0};

    exti_config.Line            = hexti1.Line;
    exti_config.Mode            = EXTI_MODE_INTERRUPT;
    exti_config.Trigger         = EXTI_TRIGGER_RISING_FALLING;
    exti_config.GPIOSel         = EXTI_GPIOA;

    HAL_EXTI_SetConfigLine(&hexti1, &exti_config);
    HAL_EXTI_ClearPending(&hexti1, EXTI_TRIGGER_RISING_FALLING);

    exti_config.Line            = hexti2.Line;
    exti_config.GPIOSel         = EXTI_GPIOC;

    HAL_EXTI_SetConfigLine(&hexti2, &exti_config);
    HAL_EXTI_ClearPending(&hexti2, EXTI_TRIGGER_RISING_FALLING);

    /* NVIC configuration */
    HAL_NVIC_ClearPendingIRQ(EXTI3_IRQn);
    HAL_NVIC_SetPriority(EXTI3_IRQn, 4, 0);

    HAL_NVIC_ClearPendingIRQ(EXTI9_5_IRQn);
    HAL_NVIC_SetPriority(EXTI9_5_IRQn, 4, 0);

    /* Timer init & start */
    HAL_TIM_RegisterCallback(&alg_tim, HAL_TIM_BASE_MSPINIT_CB_ID, __sniffer_rs232_tim_msp_init);
    HAL_TIM_RegisterCallback(&alg_tim, HAL_TIM_BASE_MSPDEINIT_CB_ID, __sniffer_rs232_tim_msp_deinit);

    alg_tim.Init.Prescaler = bsp_rcc_apb_timer_freq_get(alg_tim.Instance) / (1000000 * config.baudrate_tolerance) - 1;
    alg_tim.Init.Period = UINT16_MAX;
    alg_tim.Init.CounterMode = TIM_COUNTERMODE_UP;
    alg_tim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    alg_tim.Init.RepetitionCounter = 0;
    alg_tim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&alg_tim) != HAL_OK)
        return RES_NOK;

    if (HAL_TIM_Base_Start(&alg_tim) != HAL_OK)
        return RES_NOK;

    return RES_OK;
}

/* Algorithm deinitialization, see header file for details */
uint8_t sniffer_rs232_deinit(void)
{
    if (HAL_TIM_Base_Stop(&alg_tim) != HAL_OK)
        return RES_NOK;

    if (HAL_TIM_Base_DeInit(&alg_tim) != HAL_OK)
        return RES_NOK;

    HAL_NVIC_DisableIRQ(EXTI3_IRQn);
    HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);

    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_3);
    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_5);

    memset(tx_buffer, 0, sizeof(tx_buffer));
    memset(rx_buffer, 0, sizeof(rx_buffer));
    tx_cnt = rx_cnt = 0;

    return RES_OK;
}

/* Algorithm calculation, see header file for details */
uint8_t sniffer_rs232_calc(struct uart_init_ctx *uart_params)
{
    if (!uart_params)
        return RES_INVALID_PAR;

    uint8_t res = RES_OK;
    memset(uart_params, 0, sizeof(struct uart_init_ctx));

    for (uint8_t i = 0; i < config.calc_attempts; i++) {
        uint32_t baudrate = 0;
        bool lin_detected = false;
        res = __sniffer_rs232_baudrate_calc(config.channel_type, &baudrate, &lin_detected);

        if (res != RES_OK)
            break;

        if (!baudrate)
            continue;

        if (config.lin_detection && lin_detected) {
            uart_params->baudrate = baudrate;
            uart_params->lin_enabled = true;
            uart_params->wordlen = BSP_UART_WORDLEN_8;
            uart_params->parity = BSP_UART_PARITY_NONE;
            uart_params->stopbits = BSP_UART_STOPBITS_1;
            break;
        }

        int8_t hyp_num = 0;
        res = __sniffer_rs232_params_calc(config.channel_type, baudrate, &hyp_num);

        if (res != RES_OK)
            break;

        if (hyp_num < 0)
            continue;

        uart_params->baudrate = baudrate;
        uart_params->wordlen = hyp_seq[hyp_num].wordlen;
        uart_params->parity = hyp_seq[hyp_num].parity;
        uart_params->stopbits = BSP_UART_STOPBITS_1;

        break;
    }

    return res;
}

/** NVIC IRQ EXTI3 handler
 * 
 * Handler is used to fill in \ref tx_buffer
*/
void EXTI3_IRQHandler(void)
{
    tx_buffer[tx_cnt++] = alg_tim.Instance->CNT;

    if (tx_cnt == BUFFER_SIZE)
        HAL_NVIC_DisableIRQ(EXTI3_IRQn);

    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_3);
}

/** NVIC IRQ EXTI5 handler
 * 
 * Handler is used to fill in \ref rx_buffer
*/
void EXTI9_5_IRQHandler(void)
{
    rx_buffer[rx_cnt++] = alg_tim.Instance->CNT;

    if (rx_cnt == BUFFER_SIZE)
        HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);

    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_5);
}

/** @} */