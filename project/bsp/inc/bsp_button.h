/**
\file
\author JavaLandau
\copyright MIT License
\brief Header of BSP button module
*/

#ifndef __BSP_BUTTON_H__
#define __BSP_BUTTON_H__

#include <stdint.h>
#include "stm32f4xx_hal.h"

/** 
 * \addtogroup bsp_button
 * @{
*/

/// BSP button actions
enum button_action {
    /** None of button actions */
    BUTTON_NONE = 0,
    /** Button is pressed for a short time not less than \ref button_init_ctx::press_min_dur_ms  
     * but less than \ref button_init_ctx::long_press_dur_ms */
    BUTTON_PRESSED,
    /** Button is pressed for a long time not less than \ref button_init_ctx::long_press_dur_ms */
    BUTTON_LONG_PRESSED,
    /** Count of types of button actions */
    BUTTON_ACTION_MAX
};

/// Initializing context of BSP button
struct button_init_ctx {
    /** Delay in ms as time protection against contact bounce */
    uint32_t press_delay_ms;
    /** Minimal duration in ms of button pressing, 
     * also contact bounce protection together with \ref press_delay_ms 
     * \note should be less than \ref long_press_dur_ms */
    uint32_t press_min_dur_ms;
    /** Minimal duration in ms to detect long press action on the button
     * \note should be more than \ref press_min_dur_ms */
    uint32_t long_press_dur_ms;
    /** User callback called by button action */
    void (*button_isr_cb)(enum button_action action);
};

/** BSP button initialization
 * 
 * \param[in] init_ctx initializing context of BSP button
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_button_init(struct button_init_ctx *init_ctx);

/** BSP button deinitialization
 * 
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_button_deinit(void);

/** @} */

#endif //__BSP_BUTTON_H__