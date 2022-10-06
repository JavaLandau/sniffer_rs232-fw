#include "common.h"
#include "bsp_button.h"
#include "bsp_rcc.h"
#include "stm32f4xx_ll_tim.h"

#define BUTTON_TIM_FREQ            (10000)
#define TIM_TICK_TO_MS(X)          ((1000 * (X)) / BUTTON_TIM_FREQ)

static EXTI_HandleTypeDef hexti = {.Line = EXTI_LINE_4};
static TIM_HandleTypeDef htim = {.Instance = TIM7};

static struct button_init_ctx ctx = {0};
static enum button_action cur_action = BUTTON_NONE;
static uint16_t prev_tick = 0;

static void __button_tim_msp_init(TIM_HandleTypeDef* htim)
{
    if (htim->Instance != TIM7)
        return;

    __HAL_RCC_TIM7_CLK_ENABLE();

    HAL_NVIC_SetPriority(TIM7_IRQn, 5, 0);
    HAL_NVIC_ClearPendingIRQ(TIM7_IRQn);
    HAL_NVIC_EnableIRQ(TIM7_IRQn);
}

static void __button_tim_msp_deinit(TIM_HandleTypeDef* htim)
{
    if (htim->Instance != TIM7)
        return;

    __HAL_RCC_TIM7_CLK_DISABLE();

    HAL_NVIC_DisableIRQ(TIM7_IRQn);
}

static void __button_tim_period_elapsed_callback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM7)
        return;

    HAL_TIM_Base_Stop_IT(htim);

    if (cur_action != BUTTON_NONE && ctx.button_isr_cb)
        ctx.button_isr_cb(cur_action);

    cur_action = BUTTON_NONE;
}

uint8_t bsp_button_init(struct button_init_ctx *init_ctx)
{
    if (!init_ctx || !init_ctx->press_delay_ms || !init_ctx->press_timeout_ms)
        return RES_INVALID_PAR;

    ctx = *init_ctx;

    /* GPIO init */
    if (__HAL_RCC_GPIOB_IS_CLK_DISABLED())
        __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* EXTI configuration */
    EXTI_ConfigTypeDef exti_config = {0};

    exti_config.Line            = hexti.Line;
    exti_config.Mode            = EXTI_MODE_INTERRUPT;
    exti_config.Trigger         = EXTI_TRIGGER_FALLING;
    exti_config.GPIOSel         = EXTI_GPIOB;

    HAL_EXTI_SetConfigLine(&hexti, &exti_config);
    HAL_EXTI_ClearPending(&hexti, EXTI_TRIGGER_FALLING);

    HAL_NVIC_ClearPendingIRQ(EXTI4_IRQn);
    HAL_NVIC_SetPriority(EXTI4_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI4_IRQn);

    /* Timer init */
    HAL_TIM_RegisterCallback(&htim, HAL_TIM_BASE_MSPINIT_CB_ID, __button_tim_msp_init);
    HAL_TIM_RegisterCallback(&htim, HAL_TIM_BASE_MSPDEINIT_CB_ID, __button_tim_msp_deinit);

    htim.Init.Prescaler = __LL_TIM_CALC_PSC(bsp_rcc_apb_timer_freq_get(htim.Instance), BUTTON_TIM_FREQ);
    htim.Init.Period = (BUTTON_TIM_FREQ * init_ctx->press_timeout_ms) / 1000;

    if (htim.Init.Period > UINT16_MAX || htim.Init.Prescaler > UINT16_MAX)
        return RES_INVALID_PAR;

    htim.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim.Init.RepetitionCounter = 0;
    htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim) != HAL_OK)
        return RES_NOK;

    HAL_TIM_RegisterCallback(&htim, HAL_TIM_PERIOD_ELAPSED_CB_ID, __button_tim_period_elapsed_callback);

    return RES_OK;
}

uint8_t bsp_button_deinit(void)
{
    if (HAL_TIM_Base_Stop_IT(&htim) != HAL_OK)
        return RES_NOK;

    if (HAL_TIM_Base_DeInit(&htim) != HAL_OK)
        return RES_NOK;

    HAL_NVIC_DisableIRQ(EXTI4_IRQn);

    if (HAL_EXTI_ClearConfigLine(&hexti) != HAL_OK)
        return RES_NOK;

    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_4);

    cur_action = BUTTON_NONE;

    return RES_OK;
}

void EXTI4_IRQHandler(void)
{
    uint16_t cur_tick = __HAL_TIM_GET_COUNTER(&htim);

    do {
        if (cur_action != BUTTON_NONE) {
            if (TIM_TICK_TO_MS(cur_tick - prev_tick) < ctx.press_delay_ms)
                break;
        }

        prev_tick = cur_tick;

        switch (cur_action) {
        case BUTTON_NONE:
        case BUTTON_PRESSED:
            cur_action = (cur_action == BUTTON_NONE) ? BUTTON_PRESSED : BUTTON_TWICE_PRESSED;

            HAL_TIM_Base_Stop_IT(&htim);

            __HAL_TIM_SET_COUNTER(&htim, 0);
            HAL_TIM_Base_Start(&htim);

            __HAL_TIM_CLEAR_FLAG(&htim, TIM_FLAG_UPDATE);
            __HAL_TIM_ENABLE_IT(&htim, TIM_IT_UPDATE);

            break;

        default:
            break;
        }
    } while(0);

    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_4);
}

void TIM7_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim);
}