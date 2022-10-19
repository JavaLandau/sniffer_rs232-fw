/**
\file
\author JavaLandau
\copyright MIT License
\brief BSP LED RGB module

The file includes implementation of BSP layer of the LED RGB
*/

#include "common.h"
#include "bsp_led_rgb.h"
#include "bsp_rcc.h"
#include "stm32f4xx_ll_tim.h"
#include <stdbool.h>

/** 
 * \defgroup bsp_led_rgb BSP LED RGB
 * \brief Module of BSP LED RGB  
 * 
 *      The module includes two STM32 timers:  
 *      1. RGB timer which shapes RGB color of the LED by PWM  
 *      2. Blink timer which makes blinking of the LED
 * \ingroup bsp
 * @{
*/

/// Frequency of RGB timer in Hz
#define RGB_TIM_FREQ          1000

/// Value of period register of RGB timer
#define RGB_TIM_PERIOD              UINT16_MAX

/// Frequency of blink timer in Hz
#define BLINK_TIM_FREQ          2000

/// STM32 HAL TIM instance of RGB timer
static TIM_HandleTypeDef htim_rgb = {.Instance = TIM1};

/// STM32 HAL TIM instance of blink timer
static TIM_HandleTypeDef htim_blink = {.Instance = TIM2};

/// Array of STM32 HAL TIM channels
static uint32_t led_rgb_tim_channels[] = {TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3};

/// Corrective coefficient for red channel, set by \ref bsp_led_rgb_calibrate
static float coef_r = 1.0f;

/// Corrective coefficient for green channel, set by \ref bsp_led_rgb_calibrate
static float coef_g = 1.0f;

/// Corrective coefficient for blue channel, set by \ref bsp_led_rgb_calibrate
static float coef_b = 1.0f;

/** Timer main MSP initialization
 * 
 * The function executes clock and NVIC initialization
 * 
 * \param[in] htim STM32 HAL TIM instance, should equal to \ref htim_rgb or \ref htim_blink
*/
static void __led_rgb_tim_pwm_msp_init(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1) {
        __HAL_RCC_TIM1_CLK_ENABLE();
    } else if (htim->Instance == TIM2) {
        __HAL_RCC_TIM2_CLK_ENABLE();

        HAL_NVIC_SetPriority(TIM2_IRQn, 5, 0);
        HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);
        HAL_NVIC_EnableIRQ(TIM2_IRQn);
    }
}

/** Timer main MSP deinitialization
 * 
 * The function executes clock and NVIC deinitialization
 * 
 * \param[in] htim STM32 HAL TIM instance, should equal to \ref htim_rgb or \ref htim_blink
*/
static void __led_rgb_tim_pwm_msp_deinit(TIM_HandleTypeDef *htim)
{
    if (htim->Instance == TIM1) {
        __HAL_RCC_TIM1_CLK_DISABLE();
    } else if (htim->Instance == TIM2) {
        HAL_NVIC_DisableIRQ(TIM2_IRQn);
        __HAL_RCC_TIM2_CLK_DISABLE();
    }
}

/** Timer posterior MSP initialization
 * 
 * The function executes GPIO initialization for PWM purposes of RGB timer
*/
static void __led_rgb_tim_msp_post_init(void)
{
    GPIO_InitTypeDef gpio_init_struct = {0};

    if (__HAL_RCC_GPIOA_IS_CLK_DISABLED())
        __HAL_RCC_GPIOA_CLK_ENABLE();

    gpio_init_struct.Pin = GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10;
    gpio_init_struct.Mode = GPIO_MODE_AF_PP;
    gpio_init_struct.Pull = GPIO_NOPULL;
    gpio_init_struct.Speed = GPIO_SPEED_FREQ_LOW;
    gpio_init_struct.Alternate = GPIO_AF1_TIM1;
    HAL_GPIO_Init(GPIOA, &gpio_init_struct);
}

/** Timer preliminary MSP deinitialization
 * 
 * The function executes deinitialization of GPIO used of RGB timer
*/
static void __led_rgb_tim_msp_prev_deinit(void)
{
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10);
}

/** Callback by period elapsion of blink timer
 * 
 * The function enables outputs of RGB timer turning ON RGB LED (enabled phase of blink)

* \param[in] htim STM32 HAL TIM instance, should equal to \ref htim_blink
*/
static void __led_rgb_blink_tim_period_elapsed_callback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM2)
        return;

    LL_TIM_EnableAllOutputs(htim_rgb.Instance);
}

/** Callback by pulse elapsion of blink timer
 * 
 * The function disables outputs of RGB timer turning OFF RGB LED (disabled phase of blink)

* \param[in] htim STM32 HAL TIM instance, should equal to \ref htim_blink
*/
static void __led_rgb_blink_tim_pwm_pulse_finished_callback(TIM_HandleTypeDef *htim)
{
    if (htim->Instance != TIM2)
        return;

    LL_TIM_DisableAllOutputs(htim_rgb.Instance);
}

