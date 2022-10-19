/**
\file
\author JavaLandau
\copyright MIT License
\brief Header of flash configuration
*/

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "common.h"
#include "stm32f4xx_hal.h"
#include "sniffer_rs232.h"

/** 
 * \addtogroup config
 * @{
*/

/** MACRO RS-S232 trace type is valid
 * 
 * The macro decides whether \p X is valid RS-232 trace type
 * 
 * \param[in] X RS-232 trace type
 * \result true if \p X is valid RS-232 trace type false otherwise
*/
#define RS232_TRACE_TYPE_VALID(X)       ((X) < RS232_TRACE_MAX)

/** MACRO RS-S232 interspace type is valid
 * 
 * The macro decides whether \p X is valid RS-232 interspace type
 * 
 * \param[in] X RS-232 interspace type
 * \result true if \p X is valid RS-232 interspace type false otherwise
*/
#define RS232_INTERSPACE_TYPE_VALID(X)  ((X) < RS232_INTERSPCACE_MAX)

/// Trace type of RS-232 data
enum rs232_trace_type {
    RS232_TRACE_HEX = 0,            ///< Data is traced in HEX format
    RS232_TRACE_HYBRID,             ///< Data is traced as char if printable and as HEX if not
    RS232_TRACE_MAX                 ///< Count of trace types
};

/// Type of interspaces between RS-232 data
enum rs232_interspace_type {
    RS232_INTERSPCACE_NONE = 0,     ///< No interspaces
    RS232_INTERSPCACE_SPACE,        ///< Space between RS-232 data
    RS232_INTERSPCACE_NEW_LINE,     ///< New line symbol between RS-232 data
    RS232_INTERSPCACE_MAX           ///< Count of interspace types
};

/// UART presettings
struct uart_presettings {
    bool enable;                    ///< Flag whether presettings are enabled
    uint32_t baudrate;              ///< UART baudrate in bods
    enum uart_wordlen wordlen;      ///< Size of UART frame in bits
    enum uart_parity parity;        ///< Parity type
    enum uart_stopbits stopbits;    ///< Count of stop bits
    bool lin_enabled;               ///< Flag whether LIN protocol is supported
};

/// Firmware configuration
#pragma pack(1)
struct flash_config {
    /** Algorithm settings \ref sniffer_rs232_config */
    struct sniffer_rs232_config alg_config;
    /** UART presettings \ref uart_presettings */
    struct uart_presettings presettings;
    /** Trace type of RS-232 data \ref rs232_trace_type */
    enum rs232_trace_type trace_type;
    /** IDLE symbol for RS-232 data */
    enum rs232_interspace_type idle_presence;
    /** Delimiter symbol between RS-232 TX & RX data */
    enum rs232_interspace_type txrx_delimiter;
    /** Flag whether result of the algorithm \ref sniffer_rs232 
     * is stored into \ref uart_presettings */
    bool save_to_presettings;
    /** CRC of configuration */
    uint32_t crc;
};
#pragma pack()

/** MACRO Default UART presettings
 * 
 * \result initializer for \ref uart_presettings
*/
#define UART_PRESETTINGS_DEFAULT()  {\
    .enable = false,\
    .parity = BSP_UART_PARITY_NONE,\
    .baudrate = 0,\
    .stopbits = BSP_UART_STOPBITS_1,\
    .wordlen = BSP_UART_WORDLEN_8,\
    .lin_enabled = false\
}

/** MACRO Default configuration
 * 
 * \result initializer for \ref flash_config
*/
#define FLASH_CONFIG_DEFAULT()  {\
    .alg_config = SNIFFER_RS232_CONFIG_DEFAULT(),\
    .presettings = UART_PRESETTINGS_DEFAULT(),\
    .trace_type = RS232_TRACE_HEX,\
    .idle_presence = RS232_INTERSPCACE_NONE,\
    .txrx_delimiter = RS232_INTERSPCACE_NONE,\
    .save_to_presettings = true\
}

/** Save configuration
 * 
 * The function saves configuration into flash
 * 
 * \param[in] config saved configuration
 * \return \ref RES_OK on success error otherwise
 */
uint8_t config_save(struct flash_config *config);

/** Read configuration
 * 
 * The function reads configuration from flash
 * 
 * \param[out] config read configuration
 * \return \ref RES_OK on success error otherwise
 */
uint8_t config_read(struct flash_config *config);

/** @} */

#endif //__CONFIG_H__