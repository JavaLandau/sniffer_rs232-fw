/**
\file
\author JavaLandau
\copyright MIT License
\brief File of handlers for basic interrups

The file includes handlers for main MPU interrupts
*/

#include "stm32f4xx_hal.h"
#include "bsp_led_rgb.h"

/** 
 * \defgroup basic_interrupts Basic interrupts
 * \brief Handlers for basic MPU interrupts
 * \ingroup application
 * @{
*/

/** NMI IRQ handler 
 * 
 * The handler uses \ref BSP_LED_RGB_HARDFAULT as firmware dead end
*/
void NMI_Handler(void)
{
    BSP_LED_RGB_HARDFAULT();
}

/** Hardfault handler 
 * 
 * The handler uses \ref BSP_LED_RGB_HARDFAULT as firmware dead end
*/
void HardFault_Handler(void)
{
    BSP_LED_RGB_HARDFAULT();
}

/** MemManage IRQ handler 
 * 
 * The handler uses \ref BSP_LED_RGB_HARDFAULT as firmware dead end
*/
void MemManage_Handler(void)
{
    BSP_LED_RGB_HARDFAULT();
}

/** BusFault IRQ handler 
 * 
 * The handler uses \ref BSP_LED_RGB_HARDFAULT as firmware dead end
*/
void BusFault_Handler(void)
{
    BSP_LED_RGB_HARDFAULT();
}

/** UsageFault IRQ handler 
 * 
 * The handler uses \ref BSP_LED_RGB_HARDFAULT as firmware dead end
*/
void UsageFault_Handler(void)
{
    BSP_LED_RGB_HARDFAULT();
}

/** SVC IRQ handler 
 * 
 * NOT used
*/
void SVC_Handler(void)
{
}

/** DebugMon IRQ handler 
 * 
 * NOT used
*/
void DebugMon_Handler(void)
{
}

/** PendSV IRQ handler 
 * 
 * NOT used
*/
void PendSV_Handler(void)
{
}

/** Systick IRQ handler 
 * 
 * The handler makes count of HAL tick counter
*/
void SysTick_Handler(void)
{
    HAL_IncTick();
}

/** @} */