/**
\file
\author JavaLandau
\copyright MIT License
\brief Header of BSP LED RGB module
*/

#ifndef __BSP_LED_RGB_H__
#define __BSP_LED_RGB_H__

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include "bsp_gpio.h"
#include "common.h"

/** 
 * \addtogroup bsp_led_rgb
 * @{
*/

/// RGB LED structure
struct bsp_led_rgb {
    uint8_t r;          ///< RED value
    uint8_t g;          ///< GREEN value
    uint8_t b;          ///< BLUE value
};

/// Parameters of RGB LED blinking
struct bsp_led_pwm {
    uint32_t width_on_ms;           ///< Width of enabled phase of blink in ms
    uint32_t width_off_ms;          ///< Width of disabled phase of blink in ms
};

/** Calibration of RGB LED
 *
 * The function sets corrective coefficients, used to adjust  
 * color of LED light according to perception.  
 * Each corrective coefficient means maximum level which can be set so  
 * the less the coefficient the less maximum brightness of appropriate color
 * 
 * As consequence if some coefficitient is:  
 * 255 - appropriate color channel is with no corrections,  
 * 0 - color channel is not used in LED light
 * 
 * \param[in] coef_rgb RGB corrective coefficients
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_led_rgb_calibrate(const struct bsp_led_rgb *coef_rgb);

/** Set RGB value for LED
 * 
 * \param[in] rgb color of LED light in RGB format
 * \return \ref RES_OK on success error otherwise
*/
uint8_t bsp_led_rgb_set(const struct bsp_led_rgb *rgb);

/** BSP RGB LED initialization
 *
 * The function executes initialization of RGB & blink timers,  
 * both of them are disabled afterwards
 * 
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_led_rgb_init(void);

/** BSP RGB LED deinitialization
 *
 * The function executes deinitialization of RGB & blink timers
 * 
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_led_rgb_deinit(void);

/** LED RGB blinking enable
 * 
 * \param[in] pwm parameters of LED blinking
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_led_rgb_blink_enable(const struct bsp_led_pwm *pwm);

/** LED RGB blinking disable
 * 
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_led_rgb_blink_disable(void);

/** MACRO Activate specific RGB LED behaivour
 * 
 * The macro is used to activate specific RGB LED behaivour  
 * in case of firmware hardfaults. Call of this macro  
 * unconditionally disables other settings of RGB LED
 * \warning The macro is firmware dead end and reset is needed to start firmware
*/
#define BSP_LED_RGB_HARDFAULT() \
    do {\
        BSP_GPIO_FORCE_OUTPUT_MODE(GPIOA, 8);\
        BSP_GPIO_FORCE_OUTPUT_MODE(GPIOA, 9);\
        BSP_GPIO_FORCE_OUTPUT_MODE(GPIOA, 10);\
        \
        BSP_GPIO_PORT_WRITE(GPIOA, GPIO_PIN_8, false);\
        BSP_GPIO_PORT_WRITE(GPIOA, GPIO_PIN_10, false);\
        \
        while (true) {\
            BSP_GPIO_PORT_WRITE(GPIOA, GPIO_PIN_9, false);\
            INSTR_DELAY_US(100000);\
            BSP_GPIO_PORT_WRITE(GPIOA, GPIO_PIN_9, true);\
            INSTR_DELAY_US(100000);\
        }\
    } while (0)

/** @} */

#endif //__BSP_LED_RGB_H__