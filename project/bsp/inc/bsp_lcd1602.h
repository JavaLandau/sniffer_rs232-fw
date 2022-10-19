/**
\file
\author JavaLandau
\copyright MIT License
\brief Header of BSP LCD1602 module
*/

#ifndef __BSP_LCD1602_H__
#define __BSP_LCD1602_H__

#include <stdint.h>

/** 
 * \addtogroup bsp_lcd1602
 * @{
*/

/// Type of cursor/display shift
enum lcd1602_type_shift {
    LCD1602_SHIFT_CURSOR_UNDEF = -1,        ///< Type is undefined
    LCD1602_SHIFT_CURSOR_LEFT,              ///< Cursor shifts one position left
    LCD1602_SHIFT_CURSOR_RIGHT,             ///< Cursor shifts one position right
    LCD1602_SHIFT_DISPLAY_LEFT,             ///< Content of display shifts one position left
    LCD1602_SHIFT_DISPLAY_RIGHT,            ///< Content of display shifts one position right
    LCD1602_SHIFT_MAX                       ///< Count of shift types
};

/// Numbrt line of LCD1602
enum lcd1602_num_line {
    LCD1602_NUM_LINE_UNDEF = -1,            ///< Number line is undefined
    LCD1602_NUM_LINE_1,                     ///< First line
    LCD1602_NUM_LINE_2,                     ///< Second line
    LCD1602_NUM_LINE_MAX                    ///< Count of lines
};

/// Types of font size
enum lcd1602_font_size {
    LCD1602_FONT_SIZE_UNDEF = -1,           ///< Font size is undefined
    LCD1602_FONT_SIZE_5X8,                  ///< Font size 5x8
    LCD1602_FONT_SIZE_5X11,                 ///< Font size 5x11
    LCD1602_FONT_SIZE_MAX                   ///< Count of types of font size
};

/// Move types of cursor
enum lcd1602_type_move_cursor {
    LCD1602_CURSOR_MOVE_UNDEF = -1,         ///< Move type is undefined
    LCD1602_CURSOR_MOVE_LEFT,               ///< Cursor moves left
    LCD1602_CURSOR_MOVE_RIGHT,              ///< Cursor moves right
    LCD1602_CURSOR_MOVE_MAX                 ///< Count of move types of cursor
};

/// Shift types of entire display
enum lcd1602_shift_entire_disp {
    LCD1602_SHIFT_ENTIRE_UNDEF = -1,        ///< Shift type is undefined
    LCD1602_SHIFT_ENTIRE_PERFORMED,         ///< Shift of entire display is performed
    LCD1602_SHIFT_ENTIRE_NOT_PERFORMED,     ///< Shift of entire display is not performed
    LCD1602_SHIFT_ENTIRE_MAX                ///< Count of shift types of entire display
};

/// Type of LCD1602 interfaces
enum lcd1602_type_interface {
    LCD1602_INTERFACE_UNDEF = -1,           ///< Inteface is undefined
    LCD1602_INTERFACE_4BITS,                ///< 4-bit parallel interface
    LCD1602_INTERFACE_8BITS,                ///< 8-bit parallel interface
    LCD1602_INTERFACE_MAX                   ///< Count of LCD1602 interfaces
};


/// Display states
enum lcd1602_disp_state {
    LCD1602_DISPLAY_UNDEF = -1,             ///< Display state is undefined
    LCD1602_DISPLAY_OFF,                    ///< Display is turned OFF
    LCD1602_DISPLAY_ON,                     ///< Display is turned ON
    LCD1602_DISPLAY_MAX                     ///< Count of display states
};

/// Cursor states
enum lcd1602_cursor_state {
    LCD1602_CURSOR_UNDEF = -1,              ///< Cursor state is undefined
    LCD1602_CURSOR_OFF,                     ///< Cursor is turned OFF
    LCD1602_CURSOR_ON,                      ///< Cursor is turned ON
    LCD1602_CURSOR_MAX                      ///< Count of cursor states
};

/// Cursor blink states
enum lcd1602_cursor_blink_state {
    LCD1602_CURSOR_BLINK_UNDEF = -1,        ///< Cursor blink state is undefined
    LCD1602_CURSOR_BLINK_OFF,               ///< Cursor does NOT blink
    LCD1602_CURSOR_BLINK_ON,                ///< Cursor blinks
    LCD1602_CURSOR_BLINK_MAX                ///< Count of cursor blink states
};