/** Start blink timer
 * 
 * The function starts blink timer, used to enable blink feature
 * 
 * \return \ref RES_OK on success error otherwise
 */
static uint8_t __led_rgb_blink_start(void)
{
    __HAL_TIM_SET_COUNTER(&htim_blink, 0);
    if (HAL_TIM_PWM_Start_IT(&htim_blink, TIM_CHANNEL_1) != HAL_OK)
        return RES_NOK;

    __HAL_TIM_ENABLE_IT(&htim_blink, TIM_IT_UPDATE);

    return RES_OK;
}

/** Stop blink timer
 * 
 * The function stops blink timer, used to disable blink feature
 * 
 * \return \ref RES_OK on success error otherwise
 */
static uint8_t __led_rgb_blink_stop(void)
{
    if (HAL_TIM_PWM_Stop_IT(&htim_blink, TIM_CHANNEL_1) != HAL_OK)
        return RES_NOK;

    __HAL_TIM_DISABLE_IT(&htim_blink, TIM_IT_UPDATE);
    HAL_NVIC_ClearPendingIRQ(TIM2_IRQn);

    LL_TIM_EnableAllOutputs(htim_rgb.Instance);

    return RES_OK;
}

/** Flag whether blink timer is started
 * 
 * \return true if started false otherwise
 */
static bool __led_rgb_blink_is_started(void)
{
    return (LL_TIM_IsEnabledCounter(htim_blink.Instance) != 0);
}

/* BSP RGB LED initialization, see header file for details */
uint8_t bsp_led_rgb_init(void)
{
    uint8_t res = RES_OK;
    TIM_ClockConfigTypeDef clock_source_config = {0};

    do {
        /* PWM timer init */
        HAL_TIM_RegisterCallback(&htim_rgb, HAL_TIM_PWM_MSPINIT_CB_ID, __led_rgb_tim_pwm_msp_init);
        HAL_TIM_RegisterCallback(&htim_rgb, HAL_TIM_PWM_MSPDEINIT_CB_ID, __led_rgb_tim_pwm_msp_deinit);

        htim_rgb.Init.Period = RGB_TIM_PERIOD;
        htim_rgb.Init.Prescaler = (uint32_t)((float)bsp_rcc_apb_timer_freq_get(htim_rgb.Instance) / (float)(htim_rgb.Init.Period * RGB_TIM_FREQ) + 0.5f) - 1;

        if (htim_rgb.Init.Prescaler > UINT16_MAX) {
            res = RES_INVALID_PAR;
            break;
        }

        htim_rgb.Init.CounterMode = TIM_COUNTERMODE_UP;
        htim_rgb.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        htim_rgb.Init.RepetitionCounter = 0;
        htim_rgb.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
        if (HAL_TIM_PWM_Init(&htim_rgb) != HAL_OK) {
            res = RES_NOK;
            break;
        }

        /* PWM timer clock configuration */
        clock_source_config.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
        if (HAL_TIM_ConfigClockSource(&htim_rgb, &clock_source_config) != HAL_OK) {
            res = RES_NOK;
            break;
        }

        __led_rgb_tim_msp_post_init();

        /* Blink timer init */
        HAL_TIM_RegisterCallback(&htim_blink, HAL_TIM_PWM_MSPINIT_CB_ID, __led_rgb_tim_pwm_msp_init);
        HAL_TIM_RegisterCallback(&htim_blink, HAL_TIM_PWM_MSPDEINIT_CB_ID, __led_rgb_tim_pwm_msp_init);
    } while(0);

    return res;
}

/* BSP RGB LED deinitialization, see header file for details */
uint8_t bsp_led_rgb_deinit(void)
{
    uint8_t res = __led_rgb_blink_stop();

    if (res != RES_OK)
        return res;

    for (uint8_t i = 0; i < 3; i++) {
        if (HAL_TIM_PWM_Stop(&htim_rgb, led_rgb_tim_channels[i]) != HAL_OK) {
            res = RES_NOK;
            break;
        }
    }

    if (res != RES_OK)
        return res;

    __led_rgb_tim_msp_prev_deinit();

    if (HAL_TIM_PWM_DeInit(&htim_rgb) != HAL_OK)
        return RES_NOK;

    if (HAL_TIM_PWM_DeInit(&htim_blink) != HAL_OK)
        return RES_NOK;

    return RES_OK;
}

/* Calibration of RGB LED, see header file for details */
uint8_t bsp_led_rgb_calibrate(const struct bsp_led_rgb *coef_rgb)
{
    if (!coef_rgb)
        return RES_INVALID_PAR;

    coef_r = (float)coef_rgb->r / UINT8_MAX;
    coef_g = (float)coef_rgb->g / UINT8_MAX;
    coef_b = (float)coef_rgb->b / UINT8_MAX;

    return RES_OK;
}

