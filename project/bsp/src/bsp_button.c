/**
\file
\author JavaLandau
\copyright MIT License
\brief BSP button module

The file includes implementation of BSP layer of the button
*/

#include "common.h"
#include "bsp_button.h"
#include "bsp_rcc.h"
#include "bsp_gpio.h"
#include "stm32f4xx_ll_tim.h"
#include <stdbool.h>

/** 
 * \defgroup bsp_button BSP button
 * \brief Module of BSP button
 * \ingroup bsp
 * @{
*/

/// Frequency of \ref htim
#define BUTTON_TIM_FREQ            (10000)

/** MACRO Conversion of TIM tick counter to milliseconds
 * 
 * \param[in] X TIM tick counter
 * \return milliseconds elapsed from the start of the timer
*/
#define TIM_TICK_TO_MS(X)          ((1000 * (X)) / BUTTON_TIM_FREQ)

/** MACRO TIM period register calculation
 * 
 * \param[in] X desired TIM period in milliseconds
 * \return value of TIM period register
*/
#define TIM_PERIOD_CALC(X)         ((BUTTON_TIM_FREQ * (X)) / 1000)

/// STM32 HAL EXTI instance, used to detect pushing and releasing actions on the button
static EXTI_HandleTypeDef hexti = {.Line = EXTI_LINE_4};

/// STM32 HAL TIM instance, used to detect long pressing and filter contact bounce
static TIM_HandleTypeDef htim = {.Instance = TIM7};

/// BSP button context
static struct button_init_ctx ctx = {0};

/// Current state of the button: true - pressed, false - not
static bool button_pressed = false;

/// Flag whether button timer is checking of long press action on the button
static bool is_long_action = false;

/** STM32 HAL TIM MSP initialization
 *
 * The function executes clock, NVIC initialization
 * 
 * \param[in] htim STM32 HAL TIM instance, should equal to \ref htim
 */
static void __button_tim_msp_init(TIM_HandleTypeDef* htim)
{
    if (htim->Instance != TIM7)
        return;

    __HAL_RCC_TIM7_CLK_ENABLE();

    HAL_NVIC_SetPriority(TIM7_IRQn, 5, 0);
    HAL_NVIC_ClearPendingIRQ(TIM7_IRQn);
    HAL_NVIC_EnableIRQ(TIM7_IRQn);
}

/** STM32 HAL TIM MSP deinitialization
 *
 * The function executes clock, NVIC deinitialization
 * 
 * \param[in] htim STM32 HAL TIM instance, should equal to \ref htim
 */
static void __button_tim_msp_deinit(TIM_HandleTypeDef* htim)
{
    if (htim->Instance != TIM7)
        return;

    __HAL_RCC_TIM7_CLK_DISABLE();

    HAL_NVIC_DisableIRQ(TIM7_IRQn);
}

/** Callback for button timer period elapsion
 *
 * The function is designed to have two modes:  
 * 1. Timeout as protection against contact bounce is expired (\ref is_long_action = false).  
 * After that next button action can be caught  
 * 
 * 2. Minimum duration to consider that the button is pressed for a long time (\ref is_long_action = true)  
 * User callback \ref button_init_ctx::button_isr_cb with parameter \ref BUTTON_LONG_PRESSED is called
 * 
 * \param[in] htim STM32 HAL TIM instance, should equal to \ref htim
 */
static void __button_tim_period_elapsed_callback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM7)
        return;

    HAL_TIM_Base_Stop_IT(htim);

    if (button_pressed && is_long_action && ctx.button_isr_cb)
        ctx.button_isr_cb(BUTTON_LONG_PRESSED);
}

/** Flag whether button timer is started
 * 
 * \return true if the timer is started, false otherwise
*/
static bool __button_tim_is_started(void)
{
    return LL_TIM_IsEnabledCounter(htim.Instance) ? true : false;
}

/** Stop button timer
 * 
 * \return \ref RES_OK on success error otherwise
*/
static uint8_t __button_tim_stop(void)
{
    return (HAL_TIM_Base_Stop_IT(&htim) == HAL_OK) ? RES_NOK : RES_OK;
}

/** Start button timer
 * 
 * \param[in] period_ms of the timer in ms
 * \return \ref RES_OK on success error otherwise
*/
static uint8_t __button_tim_start(uint32_t period_ms)
{
    if (HAL_TIM_Base_Stop_IT(&htim) != HAL_OK)
        return RES_NOK;

    __HAL_TIM_SET_AUTORELOAD(&htim, TIM_PERIOD_CALC(period_ms));
    __HAL_TIM_SET_COUNTER(&htim, 0);

    uint8_t res = (HAL_TIM_Base_Start(&htim) == HAL_OK) ? RES_NOK : RES_OK;

    __HAL_TIM_CLEAR_FLAG(&htim, TIM_FLAG_UPDATE);
    __HAL_TIM_ENABLE_IT(&htim, TIM_IT_UPDATE);

    return res;
}

