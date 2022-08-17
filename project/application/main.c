#include "types.h"
#include "stm32f4xx_hal.h"
#include "bsp_lcd1602.h"

static uint8_t system_clock_config(void)
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

int main()
{
	//HAL_Init();

	if (system_clock_config() != RES_OK)
		HAL_NVIC_SystemReset();

	lcd1602_settings_t settings = {
		.type_shift = 	LCD1602_SHIFT_CURSOR_RIGHT,
		.num_line = LCD1602_NUM_LINE_2,
		.font_size = LCD1602_FONT_SIZE_5X8,
		.type_move_cursor = LCD1602_CURSOR_MOVE_RIGHT,
		.shift_entire_disp = LCD1602_SHIFT_ENTIRE_PERFORMED,
		.type_interface = LCD1602_INTERFACE_8BITS,
		.disp_state = LCD1602_DISPLAY_ON,
		.cursor_state = LCD1602_CURSOR_OFF,
		.cursor_blink_state = LCD1602_CURSOR_BLINK_OFF
	};

	uint8_t res = lcd1602_init(&settings);

	if (res != RES_OK)
		HAL_NVIC_SystemReset();

	lcd1602_printf("  Sladkyh snov", "    Yanuska!");
	
	while(1) {}

	return 0;
}