/// Settings of BSP LCD1602
struct lcd1602_settings {
    enum lcd1602_num_line              num_line;            ///< 1-line or 2-line mode of display
    enum lcd1602_font_size             font_size;           ///< Font size
    enum lcd1602_type_move_cursor      type_move_cursor;    ///< Move type of cursor
    enum lcd1602_shift_entire_disp     shift_entire_disp;   ///< Shift type of entire display
    enum lcd1602_type_interface        type_interface;      ///< Type of LCD1602 interface
    enum lcd1602_disp_state            disp_state;          ///< Initial display state
    enum lcd1602_cursor_state          cursor_state;        ///< Initial cursor state
    enum lcd1602_cursor_blink_state    cursor_blink_state;  ///< Initial cursor blink state
};

/** LCD1602 initialization
 * 
 * The function does MSP initialization, sets settings to LCD1602 accoring to \p init_settings
 * \note BSP LCD1602 is designed so that 4-bit interface is not supported
 * 
 * \param[in] init_settings settings of LCD1602
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_lcd1602_init(struct lcd1602_settings *init_settings);

/** LCD1602 deinitialization
 * 
 * The function clears display and does MSP deinitialization
 * 
 * \return \ref RES_OK on success error otherwise
*/
uint8_t bsp_lcd1602_deinit(void);

/** Not centered print on display
 *
 * API implemented as wrapper over not centered \ref __lcd1602_printf
 * 
 * \param[in] line1 formatted first line, if NULL - previous content remains
 * \param[in] line2 formatted second line, if NULL - previous content remains
 * \param[in] ... variable argument list for formatting of both lines
 * \return \ref RES_OK on success error otherwise 
 */
uint8_t bsp_lcd1602_printf(const char *line1, const char *line2, ...);

/** Centered print on display
 *
 * API implemented as wrapper over centered \ref __lcd1602_printf
 * 
 * \param[in] line1 formatted first line, if NULL - previous content remains
 * \param[in] line2 formatted second line, if NULL - previous content remains 
 * \param[in] ... variable argument list for formatting of both lines
 * \return \ref RES_OK on success error otherwise 
 */
uint8_t bsp_lcd1602_cprintf(const char *line1, const char *line2, ...);

/** DDRAM address set
 * 
 * \param[in] address DDRAM address
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_lcd1602_ddram_address_set(const uint8_t address);

/** CGRAM address set
 * 
 * \param[in] address CGRAM address
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_lcd1602_cgram_address_set(const uint8_t address);

/** Set function to LCD1602
 *
 * \param[in] interface type of interface
 * \param[in] num_line 1-line or 2-line mode of display
 * \param[in] font_size font size
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_lcd1602_function_set(const enum lcd1602_type_interface interface,
                                 const enum lcd1602_num_line num_line,
                                 const enum lcd1602_font_size font_size);

/** Cursor or display shift
 * 
 * The function makes shift according to \p shift
 * 
 * \param[in] shift type of cursor/display shift
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_lcd1602_cursor_disp_shift(const enum lcd1602_type_shift shift);

/** Display ON/OFF
 * 
 * \param[in] disp_state display state
 * \param[in] cursor_state cursor state
 * \param[in] cursor_blink_state cursor blink state
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_lcd1602_display_on_off(const enum lcd1602_disp_state disp_state,
                                   const enum lcd1602_cursor_state cursor_state,
                                   const enum lcd1602_cursor_blink_state cursor_blink_state);

/** Entry mode set
 * 
 * \param[in] cursor move type of cursor
 * \param[in] shift_entire shift type of entire display
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_lcd1602_entry_mode_set(const enum lcd1602_type_move_cursor cursor,
                                   const enum lcd1602_shift_entire_disp shift_entire);

/** Return home
 * 
 * Value of address counter is set to 0 and  
 * current position on display is set to start of first line
 * 
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_lcd1602_return_home(void);

/** Display clear
 * 
 * \return \ref RES_OK on success error otherwise
 */
uint8_t bsp_lcd1602_display_clear(void);

/** @} */

#endif //__BSP_LCD1602_H__
