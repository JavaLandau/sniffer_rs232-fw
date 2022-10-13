#include "stm32f4xx_hal.h"
#include "bsp_led_rgb.h"

void NMI_Handler(void)
{
    BSP_LED_RGB_HARDFAULT();
}

void HardFault_Handler(void)
{
    BSP_LED_RGB_HARDFAULT();
}

void MemManage_Handler(void)
{
    BSP_LED_RGB_HARDFAULT();
}

void BusFault_Handler(void)
{
    BSP_LED_RGB_HARDFAULT();
}

void UsageFault_Handler(void)
{
    BSP_LED_RGB_HARDFAULT();
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}
