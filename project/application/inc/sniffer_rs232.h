/**
\file
\author JavaLandau
\copyright MIT License
\brief Header of algorithm of Sniffer RS-232
*/

#ifndef __SNIFFER_RS232_H__
#define __SNIFFER_RS232_H__

#include "common.h"
#include "stm32f4xx_hal.h"
#include "bsp_uart.h"
#include <stdbool.h>

/** 
 * \addtogroup sniffer_rs232
 * @{
*/

/** RS-232 channel detection type */
enum rs232_channel_type {
    RS232_CHANNEL_TX = 0,       ///< Algorithm works only on RS-232 TX
    RS232_CHANNEL_RX,           ///< Algorithm works only on RS-232 RX
    RS232_CHANNEL_ANY,          ///< Algorithm works until one of the RS-232 channels calculated successfully
    RS232_CHANNEL_ALL,          ///< Algorithm works on both RS-232 lines
    RS232_CHANNEL_MAX           ///< Count of RS-232 channel detection types
};

/** MACRO Check if RS-232 channel detection type is valid
 * 
 * The macro checks whether \a TYPE is valid RS-232 channel detection type
 * 
 * \param[in] TYPE RS-232 channel detection type
 * \return true if valid false otherwise
*/
#define RS232_CHANNEL_TYPE_VALID(TYPE)          (((uint32_t)(TYPE)) < RS232_CHANNEL_MAX)

/// Algorithm settings
struct sniffer_rs232_config {
    enum rs232_channel_type channel_type;   ///< RS-232 channel detection type
    uint32_t valid_packets_count;           ///< Count of received bytes to approve a hypothesis
    uint32_t uart_error_count;              ///< Count of UART frame errors when hypothesis is failed
    uint8_t baudrate_tolerance;             ///< Tolerance of UART baudrate in percents
    uint32_t min_detect_bits;               ///< Minimum count of lower levels on RS-232 line to analyse baudrate
    uint32_t exec_timeout;                  ///< Maximum time of algorithm execution
    uint32_t calc_attempts;                 ///< Count of tries of algorithm calculation
    bool lin_detection;                     ///< Flag whether LIN protocol should be detected
};

/** MACRO Get minimum valid value of a parameter
 * 
 * The macro returns minimum valid value of a parameter  
 * from algoritm settings \ref sniffer_rs232_config  
 * The macro is wrapper over \ref sniffer_rs232_config_item_range
 * 
 * \param[in] X parameter name
 * \return minimum valid value
*/
#define SNIFFER_RS232_CFG_PARAM_MIN(X)          sniffer_rs232_config_item_range((uint32_t)&((struct sniffer_rs232_config*)0)->X, true)

/** MACRO Get maximum valid value of a parameter
 * 
 * The macro returns maximum valid value of a parameter  
 * from algoritm settings \ref sniffer_rs232_config  
 * The macro is wrapper over \ref sniffer_rs232_config_item_range
 * 
 * \param[in] X parameter name
 * \return maximum valid value
*/
#define SNIFFER_RS232_CFG_PARAM_MAX(X)          sniffer_rs232_config_item_range((uint32_t)&((struct sniffer_rs232_config*)0)->X, false)

/** MACRO Check whether parameter is valid
 * 
 * The macro checks whether parameter from algorithm settings is valid
 * 
 * \param[in] X parameter name
 * \param[in] V value of a parameter
 * \return true if a parameter is valid false otherwise
 */
#define SNIFFER_RS232_CFG_PARAM_IS_VALID(X, V)  (((V) >= SNIFFER_RS232_CFG_PARAM_MIN(X)) && ((V) <= SNIFFER_RS232_CFG_PARAM_MAX(X)))

/** MACRO Default algorithm settings
 * 
 * \result initializer for \ref sniffer_rs232_config
*/
#define SNIFFER_RS232_CONFIG_DEFAULT() {\
                .channel_type = RS232_CHANNEL_ANY,\
                .valid_packets_count = 20,\
                .uart_error_count = 2,\
                .baudrate_tolerance = 10,\
                .min_detect_bits = 48,\
                .exec_timeout = 600,\
                .calc_attempts = 3,\
                .lin_detection = false\
            }

/** Algorithm initialization
 * 
 * \param[in] __config algorithm settings
 * \return \ref RES_OK on success error otherwise
 */
uint8_t sniffer_rs232_init(struct sniffer_rs232_config *__config);

/** Algorithm deinitialization
 * 
 * \return \ref RES_OK on success error otherwise
 */
uint8_t sniffer_rs232_deinit(void);

/** Algorithm calculation
 * 
 * The function executes the algorithm
 * \note uart_init_ctx::baudrate is 0 if calculation failed  
 * Despite of it the function returns \ref RES_OK if all hypotheses have been tried
 * 
 * \param[out] uart_params UART parameters of RS-232 lines
 * \return \ref RES_OK on success error otherwise
 */
uint8_t sniffer_rs232_calc(struct uart_init_ctx *uart_params);

/** Valid value range of items from algorithm settings
 * 
 * The function is used to validate settings for the algorithm
 * 
 * \param[in] shift memory shift of an item over \ref sniffer_rs232_config
 * \param[in] is_min flag indicating lower border of an range if true, upper border if false
 * \return value of a border of a range
 */
uint32_t sniffer_rs232_config_item_range(uint32_t shift, bool is_min);

/** Check algorithm settings
 * 
 * \param[in] __config algorithm settings
 * \return true if settings are valid false otherwise
*/
bool sniffer_rs232_config_check(struct sniffer_rs232_config *__config);

/** @} */

#endif //__SNIFFER_RS232_H__