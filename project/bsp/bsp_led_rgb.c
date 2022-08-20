#include "types.h"
#include "bsp_led_rgb.h"
#include "stm32f4xx_ll_rcc.h"

#define LED_RGB_PERIOD          1000
#define TIM_PERIOD              UINT16_MAX

static TIM_HandleTypeDef htim1 = {.Instance = TIM1};

static uint32_t led_rgb_tim_channels[] = {TIM_CHANNEL_1, TIM_CHANNEL_2, TIM_CHANNEL_3};

static float coef_r = 1.0f, coef_g = 1.0f, coef_b = 1.0f;

static uint32_t __led_rgb_tim_clk_get(void)
{
    uint32_t timclk_prescaler = LL_RCC_GetTIMPrescaler() >> RCC_DCKCFGR_TIMPRE_Pos;
    uint32_t hclk_freq = HAL_RCC_GetHCLKFreq();
    uint32_t pclk_freq = HAL_RCC_GetPCLK2Freq();

    uint32_t tim_freq = 0;

    tim_freq = pclk_freq * (2UL << timclk_prescaler);
    tim_freq = (tim_freq > hclk_freq) ? hclk_freq : tim_freq;

    return tim_freq;
}

static void __led_rgb_tim_pwm_msp_init(TIM_HandleTypeDef* htim)
{
    if (htim->Instance !=TIM1)
        return;

    __HAL_RCC_TIM1_CLK_ENABLE();
}

static void __led_rgb_tim_pwm_msp_deinit(TIM_HandleTypeDef* htim)
{
    if (htim->Instance !=TIM1)
        return;

    __HAL_RCC_TIM1_CLK_DISABLE();
}

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

static void __led_rgb_tim_msp_prev_deinit(void)
{
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_8 | GPIO_PIN_9 | GPIO_PIN_10);
}

uint8_t led_rgb_init(void)
{
    uint8_t res = RES_OK;
    TIM_ClockConfigTypeDef clock_source_config = {0};

    do {
        /* Timer init */
        HAL_TIM_RegisterCallback(&htim1, HAL_TIM_PWM_MSPINIT_CB_ID, __led_rgb_tim_pwm_msp_init);
        HAL_TIM_RegisterCallback(&htim1, HAL_TIM_PWM_MSPDEINIT_CB_ID, __led_rgb_tim_pwm_msp_deinit);

        htim1.Init.Period = TIM_PERIOD;
        htim1.Init.Prescaler = (uint32_t)((float)__led_rgb_tim_clk_get() / (float)(htim1.Init.Period * LED_RGB_PERIOD) + 0.5f);
        htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
        htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
        htim1.Init.RepetitionCounter = 0;
        htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
        if (HAL_TIM_PWM_Init(&htim1) != HAL_OK) {
            res = RES_NOK;
            break;
        }

        /* Clock configuration */
        clock_source_config.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
        if (HAL_TIM_ConfigClockSource(&htim1, &clock_source_config) != HAL_OK) {
            res = RES_NOK;
            break;
        }

        __led_rgb_tim_msp_post_init();

    } while(0);

    return res;
}

uint8_t led_rgb_deinit(void)
{
    uint8_t res = RES_OK;
    for (uint8_t i = 0; i < 3; i++) {
        if (HAL_TIM_PWM_Stop(&htim1, led_rgb_tim_channels[i]) != HAL_OK) {
            res = RES_NOK;
            break;
        }
    }

    if (res != RES_OK)
        return res;

    __led_rgb_tim_msp_prev_deinit();

    if (HAL_TIM_PWM_DeInit(&htim1) != HAL_OK)
        return RES_NOK;

    return RES_OK;
}

uint8_t led_rgb_calibrate(float _coef_r, float _coef_g, float _coef_b)
{
    if (_coef_r > 1.0f || _coef_g > 1.0f || _coef_b > 1.0f)
        return RES_INVALID_PAR;

    coef_r = _coef_r;
    coef_g = _coef_g;
    coef_b = _coef_b;

    return RES_OK;
}

uint8_t led_rgb_set(uint8_t r, uint8_t g, uint8_t b)
{
    uint8_t res = RES_OK;
    float pulse_width[] = {b * coef_b, r * coef_r, g * coef_g};

    TIM_OC_InitTypeDef config_OC = {0};
    config_OC.OCMode = TIM_OCMODE_PWM1;
    config_OC.OCPolarity = TIM_OCPOLARITY_HIGH;
    config_OC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
    config_OC.OCFastMode = TIM_OCFAST_DISABLE;
    config_OC.OCIdleState = TIM_OCIDLESTATE_RESET;
    config_OC.OCNIdleState = TIM_OCNIDLESTATE_RESET;

    for (uint8_t i = 0; i < 3; i++) {
        if (HAL_TIM_PWM_Stop(&htim1, led_rgb_tim_channels[i]) != HAL_OK) {
            res = RES_NOK;
            break;
        }

        config_OC.Pulse = (uint32_t)((TIM_PERIOD * pulse_width[i]) / UINT8_MAX + 0.5f);

        if (HAL_TIM_PWM_ConfigChannel(&htim1, &config_OC, led_rgb_tim_channels[i]) != HAL_OK) {
            res = RES_NOK;
            break;
        }

        if (HAL_TIM_PWM_Start(&htim1, led_rgb_tim_channels[i]) != HAL_OK) {
            res = RES_NOK;
            break;
        }
    }

    return res;
}