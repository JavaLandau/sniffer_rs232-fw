/**
\file
\author JavaLandau
\copyright MIT License
\brief Header of application layer of RGB LED
*/

#ifndef __APP_LED_H
#define __APP_LED_H

#include "common.h"
#include <stdint.h>
#include <stddef.h>

/** 
 * \addtogroup app_led
 * @{
*/

/// RGB LED event (type of LED behaivour)
enum led_event {
    LED_EVENT_NONE = 0,         ///< No events
    LED_EVENT_COMMON_ERROR,     ///< Unspecified error
    LED_EVENT_CRC_ERROR,        ///< Failed to initialize \ref bsp_crc module
    LED_EVENT_FLASH_ERROR,      ///< Error in \ref config module
    LED_EVENT_LCD1602_ERROR,    ///< Failed communication with \ref bsp_lcd1602
    LED_EVENT_IN_PROCESS,       ///< \ref sniffer_rs232 is in process
    LED_EVENT_SUCCESS,          ///< The firmware is in monitoring state with no UART errors
    LED_EVENT_FAILED,           ///< \ref sniffer_rs232 is failed
    LED_EVENT_UART_ERROR,       ///< Error in module \ref bsp_uart during monitoring
    LED_EVENT_UART_OVERFLOW,    ///< Overflow in UART reception
    LED_EVENT_MAX               ///< Count of types of RGB LED behaivours
};

/** MACRO RGB LED event is valid
 * 
 * The macro decides whether \p X is valid RGB LED event
 * 
 * \param[in] X RGB LED event
 * \result true if \p X is valid RGB LED event false otherwise
*/
#define LED_EVENT_IS_VALID(X)          (((uint32_t)(X)) < LED_EVENT_MAX)

/** Initialization of application layer of RGB LED
 * 
 * \return \ref RES_OK on success error otherwise
 */
uint8_t app_led_init(void);

/** Deinitialization of application layer of RGB LED
 * 
 * \return \ref RES_OK on success error otherwise
 */
uint8_t app_led_deinit(void);

/** Set RGB LED behaivour
 * 
 * \param[in] led_event type of LED behaivour 
 * \return \ref RES_OK on success error otherwise
 */
uint8_t app_led_set(enum led_event led_event);

/** @} */

#endif /* __APP_LED_H */
