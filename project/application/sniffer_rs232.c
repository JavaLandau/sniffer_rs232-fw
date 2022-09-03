#include "sniffer_rs232.h"
#include "bsp_gpio.h"
#include "bsp_rcc.h"
#include <stdbool.h>
#include <string.h>

#define VALID_PACKETS_THR       (20)
#define BAUD_TOLERANCE          (10)
#define TIM_FREQ                (1000000 * BAUD_TOLERANCE)
#define BUFFER_SIZE             (512)
#define MIN_BAUD_CNT_THR        (48 * 4)
#define UART_ERROR_THR          (2)
#define MAX_EXEC_TMT            (600000)
#define CALC_MAX_ATT            (3)
#define UART_BUFF_SIZE          (128)

#if (MIN_BAUD_CNT_THR > BUFFER_SIZE)
#error "MIN_BAUD_CNT_THR must be less or equaled to BUFFER_SIZE"
#endif

static TIM_HandleTypeDef htim6 = {.Instance = TIM6};
static EXTI_HandleTypeDef hexti1 = {.Line = EXTI_LINE_3};
static EXTI_HandleTypeDef hexti2 = {.Line = EXTI_LINE_5};

static uint32_t tx_cnt = 0;
static uint32_t rx_cnt = 0;
static uint32_t tx_buffer[BUFFER_SIZE] = {0};
static uint32_t rx_buffer[BUFFER_SIZE] = {0};

struct hyp_check_ctx {
    uint32_t error_parity_cnt;
    uint32_t error_frame_cnt;
    uint32_t valid_cnt;
    bool overflow;
};

struct baud_calc_ctx {
    uint32_t    *cnt;
    uint32_t    *buffer;
    uint32_t    idx;
    uint32_t    min_len_bit;
    uint32_t    baudrate;
    bool        toggle_bit;
    bool        done;
};

struct hyp_ctx {
    enum uart_wordlen wordlen;
    enum uart_parity parity;
    uint8_t jump;
};

static const struct hyp_ctx hyp_seq[] = {
    {BSP_UART_WORDLEN_8, BSP_UART_PARITY_NONE, 1},
    {BSP_UART_WORDLEN_8, BSP_UART_PARITY_EVEN, 4},
    {BSP_UART_WORDLEN_8, BSP_UART_PARITY_ODD, 4},
    {BSP_UART_WORDLEN_9, BSP_UART_PARITY_NONE, 4},
    {BSP_UART_WORDLEN_9, BSP_UART_PARITY_EVEN, 0},
    {BSP_UART_WORDLEN_9, BSP_UART_PARITY_ODD, 0}
};

static void __sniffer_rs232_tim_msp_init(TIM_HandleTypeDef* htim)
{
    if (htim->Instance != TIM6)
        return;

    __HAL_RCC_TIM6_CLK_ENABLE();
}

static void __sniffer_rs232_tim_msp_deinit(TIM_HandleTypeDef* htim)
{
    if (htim->Instance != TIM6)
        return;

    __HAL_RCC_TIM6_CLK_DISABLE();
}

static uint32_t __sniffer_rs232_baudrate_get(uint32_t len_bit)
{
    const uint32_t baudrates_list[] = {921600, 460800, 230400, 115200, 57600, 38400, 19200, 9600, 4800, 2400};
    const float tolerance = (float)BAUD_TOLERANCE / 100.0f;

    if (!len_bit)
        return 0;

    float calc_baud = (float)TIM_FREQ / (float)len_bit;

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

static uint8_t __sniffer_rs232_line_baudrate_calc_init(GPIO_TypeDef* gpiox, uint16_t port, IRQn_Type irq_type)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = port;
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

        if (BSP_GPIO_PORT_READ(gpiox, port)) {
            HAL_NVIC_ClearPendingIRQ(irq_type);
            HAL_NVIC_EnableIRQ(irq_type);
            break;
        }
    }

    return res;
}

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
                if (__sniffer_rs232_baudrate_get(len_bit))
                    ctx->min_len_bit = len_bit;
            }
        }

        ctx->toggle_bit = !ctx->toggle_bit;
    }

    ctx->done = (ctx->idx >= MIN_BAUD_CNT_THR);
}

