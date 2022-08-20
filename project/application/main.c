#include "types.h"
#include "stm32f4xx_hal.h"
#include "bsp_lcd1602.h"
#include "bsp_led_rgb.h"

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

void test_hardfault(void)
{
    uint8_t value = 0;

    while(1) {
        led_rgb_set(255 * value, 0, 0);
        value = 1 - value;
        HAL_Delay(1000);
    }
}

static uint8_t __r = 0, __g = 0, __b = 0;

int main()
{
    HAL_Init();

    if (system_clock_config() != RES_OK)
        HAL_NVIC_SystemReset();

    uint8_t res = led_rgb_init();

    if (res != RES_OK)
        HAL_NVIC_SystemReset();

    //led_rgb_calibrate(1.0f, 0.247f, 0.0784);
    //led_rgb_calibrate(1.0f, 0.353f, 0.0784);
    led_rgb_calibrate(1.0f, 0.29412f, 0.047059f);

    /*if( !(DWT->CTRL & 1) )
    {
        CoreDebug->DEMCR |= 0x01000000;
        DWT->CTRL |= 1; // enable the counter
    }*/

    //uint32_t t1 = DWT->CYCCNT;
    //HAL_Delay(1);
    //uint32_t t2 = DWT->CYCCNT;

    //uint32_t dir = t2 - t1;

    lcd1602_settings_t settings = {
        .num_line = LCD1602_NUM_LINE_2,
        .font_size = LCD1602_FONT_SIZE_5X8,
        .type_move_cursor = LCD1602_CURSOR_MOVE_RIGHT,
        .shift_entire_disp = LCD1602_SHIFT_ENTIRE_PERFORMED,
        .type_interface = LCD1602_INTERFACE_8BITS,
        .disp_state = LCD1602_DISPLAY_ON,
        .cursor_state = LCD1602_CURSOR_OFF,
        .cursor_blink_state = LCD1602_CURSOR_BLINK_OFF
    };

    //res = lcd1602_init(&settings);

    //if (res != RES_OK)
    //    test_hardfault();

    //lcd1602_printf(" Sladkyh snov", " Yanuska");


    //res = led_rgb_set(255, 70, 0);

    //res = led_rgb_set(127, 127, 127);
    //res = led_rgb_set(10, 10, 10);

    uint8_t prev_r = 0, prev_g = 0, prev_b = 0;

    while(1) {
        if(prev_r != __r || prev_g != __g || prev_b != __b) {
            prev_r = __r;
            prev_g = __g;
            prev_b = __b;
            
			led_rgb_set(__r, __g, __b);
        }
        HAL_Delay(100);
    }

    return 0;
}
