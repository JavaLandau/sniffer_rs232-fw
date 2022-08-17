#ifndef __BSP_LCD1602_H__
#define __BSP_LCD1602_H__

#include <stdint.h>

typedef enum {
	LCD1602_SHIFT_CURSOR_UNDEF = -1,
	LCD1602_SHIFT_CURSOR_LEFT,
	LCD1602_SHIFT_CURSOR_RIGHT,
	LCD1602_SHIFT_DISPLAY_LEFT,
	LCD1602_SHIFT_DISPLAY_RIGHT,
	LCD1602_SHIFT_MAX
} lcd1602_type_shift_e;

typedef enum {
	LCD1602_NUM_LINE_UNDEF = -1,
	LCD1602_NUM_LINE_1,
	LCD1602_NUM_LINE_2,
	LCD1602_NUM_LINE_MAX
} lcd1602_num_line_e;

typedef enum {
	LCD1602_FONT_SIZE_UNDEF = -1,
	LCD1602_FONT_SIZE_5X8,
	LCD1602_FONT_SIZE_5X11,
	LCD1602_FONT_SIZE_MAX
} lcd1602_font_size_e;

typedef enum {
	LCD1602_CURSOR_MOVE_UNDEF = -1,
	LCD1602_CURSOR_MOVE_LEFT,
	LCD1602_CURSOR_MOVE_RIGHT,
	LCD1602_CURSOR_MOVE_MAX
} lcd1602_type_move_cursor_e;

typedef enum {
	LCD1602_SHIFT_ENTIRE_UNDEF = -1,
	LCD1602_SHIFT_ENTIRE_PERFORMED,
	LCD1602_SHIFT_ENTIRE_NOT_PERFORMED,
	LCD1602_SHIFT_ENTIRE_MAX
} lcd1602_shift_entire_disp_e;

typedef enum {
	LCD1602_INTERFACE_UNDEF = -1,
	LCD1602_INTERFACE_4BITS,
	LCD1602_INTERFACE_8BITS,
	LCD1602_INTERFACE_MAX
} lcd1602_type_interface_e;

typedef enum {
	LCD1602_DISPLAY_UNDEF = -1,
	LCD1602_DISPLAY_OFF,
	LCD1602_DISPLAY_ON,
	LCD1602_DISPLAY_MAX
} lcd1602_disp_state_e;

typedef enum {
	LCD1602_CURSOR_UNDEF = -1,
	LCD1602_CURSOR_OFF,
	LCD1602_CURSOR_ON,
	LCD1602_CURSOR_MAX
} lcd1602_cursor_state_e;

typedef enum {
	LCD1602_CURSOR_BLINK_UNDEF = -1,
	LCD1602_CURSOR_BLINK_OFF,
	LCD1602_CURSOR_BLINK_ON,
	LCD1602_CURSOR_BLINK_MAX
} lcd1602_cursor_blink_state_e;

typedef struct {
	lcd1602_type_shift_e			type_shift;
	lcd1602_num_line_e				num_line;
	lcd1602_font_size_e				font_size;
	lcd1602_type_move_cursor_e		type_move_cursor;
	lcd1602_shift_entire_disp_e		shift_entire_disp;
	lcd1602_type_interface_e		type_interface;
	lcd1602_disp_state_e			disp_state;
	lcd1602_cursor_state_e			cursor_state;
	lcd1602_cursor_blink_state_e	cursor_blink_state;
} lcd1602_settings_t;

typedef enum {
	LCD1602_NO_CODES = 0,
	LCD1602_NO_CONNECTION,
	LCD1602_INTERFACE_ERROR,
} lcd1602_ext_code;

uint8_t lcd1602_init(lcd1602_settings_t *settings);
uint8_t lcd1602_printf(const char *line1, const char *line2, ...);
uint8_t lcd1602_ddram_address_set(const uint8_t address);
uint8_t lcd1602_cgram_address_set(const uint8_t address);

uint8_t lcd1602_cursor_disp_shift(const lcd1602_type_shift_e shift);

uint8_t lcd1602_display_on_off(const lcd1602_disp_state_e disp_state,
								const lcd1602_cursor_state_e cursor_state, 
								const lcd1602_cursor_blink_state_e cursor_blink_state);

uint8_t lcd1602_entry_mode_set(const lcd1602_type_move_cursor_e cursor,
								const lcd1602_shift_entire_disp_e shift_entire);

uint8_t lcd1602_return_home(void);
uint8_t lcd1602_display_clear(void);

#endif //__BSP_LCD1602_H__
