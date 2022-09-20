#ifndef __BSP_LCD1602_H__
#define __BSP_LCD1602_H__

#include <stdint.h>

enum lcd1602_type_shift {
    LCD1602_SHIFT_CURSOR_UNDEF = -1,
    LCD1602_SHIFT_CURSOR_LEFT,
    LCD1602_SHIFT_CURSOR_RIGHT,
    LCD1602_SHIFT_DISPLAY_LEFT,
    LCD1602_SHIFT_DISPLAY_RIGHT,
    LCD1602_SHIFT_MAX
};

enum lcd1602_num_line {
    LCD1602_NUM_LINE_UNDEF = -1,
    LCD1602_NUM_LINE_1,
    LCD1602_NUM_LINE_2,
    LCD1602_NUM_LINE_MAX
};

enum lcd1602_font_size {
    LCD1602_FONT_SIZE_UNDEF = -1,
    LCD1602_FONT_SIZE_5X8,
    LCD1602_FONT_SIZE_5X11,
    LCD1602_FONT_SIZE_MAX
};

enum lcd1602_type_move_cursor {
    LCD1602_CURSOR_MOVE_UNDEF = -1,
    LCD1602_CURSOR_MOVE_LEFT,
    LCD1602_CURSOR_MOVE_RIGHT,
    LCD1602_CURSOR_MOVE_MAX
};

enum lcd1602_shift_entire_disp {
    LCD1602_SHIFT_ENTIRE_UNDEF = -1,
    LCD1602_SHIFT_ENTIRE_PERFORMED,
    LCD1602_SHIFT_ENTIRE_NOT_PERFORMED,
    LCD1602_SHIFT_ENTIRE_MAX
};

enum lcd1602_type_interface {
    LCD1602_INTERFACE_UNDEF = -1,
    LCD1602_INTERFACE_4BITS,
    LCD1602_INTERFACE_8BITS,
    LCD1602_INTERFACE_MAX
};

enum lcd1602_disp_state {
    LCD1602_DISPLAY_UNDEF = -1,
    LCD1602_DISPLAY_OFF,
    LCD1602_DISPLAY_ON,
    LCD1602_DISPLAY_MAX
};

enum lcd1602_cursor_state {
    LCD1602_CURSOR_UNDEF = -1,
    LCD1602_CURSOR_OFF,
    LCD1602_CURSOR_ON,
    LCD1602_CURSOR_MAX
};

enum lcd1602_cursor_blink_state {
    LCD1602_CURSOR_BLINK_UNDEF = -1,
    LCD1602_CURSOR_BLINK_OFF,
    LCD1602_CURSOR_BLINK_ON,
    LCD1602_CURSOR_BLINK_MAX
};

struct lcd1602_settings {
    enum lcd1602_num_line              num_line;
    enum lcd1602_font_size             font_size;
    enum lcd1602_type_move_cursor      type_move_cursor;
    enum lcd1602_shift_entire_disp     shift_entire_disp;
    enum lcd1602_type_interface        type_interface;
    enum lcd1602_disp_state            disp_state;
    enum lcd1602_cursor_state          cursor_state;
    enum lcd1602_cursor_blink_state    cursor_blink_state;
};

uint8_t bsp_lcd1602_init(struct lcd1602_settings *settings);
uint8_t bsp_lcd1602_printf(const char *line1, const char *line2, ...);
uint8_t bsp_lcd1602_ddram_address_set(const uint8_t address);
uint8_t bsp_lcd1602_cgram_address_set(const uint8_t address);

uint8_t bsp_lcd1602_function_set(const enum lcd1602_type_interface interface,
                                 const enum lcd1602_num_line num_line,
                                 const enum lcd1602_font_size font_size);

uint8_t bsp_lcd1602_cursor_disp_shift(const enum lcd1602_type_shift shift);

uint8_t bsp_lcd1602_display_on_off(const enum lcd1602_disp_state disp_state,
                                   const enum lcd1602_cursor_state cursor_state,
                                   const enum lcd1602_cursor_blink_state cursor_blink_state);

uint8_t bsp_lcd1602_entry_mode_set(const enum lcd1602_type_move_cursor cursor,
                                   const enum lcd1602_shift_entire_disp shift_entire);

uint8_t bsp_lcd1602_return_home(void);
uint8_t bsp_lcd1602_display_clear(void);

#endif //__BSP_LCD1602_H__
