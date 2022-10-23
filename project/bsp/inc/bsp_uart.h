/**
\file
\author JavaLandau
\copyright MIT License
\brief Header of BSP UART module
*/

#ifndef __BSP_UART_H__
#define __BSP_UART_H__

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

/** 
 * \addtogroup bsp_uart
 * @{
*/

/** MACRO Check BSP UART type
 * 
 * The macro checks whether \p X is valid BSP UART type
 * 
 * \param[in] X BSP UART type
 * \return true if type is valid false otherwise
*/
#define UART_TYPE_VALID(X)      (((uint32_t)(X) < BSP_UART_TYPE_MAX))

/** MACRO Check BSP UART word length
 * 
 * The macro checks whether \p X is valid BSP UART word length
 * 
 * \param[in] X BSP UART word length
 * \return true if word length is valid false otherwise
*/
#define UART_WORDLEN_VALID(X)   (((X) == BSP_UART_WORDLEN_8) || ((X) == BSP_UART_WORDLEN_9))

/** MACRO Check BSP UART parity type
 * 
 * The macro checks whether \p X is valid BSP UART parity type
 * 
 * \param[in] X BSP UART parity type
 * \return true if parity type is valid false otherwise
*/
#define UART_PARITY_VALID(X)    (((X) == BSP_UART_PARITY_NONE) || ((X) == BSP_UART_PARITY_EVEN) || ((X) == BSP_UART_PARITY_ODD))

/** MACRO Check BSP UART stop bits count
 * 
 * The macro checks whether \p X is valid BSP UART stop bits count
 * 
 * \param[in] X BSP UART stop bits count
 * \return true if stop bits count is valid false otherwise
*/
#define UART_STOPBITS_VALID(X)  (((X) == BSP_UART_STOPBITS_1) || ((X) == BSP_UART_STOPBITS_2))

#define BSP_UART_ERROR_PE       HAL_UART_ERROR_PE   ///< BSP UART parity error
#define BSP_UART_ERROR_NE       HAL_UART_ERROR_NE   ///< BSP UART noise error
#define BSP_UART_ERROR_FE       HAL_UART_ERROR_FE   ///< BSP UART frame error
#define BSP_UART_ERROR_ORE      HAL_UART_ERROR_ORE  ///< BSP UART overrun error
#define BSP_UART_ERROR_DMA      HAL_UART_ERROR_DMA  ///< BSP UART DMA error

/// Mask including all possible BSP UART errors
#define BSP_UART_ERRORS_ALL     (BSP_UART_ERROR_PE | BSP_UART_ERROR_NE | BSP_UART_ERROR_FE | BSP_UART_ERROR_ORE | BSP_UART_ERROR_DMA)

/// Types of BSP UART instances
enum uart_type {
    BSP_UART_TYPE_CLI = 0,      ///< CLI
    BSP_UART_TYPE_RS232_TX,     ///< RS-232 TX channel (RX only)
    BSP_UART_TYPE_RS232_RX,     ///< RS-232 RX channel (RX only)
    BSP_UART_TYPE_MAX           ///< Count of BSP UART types
};

/// BSP UART word length
enum uart_wordlen {
    BSP_UART_WORDLEN_8 = 8,     ///< Word length is 8 bits
    BSP_UART_WORDLEN_9 = 9      ///< Word length is 9 bits
};

/// BSP UART parity types
enum uart_parity {
    BSP_UART_PARITY_NONE = 0,   ///< None parity
    BSP_UART_PARITY_EVEN = 1,   ///< Even parity
    BSP_UART_PARITY_ODD = 2     ///< Odd parity
};

/// BSP UART stop bits count
enum uart_stopbits {
    BSP_UART_STOPBITS_1 = 1,    ///< 1 stop bit
    BSP_UART_STOPBITS_2 = 2     ///< 2 stop bits
};

/// BSP UART initializing context
struct uart_init_ctx {
    uint32_t baudrate;                                                          ///< UART baudrate
    uint32_t tx_size;                                                           ///< Size of sent buffer
    uint32_t rx_size;                                                           ///< Size of received buffer
    bool lin_enabled;                                                           ///< Flag whether LIN protocol is supported
    enum uart_wordlen wordlen;                                                  ///< Word length
    enum uart_parity parity;                                                    ///< Parity type
    enum uart_stopbits stopbits;                                                ///< Count of stop bits
    void (*error_isr_cb)(enum uart_type type, uint32_t error, void *params);    ///< Callback for occurrence of BSP UART error
    void (*overflow_isr_cb)(enum uart_type type, void *params);                 ///< Callback for occurrence of overflow of receive buffer
    void (*lin_break_isr_cb)(enum uart_type type, void *params);                ///< Callback for occurrence of LIN break detection
    void *params;                                                               ///< Optional parameters, passed to the callbacks
};

/** Initialization of BSP UART instance
 * 
 * The function executes initizalition of BSP UART instance according  
 * to settings stored in \p init  
 * if appropriate BSP UART instance is initialized it will be reinitialized
 * 
 * \param[in] type BSP UART type
 * \param[in] init initializating context of BSP UART instance
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_uart_init(enum uart_type type, struct uart_init_ctx *init);

/** Deinitialization of BSP UART instance
 * 
 * The function executes deinitizalition of BSP UART instance
 * 
 * \param[in] type BSP UART type
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_uart_deinit(enum uart_type type);

/** Receive BSP UART data
 * 
 * The function executes reading of data received via DMA UART
 * \note The function is blocking if \p tmt_ms is not zero
 * 
 * \param[in] type BSP UART type
 * \param[out] data received data
 * \param[out] len size of received data
 * \param[in] tmt_ms timeout for receiving in ms
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_uart_read(enum uart_type type, void *data, uint16_t *len, uint32_t tmt_ms);

/** Send BSP UART data
 * 
 * The function executes sending of data via DMA UART
 * \note The function is blocking if previous DMA UART sending is not completed  
 * and \p tmt_ms is not zero
 * 
 * \param[in] type BSP UART type
 * \param[in] data sent data
 * \param[in] len size of sent data
 * \param[in] tmt_ms timeout for sending in ms
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_uart_write(enum uart_type type, void *data, uint16_t len, uint32_t tmt_ms);

/** BSP UART instance start
 * 
 * The function (re-)start UART DMA reception, enable appropriate interrupts and etc.
 * 
 * \param[in] type BSP UART type
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_uart_start(enum uart_type type);

/** BSP UART instance stop
 * 
 * The function stop UART DMA reception/sending
 * 
 * \param[in] type BSP UART type
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_uart_stop(enum uart_type type);

/** Flag whether BSP UART instance is started
 * 
 * \param[in] type BSP UART type
 * \return true if the instance is started false otherwise
 */
bool bsp_uart_is_started(enum uart_type type);

/** Flag whether received buffer is empty
 * 
 * \param[in] type BSP UART type
 * \return true if \ref uart_ctx::rx_buff is empty false otherwise
 */
bool bsp_uart_rx_buffer_is_empty(enum uart_type type);

/** @} */

#endif //__BSP_UART_H__