uint8_t __sniffer_rs232_baudrate_calc(enum rs232_calc_type calc_type, uint32_t *baudrate)
{
    if (!baudrate || !RS232_CALC_TYPE_VALID(calc_type))
        return RES_INVALID_PAR;

    const uint32_t uart_max_exec_tmt = MAX_EXEC_TMT;

    memset(tx_buffer, 0, sizeof(tx_buffer));
    memset(rx_buffer, 0, sizeof(rx_buffer));
    tx_cnt = rx_cnt = 0;

    struct baud_calc_ctx tx_ctx = {.cnt = &tx_cnt, .buffer = tx_buffer, .idx = 0, .min_len_bit = UINT32_MAX,
                                    .baudrate = 0, .toggle_bit = false, .done = false};

    struct baud_calc_ctx rx_ctx = {.cnt = &rx_cnt, .buffer = rx_buffer, .idx = 0, .min_len_bit = UINT32_MAX,
                                    .baudrate = 0, .toggle_bit = false, .done = false};

    uint32_t res = RES_OK;

    if (calc_type != RS232_CALC_RX) {
        res = __sniffer_rs232_line_baudrate_calc_init(GPIOA, GPIO_PIN_3, EXTI3_IRQn);

        if (res != RES_OK)
            return res;
    }

    if (calc_type != RS232_CALC_TX) {
        res = __sniffer_rs232_line_baudrate_calc_init(GPIOC, GPIO_PIN_5, EXTI9_5_IRQn);

        if (res != RES_OK)
            return res;
    }

    bool finish_flag = false;
    uint32_t calc_baudrate = 0;
    uint32_t start_exec_time = HAL_GetTick();

    while (true) {
        if ((HAL_GetTick() - start_exec_time) > uart_max_exec_tmt) {
            res = RES_TIMEOUT;
            break;
        }

        if (res != RES_OK)
            break;

        /* TX line */
        if (calc_type != RS232_CALC_RX && !tx_ctx.done) {
            __sniffer_rs232_line_baudrate_calc(&tx_ctx);
        }

        /* RX line */
        if (calc_type != RS232_CALC_TX && !rx_ctx.done) {
            __sniffer_rs232_line_baudrate_calc(&rx_ctx);
        }

        /* Result processing */
        struct baud_calc_ctx *ctx = &rx_ctx;
        switch (calc_type) {
        case RS232_CALC_TX:
            ctx = &tx_ctx;
        case RS232_CALC_RX:
            if (ctx->done) {
                calc_baudrate = ctx->baudrate;
                finish_flag = true;
            }
            break;

        case RS232_CALC_ANY:
            if (tx_ctx.done || rx_ctx.done) {
                calc_baudrate = tx_ctx.baudrate ? tx_ctx.baudrate : rx_ctx.baudrate;
                finish_flag = (calc_baudrate != 0) || (tx_ctx.done && rx_ctx.done);
            }
            break;

        case RS232_CALC_ALL:
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

    return res;
}

void __sniffer_rs232_uart_overflow_cb(enum uart_type type, void *params)
{
    struct hyp_check_ctx *check_ctx = (struct hyp_check_ctx*)params;
    check_ctx->overflow = true;
}

void __sniffer_rs232_uart_error_cb(enum uart_type type, uint32_t error, void *params)
{
    struct hyp_check_ctx *check_ctx = (struct hyp_check_ctx*)params;

    if (error & BSP_UART_ERROR_PE)
        check_ctx->error_parity_cnt++;

    if (error & BSP_UART_ERROR_FE)
        check_ctx->error_frame_cnt++;

    bsp_uart_start(type);
}

uint8_t __sniffer_rs232_params_calc(enum rs232_calc_type calc_type, uint32_t baudrate, int8_t *hyp_num)
{
    if (!baudrate || !RS232_CALC_TYPE_VALID(calc_type) || !hyp_num)
        return RES_INVALID_PAR;

    *hyp_num = -1;
    int8_t __hyp_num = 0;
    uint8_t res = RES_OK;

    struct hyp_check_ctx tx_check_ctx = {0};
    struct hyp_check_ctx rx_check_ctx = {0};

    do {
        struct uart_init_ctx init_ctx = {0};
        init_ctx.baudrate = baudrate;
        init_ctx.wordlen = hyp_seq[__hyp_num].wordlen;
        init_ctx.parity = hyp_seq[__hyp_num].parity;
        init_ctx.rx_size = UART_BUFF_SIZE;
        init_ctx.stopbits = BSP_UART_STOPBITS_1;
        init_ctx.error_isr_cb = __sniffer_rs232_uart_error_cb;

        if (calc_type != RS232_CALC_RX) {
            memset(&tx_check_ctx, 0, sizeof(tx_check_ctx));
            init_ctx.params = &tx_check_ctx;

            res = bsp_uart_init(BSP_UART_TYPE_RS232_TX, &init_ctx);

            if (res != RES_OK)
                break;
        }

        if (calc_type != RS232_CALC_TX) {
            memset(&rx_check_ctx, 0, sizeof(rx_check_ctx));
            init_ctx.params = &rx_check_ctx;

            res = bsp_uart_init(BSP_UART_TYPE_RS232_RX, &init_ctx);

            if (res != RES_OK)
                break;
        }

        bool finish_flag = false;
        const uint32_t uart_max_exec_tmt = MAX_EXEC_TMT;
        uint32_t start_exec_time = HAL_GetTick();

        while (true) {
            if ((HAL_GetTick() - start_exec_time) > uart_max_exec_tmt) {
                res = RES_TIMEOUT;
                break;
            }

            uint16_t len = 0;
            if (calc_type != RS232_CALC_RX && bsp_uart_read(BSP_UART_TYPE_RS232_TX, NULL, &len, 0) == RES_OK)
                tx_check_ctx.valid_cnt += len;

            if (calc_type != RS232_CALC_TX && bsp_uart_read(BSP_UART_TYPE_RS232_RX, NULL, &len, 0) == RES_OK)
                rx_check_ctx.valid_cnt += len;

            if (tx_check_ctx.overflow || rx_check_ctx.overflow) {
                res = RES_OVERFLOW;
                break;
            }

            bool error_parity_exceed = (tx_check_ctx.error_parity_cnt >= UART_ERROR_THR || rx_check_ctx.error_parity_cnt >= UART_ERROR_THR);
            bool error_frame_exceed = (tx_check_ctx.error_frame_cnt >= UART_ERROR_THR || rx_check_ctx.error_frame_cnt >= UART_ERROR_THR);

            if (error_frame_exceed || error_parity_exceed) {
                if (error_frame_exceed)
                    __hyp_num = hyp_seq[__hyp_num].jump;
                else
                    __hyp_num = (__hyp_num == (ARRAY_SIZE(hyp_seq) - 1)) ? 0 : (__hyp_num + 1);
                continue;
            }

            switch (calc_type) {
            case RS232_CALC_TX:
                finish_flag = (tx_check_ctx.valid_cnt >= VALID_PACKETS_THR);
                break;

            case RS232_CALC_RX:
                finish_flag = (rx_check_ctx.valid_cnt >= VALID_PACKETS_THR);
                break;

            case RS232_CALC_ANY:
                finish_flag = (tx_check_ctx.valid_cnt >= VALID_PACKETS_THR || rx_check_ctx.valid_cnt >= VALID_PACKETS_THR);
                break;

            case RS232_CALC_ALL:
                finish_flag = (tx_check_ctx.valid_cnt >= VALID_PACKETS_THR && rx_check_ctx.valid_cnt >= VALID_PACKETS_THR);
                break;

            default:
                break;
            }

            if (finish_flag)
                break;
        }

        if (res != RES_OK)
            break;

        if (finish_flag) {
            *hyp_num = __hyp_num;
            break;
        }
    } while(__hyp_num);

    return res;
}

uint8_t sniffer_rs232_init(void)
{
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
    HAL_TIM_RegisterCallback(&htim6, HAL_TIM_BASE_MSPINIT_CB_ID, __sniffer_rs232_tim_msp_init);
    HAL_TIM_RegisterCallback(&htim6, HAL_TIM_BASE_MSPDEINIT_CB_ID, __sniffer_rs232_tim_msp_deinit);

    htim6.Init.Prescaler = bsp_rcc_apb_timer_freq_get(htim6.Instance) / TIM_FREQ - 1;
    htim6.Init.Period = UINT16_MAX;
    htim6.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim6.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim6.Init.RepetitionCounter = 0;
    htim6.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim6) != HAL_OK)
        return RES_NOK;

    if (HAL_TIM_Base_Start(&htim6) != HAL_OK)
        return RES_NOK;

    return RES_OK;
}