/* Set RGB value for LED, see header file for details */
uint8_t bsp_led_rgb_set(const struct bsp_led_rgb *rgb)
{
    if (!rgb)
        return RES_INVALID_PAR;

    uint8_t res = RES_OK;
    float pulse_width[] = {rgb->b * coef_b, rgb->r * coef_r, rgb->g * coef_g};

    TIM_OC_InitTypeDef config_OC = {0};
    config_OC.OCMode = TIM_OCMODE_PWM1;
    config_OC.OCPolarity = TIM_OCPOLARITY_HIGH;
    config_OC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    config_OC.OCFastMode = TIM_OCFAST_DISABLE;
    config_OC.OCIdleState = TIM_OCIDLESTATE_RESET;
    config_OC.OCNIdleState = TIM_OCNIDLESTATE_RESET;

    bool blink_started = false;

    do {
        if (__led_rgb_blink_is_started()) {
            blink_started = true;
            res = __led_rgb_blink_stop();

            if (res != RES_OK)
                break;
        }

        for (uint8_t i = 0; i < 3; i++) {
            if (HAL_TIM_PWM_Stop(&htim_rgb, led_rgb_tim_channels[i]) != HAL_OK) {
                res = RES_NOK;
                break;
            }

            config_OC.Pulse = (uint32_t)((RGB_TIM_PERIOD * pulse_width[i]) / UINT8_MAX + 0.5f);

            if (HAL_TIM_PWM_ConfigChannel(&htim_rgb, &config_OC, led_rgb_tim_channels[i]) != HAL_OK) {
                res = RES_NOK;
                break;
            }

            if (HAL_TIM_PWM_Start(&htim_rgb, led_rgb_tim_channels[i]) != HAL_OK) {
                res = RES_NOK;
                break;
            }
        }

        if (blink_started) {
            res = __led_rgb_blink_start();

            if (res != RES_OK)
                break;
        }
    } while(0);

    return res;
}

/* LED RGB blinking enable, see header file for details */
uint8_t bsp_led_rgb_blink_enable(const struct bsp_led_pwm *pwm)
{
    if (!pwm || !pwm->width_on_ms || !pwm->width_off_ms)
        return RES_INVALID_PAR;

    uint8_t res = __led_rgb_blink_stop();

    if (res != RES_OK)
        return res;

    TIM_ClockConfigTypeDef clock_source_config = {0};

    do {
        /* Blink timer init */
        htim_blink.Init.Period = (BLINK_TIM_FREQ * (pwm->width_on_ms + pwm->width_off_ms)) / 1000;
        htim_blink.Init.Prescaler = __LL_TIM_CALC_PSC(bsp_rcc_apb_timer_freq_get(htim_blink.Instance), BLINK_TIM_FREQ);

        if (htim_blink.Init.Prescaler > UINT16_MAX) {
            res = RES_INVALID_PAR;
            break;
        }

        htim_blink.Init.CounterMode = TIM_COUNTERMODE_UP;
        htim_blink.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        htim_blink.Init.RepetitionCounter = 0;
        htim_blink.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
        if (HAL_TIM_PWM_Init(&htim_blink) != HAL_OK) {
            res = RES_NOK;
            break;
        }

        /* Blink timer clock configuration */
        clock_source_config.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
        if (HAL_TIM_ConfigClockSource(&htim_blink, &clock_source_config) != HAL_OK) {
            res = RES_NOK;
            break;
        }

        HAL_TIM_RegisterCallback(&htim_blink, HAL_TIM_PERIOD_ELAPSED_CB_ID, __led_rgb_blink_tim_period_elapsed_callback);
        HAL_TIM_RegisterCallback(&htim_blink, HAL_TIM_PWM_PULSE_FINISHED_CB_ID, __led_rgb_blink_tim_pwm_pulse_finished_callback);

        TIM_OC_InitTypeDef config_OC = {0};
        config_OC.OCMode = TIM_OCMODE_PWM1;
        config_OC.OCPolarity = TIM_OCPOLARITY_HIGH;
        config_OC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
        config_OC.OCFastMode = TIM_OCFAST_DISABLE;
        config_OC.OCIdleState = TIM_OCIDLESTATE_RESET;
        config_OC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
        config_OC.Pulse = (BLINK_TIM_FREQ * pwm->width_on_ms) / 1000;

        if (HAL_TIM_PWM_ConfigChannel(&htim_blink, &config_OC, TIM_CHANNEL_1) != HAL_OK) {
            res = RES_NOK;
            break;
        }
        __HAL_TIM_DISABLE_OCxPRELOAD(&htim_blink, TIM_CHANNEL_1);

        res = __led_rgb_blink_start();

        if (res != RES_OK)
            break;

    } while(0);

    if (res != RES_OK)
        __led_rgb_blink_stop();

    return res;
}

/* LED RGB blinking disable, see header file for details */
uint8_t bsp_led_rgb_blink_disable(void)
{
    return __led_rgb_blink_stop();
}

/** NVIC TIM2 (blink timer) IRQ handler */
void TIM2_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim_blink);
}

/** @} */