/* BSP button initialization, see header file for details */
uint8_t bsp_button_init(struct button_init_ctx *init_ctx)
{
    /* Parameters check */
    if (!init_ctx || !init_ctx->press_delay_ms)
        return RES_INVALID_PAR;

    if (!init_ctx->press_min_dur_ms || !init_ctx->long_press_dur_ms)
        return RES_INVALID_PAR;

    if (init_ctx->long_press_dur_ms < init_ctx->press_min_dur_ms)
        return RES_INVALID_PAR;

    if (TIM_PERIOD_CALC(init_ctx->press_delay_ms) > UINT16_MAX)
        return RES_INVALID_PAR;

    if (TIM_PERIOD_CALC(init_ctx->long_press_dur_ms) > UINT16_MAX)
        return RES_INVALID_PAR;

    ctx = *init_ctx;

    /* GPIO init */
    if (__HAL_RCC_GPIOB_IS_CLK_DISABLED())
        __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    GPIO_InitStruct.Pin = GPIO_PIN_4;
    GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING_FALLING;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* EXTI configuration */
    EXTI_ConfigTypeDef exti_config = {0};

    exti_config.Line            = hexti.Line;
    exti_config.Mode            = EXTI_MODE_INTERRUPT;
    exti_config.Trigger         = EXTI_TRIGGER_RISING_FALLING;
    exti_config.GPIOSel         = EXTI_GPIOB;

    HAL_EXTI_SetConfigLine(&hexti, &exti_config);
    HAL_EXTI_ClearPending(&hexti, EXTI_TRIGGER_RISING_FALLING);

    HAL_NVIC_ClearPendingIRQ(EXTI4_IRQn);
    HAL_NVIC_SetPriority(EXTI4_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(EXTI4_IRQn);

    /* Timer init */
    HAL_TIM_RegisterCallback(&htim, HAL_TIM_BASE_MSPINIT_CB_ID, __button_tim_msp_init);
    HAL_TIM_RegisterCallback(&htim, HAL_TIM_BASE_MSPDEINIT_CB_ID, __button_tim_msp_deinit);

    htim.Init.Prescaler = __LL_TIM_CALC_PSC(bsp_rcc_apb_timer_freq_get(htim.Instance), BUTTON_TIM_FREQ);

    if (htim.Init.Prescaler > UINT16_MAX)
        return RES_INVALID_PAR;

    htim.Init.Period = UINT16_MAX;
    htim.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim.Init.RepetitionCounter = 0;
    htim.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
    if (HAL_TIM_Base_Init(&htim) != HAL_OK)
        return RES_NOK;

    HAL_TIM_RegisterCallback(&htim, HAL_TIM_PERIOD_ELAPSED_CB_ID, __button_tim_period_elapsed_callback);

    return RES_OK;
}

/* BSP button deinitialization, see header file for details */
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

    return RES_OK;
}

/** NVIC EXTI4 IRQ handler
 * 
 * The handler processes pushing/releasing actions on the button,  
 * start button timer with appropriate period and etc.
 */
void EXTI4_IRQHandler(void)
{
    uint16_t cur_tick = __HAL_TIM_GET_COUNTER(&htim);
    bool button_state = BSP_GPIO_PORT_READ(GPIOB, GPIO_PIN_4);

    do {
        if (!button_state == button_pressed)
            break;

        button_pressed = !button_state;
        bool tim_is_started = __button_tim_is_started();

        /* If timeout against contact bounce is not expired skip action */
        if (tim_is_started && !is_long_action)
            break;

        /* Skip action if:
            1. The button pressed and the timer is started
               because long action is being waited and will be processed 
               in \ref __button_tim_period_elapsed_callback 
              
            2. The button released and the timer is stopped because
               long action was occured and processed in \ref __button_tim_period_elapsed_callback
        */
        if (button_pressed == tim_is_started)
            break;

        if (!button_pressed) {
            __button_tim_stop();

            if (TIM_TICK_TO_MS(cur_tick) < ctx.press_min_dur_ms)
                break;

            if (ctx.button_isr_cb)
                ctx.button_isr_cb(BUTTON_PRESSED);

            is_long_action = false;
            __button_tim_start(ctx.press_delay_ms);
        } else {
            is_long_action = true;
            __button_tim_start(ctx.long_press_dur_ms);
        }

    } while(0);

    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_PIN_4);
}

/** NVIC IRQ TIM7 handler */
void TIM7_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim);
}

/** @} */