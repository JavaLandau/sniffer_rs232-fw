/**
\file
\author JavaLandau
\copyright MIT License
\brief Header of command line interface
*/

#ifndef __CLI_H__
#define __CLI_H__

#include "common.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "config.h"

/** 
 * \addtogroup cli
 * @{
*/

/** CLI initialization
 * 
 * \return \ref RES_OK on success error otherwise
 */
uint8_t cli_init(void);

/** Menu configuration start
 * 
 * \param[in,out] config device configuration
 * \return \ref RES_OK on success error otherwise
 */
uint8_t cli_menu_start(struct flash_config *config);

/** Configuration menu exit
 * 
 * \return \ref RES_OK on success error otherwise
 */
uint8_t cli_menu_exit(void);

/** Check whether configuration menu is started
 * 
 * \return true if menu started false otherwise
 */
bool cli_menu_is_started(void);

/** Trace into CLI
 * 
 * \param[in] format formatted string
 * \param[in] ... variable argument list for formatting \p format
 */
void cli_trace(const char *format, ...);

/** Trace of monitored RS-232 data
 * 
 * The function makes output of monitored RS-232 data into CLI
 * 
 * \param[in] uart_type channel type of traced \p data, should be \ref BSP_UART_TYPE_RS232_TX or \ref BSP_UART_TYPE_RS232_RX
 * \param[in] trace_type trace type
 * \param[in] data traced data
 * \param[in] len length of traced data
 * \param[in] break_line flag whether symbol of LIN break should be traced first before \p data
 * \return \ref RES_OK on success error otherwise
 */
uint8_t cli_rs232_trace(enum uart_type uart_type,
                        enum rs232_trace_type trace_type,
                        uint8_t *data,
                        uint32_t len,
                        bool break_line);

/** Welcome routine
 * 
 * The function performs welcome routine by the following scheme:  
 * 1. Trace welcome message set by \p welcome  
 * 2. Wait for any key pressing or external action (by \p forced_exit) within \p wait_time_s seconds  
 * 3. If key pressing took place the function is terminated with \p is_pressed = true  
 * 4. If timeout or external action occured the function is terminated with \p is_pressed = false  
 * 
 * \param[in] welcome welcome message
 * \param[in] wait_time_s timeout in seconds
 * \param[in] forced_exit flag of occured external action
 * \param[out] is_pressed flag whether key pressing occured
 * \return \ref RES_OK on success error otherwise
 */
uint8_t cli_welcome(const char *welcome, uint8_t wait_time_s, bool *forced_exit, bool *is_pressed);

/** Reset settings of console
 * 
 * The function resets settings of console to defaults via escape sequences
 */
void cli_terminal_reset(void);

/** @} */

#endif //__CLI_H__