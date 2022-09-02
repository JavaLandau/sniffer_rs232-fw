#include "common.h"
#include "bsp_rcc.h"
#include "stm32f4xx_ll_rcc.h"

uint8_t rcc_main_config_init(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Configure the main internal regulator output voltage */
    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);
    /* Initializes the RCC Oscillators according to the specified parameters in the RCC_OscInitTypeDef structure. */
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 15;
    RCC_OscInitStruct.PLL.PLLN = 216;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 2;
    RCC_OscInitStruct.PLL.PLLR = 2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
        return RES_NOK;

    /* Activate the Over-Drive mode */
    if (HAL_PWREx_EnableOverDrive() != HAL_OK)
        return RES_NOK;

    /* Initializes the CPU, AHB and APB buses clocks */
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK |RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
        return RES_NOK;

    return RES_OK;
}

uint32_t rcc_apb_timer_freq_get(TIM_TypeDef *instance)
{
    if (!IS_TIM_INSTANCE(instance))
        return 0;

    uint32_t timclk_prescaler = LL_RCC_GetTIMPrescaler() >> RCC_DCKCFGR_TIMPRE_Pos;
    uint32_t hclk_freq = HAL_RCC_GetHCLKFreq();
    uint32_t pclk_freq = (TIM_APB_NUM_CLOCK_GET(instance) == 2) ? HAL_RCC_GetPCLK2Freq() : HAL_RCC_GetPCLK1Freq();

    uint32_t tim_freq = 0;

    tim_freq = pclk_freq * (2UL << timclk_prescaler);
    tim_freq = (tim_freq > hclk_freq) ? hclk_freq : tim_freq;

    return tim_freq;
}
