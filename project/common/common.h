/**
\file
\author JavaLandau
\copyright MIT License
\brief Common utils

The file includes basic utils, defines and macros
*/

/** 
 * \defgroup common_lib Common
 * \brief Common libraries for generic purposes
 * @{
 */

#ifndef __COMMON_H__
#define __COMMON_H__

#define RES_OK                  0       ///< Return code: Success
#define RES_NOK                 1       ///< Return code: Not specified error
#define RES_INVALID_PAR         2       ///< Return code: Invalid input parameter(-s)
#define RES_MEMORY_ERR          3       ///< Return code: Memory allocation error
#define RES_TIMEOUT             4       ///< Return code: Timeout occured
#define RES_NOT_SUPPORTED       5       ///< Return code: Some feature is not supported
#define RES_OVERFLOW            6       ///< Return code: Overflow of an object
#define RES_NOT_INITIALIZED     7       ///< Return code: An object is not initialized
#define RES_NOT_ALLOWED         8       ///< Return code: An object/feature is not allowed

/** MACRO Array size
 * 
 * The macro calculates size of an array, statically allocated
 * 
 * \param[in] X an array
 * \result size of an array
*/
#define ARRAY_SIZE(X)       (sizeof(X) / sizeof(X[0]))

/** MACRO Minimum from two values
 * 
 * \param[in] X first value
 * \param[in] Y second value
 * \result minimal value
*/
#define MIN(X, Y)           (((X) < (Y)) ? (X) : (Y))

/** MACRO Maximum from two values
 * 
 * \param[in] X first value
 * \param[in] Y second value
 * \result maximal value
*/
#define MAX(X, Y)           (((X) > (Y)) ? (X) : (Y))

/** MACRO Flag of an printable symbol
 * 
 * The macro decides whether symbol \p X is printable ASCII symbol
 * 
 * \param[in] X symbol in char format
 * \result true if symbol is printable false otherwise
*/
#define IS_PRINTABLE(X)     (X >= ' ' && X <= '~')

/** MACRO Delay by MPU instructions in us
 * 
 * The macro uses MPU instructions to make delay.  
 * The macro normally used when it needs to make delay with duration less 1 ms  
 * or other delay functions are unavailable
 * \warning The delay is inaccurate as the function does not take into account  
 * the time spent for interrups routine
 * 
 * \param[in] DELAY value of delay in us
*/
#define INSTR_DELAY_US(DELAY)   \
        do {\
            __IO uint32_t clock_delay = DELAY * (HAL_RCC_GetSysClockFreq() / 8 / 1000000);\
            do {\
                __NOP();\
            } while (--clock_delay);\
        } while (0)

/** @} */

#endif