uint8_t sniffer_rs232_deinit(void)
{
    if (HAL_TIM_Base_Stop(&htim6) != HAL_OK)
        return RES_NOK;

    if (HAL_TIM_Base_DeInit(&htim6) != HAL_OK)
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

uint8_t sniffer_rs232_calc(enum rs232_calc_type calc_type, struct uart_init_ctx *uart_params)
{
    if (!RS232_CALC_TYPE_VALID(calc_type) || !uart_params)
        return RES_INVALID_PAR;

    uint8_t res = RES_OK;
    memset(uart_params, 0 , sizeof(struct uart_init_ctx));

    for (uint8_t i = 0; i < CALC_MAX_ATT; i++) {
        uint32_t baudrate = 0;
        res = __sniffer_rs232_baudrate_calc(calc_type, &baudrate);

        if (res != RES_OK)
            break;

        if (!baudrate)
            continue;

        int8_t hyp_num = 0;
        res = __sniffer_rs232_params_calc(calc_type, baudrate, &hyp_num);

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

static uint32_t max_dur = 0;

void EXTI3_IRQHandler(void)
{
    uint32_t t1 = DWT->CYCCNT;
    tx_buffer[tx_cnt++] = htim6.Instance->CNT;

    if (tx_cnt == BUFFER_SIZE)
        HAL_NVIC_DisableIRQ(EXTI3_IRQn);

    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_3);

    uint32_t t2 = DWT->CYCCNT;
    uint32_t dur = t2 - t1;

    if (max_dur < dur)
        max_dur = dur;
}

void EXTI9_5_IRQHandler(void)
{
    rx_buffer[rx_cnt++] = htim6.Instance->CNT;

    if (rx_cnt == BUFFER_SIZE)
        HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);

    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_5);
}
