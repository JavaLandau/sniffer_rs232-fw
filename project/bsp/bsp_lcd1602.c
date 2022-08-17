#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "bsp_lcd1602.h"
#include "bsp_gpio.h"
#include "types.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>

#define MAX_CGRAM_ADDRESS				0x3F
#define MAX_DDRAM_ADDRESS				0x7F

#define LCD1602_LENGTH_LINE				16

#define LCD1602_DDRAM_START_LINE		0x00
#define LCD1602_DDRAM_END_LINE			0x4F
#define LCD1602_DDRAM_START_LINE1		0x00
#define LCD1602_DDRAM_END_LINE1			0x27
#define LCD1602_DDRAM_START_LINE2		0x40
#define LCD1602_DDRAM_END_LINE2			0x67


#define LCD1602_INSTR_REG				0x0
#define LCD1602_DATA_REG				0x1

#define LCD1602_READ_MODE				0x1
#define LCD1602_WRITE_MODE				0x0

#define LCD1602_DATA_CLEAR_DISPLAY		0x1
#define LCD1602_DATA_RETURN_HOME		0x2

#define TIME_FOR_DELAY					5

#define WAIT_TMT						1000

#define TYPE_SHIFT_IS_VALID(X)			(((uint8_t)(X)) < LCD1602_SHIFT_MAX)
#define NUM_LINE_IS_VALID(X)			(((uint8_t)(X)) < LCD1602_NUM_LINE_MAX)
#define FONT_SIZE_IS_VALID(X)			(((uint8_t)(X)) < LCD1602_FONT_SIZE_MAX)
#define TYPE_MOVE_CURSOR_IS_VALID(X)	(((uint8_t)(X)) < LCD1602_CURSOR_MOVE_MAX)
#define SHIFT_ENTIRE_IS_VALID(X)		(((uint8_t)(X)) < LCD1602_SHIFT_ENTIRE_MAX)
#define TYPE_INTERFACE_IS_VALID(X)		(((uint8_t)(X)) < LCD1602_INTERFACE_MAX)
#define DISP_STATE_IS_VALID(X)			(((uint8_t)(X)) < LCD1602_DISPLAY_MAX)
#define CURSOR_STATE_IS_VALID(X)		(((uint8_t)(X)) < LCD1602_CURSOR_MAX)
#define CURSOR_BLINK_STATE_IS_VALID(X)	(((uint8_t)(X)) < LCD1602_CURSOR_BLINK_MAX)

static uint16_t lcd1602_data_pins[] = {GPIO_PIN_15, GPIO_PIN_14, GPIO_PIN_13, GPIO_PIN_7, 
										GPIO_PIN_8, GPIO_PIN_9, GPIO_PIN_12, GPIO_PIN_11, 0};

#define LCD1602_DATA_PINS				(lcd1602_data_pins[0] | lcd1602_data_pins[1] | lcd1602_data_pins[2] | lcd1602_data_pins[3] | \
										 lcd1602_data_pins[4] | lcd1602_data_pins[5] | lcd1602_data_pins[6] | lcd1602_data_pins[7])


static lcd1602_ext_code ext_code = LCD1602_NO_CODES;
static lcd1602_settings_t settings;

static void __lc1602_delay_us(uint32_t delay_us)
{
	uint32_t clock_delay = delay_us * (HAL_RCC_GetSysClockFreq() / 1000000);
	for (; clock_delay; __NOP(), clock_delay--);
}

static uint8_t __lcd1602_read_write(uint8_t *data, uint8_t type_reg, uint8_t type_mode)
{
	if (!data)
		return RES_INVALID_PAR;

	if (type_reg > LCD1602_DATA_REG || type_mode > LCD1602_READ_MODE)
		return RES_INVALID_PAR;

	uint16_t data_u16 = *data;
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* Set RS pin */
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, (GPIO_PinState)type_reg);

	/* Set R/W pin */
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, (GPIO_PinState)type_mode);

	/* Set data pins if necessary */
	if (type_mode == LCD1602_WRITE_MODE) {
		GPIO_InitStruct.Pin = LCD1602_DATA_PINS;
		GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
		GPIO_InitStruct.Pull = GPIO_NOPULL;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;

		gpio_bulk_write(GPIOC, lcd1602_data_pins, data_u16);
		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
	}

	/* Set Enable pin */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
	__lc1602_delay_us(1);

	if (type_mode == LCD1602_READ_MODE) {
		gpio_bulk_read(GPIOC, lcd1602_data_pins, &data_u16);
		*data = (uint8_t)data_u16;
	}

	/* Reset Enable pin */
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	__lc1602_delay_us(2);

	/* Come back to read mode if necessary */
	if (type_mode == LCD1602_WRITE_MODE) {
		GPIO_InitStruct.Pin = LCD1602_DATA_PINS;
		GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
		GPIO_InitStruct.Pull = GPIO_PULLDOWN;
		GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;

		HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
	}

	return RES_OK;
}

static uint8_t __lcd1602_instruction_write(uint8_t instruction)
{
	ext_code = LCD1602_NO_CODES;

	uint8_t res = __lcd1602_read_write(&instruction, LCD1602_INSTR_REG, LCD1602_WRITE_MODE);

	if (res != RES_OK)
		ext_code = LCD1602_INTERFACE_ERROR;

	return res;
}

static uint8_t __lcd1602_read_busy_flag(uint8_t *busy_flag, uint8_t *address_counter)
{
	ext_code = LCD1602_NO_CODES;

	uint8_t read_data = 0;
	uint8_t res = __lcd1602_read_write(&read_data, LCD1602_INSTR_REG, LCD1602_READ_MODE);

	if (res != RES_OK) {
		ext_code = LCD1602_INTERFACE_ERROR;
		return res;
	}

	if (busy_flag)
		*busy_flag = (read_data >> 7) & 0x1;

	if (address_counter)
		*address_counter = read_data & 0x7F;

	return RES_OK;
}

static uint8_t __lcd1602_data_write(uint8_t data)
{
	ext_code = LCD1602_NO_CODES;

	uint8_t res = __lcd1602_read_write(&data, LCD1602_DATA_REG, LCD1602_WRITE_MODE);

	if (res != RES_OK)
		ext_code = LCD1602_INTERFACE_ERROR;

	return res;
}

static uint8_t __lcd1602_data_read(uint8_t *data)
{
	if (!data)
		return RES_INVALID_PAR;

	ext_code = LCD1602_NO_CODES;

	uint8_t res = __lcd1602_read_write(data, LCD1602_DATA_REG, LCD1602_READ_MODE);

	if (res != RES_OK)
		ext_code = LCD1602_INTERFACE_ERROR;

	 return res;
}

static uint8_t __lcd1602_wait(const uint32_t timeout)
{
	uint8_t res = RES_TIMEOUT;

	uint8_t busy_flag = 0;
	uint32_t time_counter = timeout;

	while (time_counter) {
		uint8_t _res = __lcd1602_read_busy_flag(&busy_flag, NULL);

		if (_res != RES_OK) {
			res = _res;
			break;
		}

		if (!busy_flag) {
			res = RES_OK;
			break;
		}

		__lc1602_delay_us(TIME_FOR_DELAY * 1000);//HAL_Delay(TIME_FOR_DELAY);
		time_counter = (time_counter > TIME_FOR_DELAY) ? (time_counter - TIME_FOR_DELAY) : 0;
	}

	return res;
}

uint8_t __lcd1602_function_set(const lcd1602_type_interface_e interface,
							const lcd1602_num_line_e num_line,
							const lcd1602_font_size_e font_size,
							bool wait_bf)
{
	if (!NUM_LINE_IS_VALID(num_line) || 
		!FONT_SIZE_IS_VALID(font_size) || 
		!TYPE_INTERFACE_IS_VALID(interface)) {
		return RES_INVALID_PAR;
	}

	uint8_t write_data = ((uint8_t)interface << 4)|((uint8_t)num_line << 3)|
							((uint8_t)font_size << 2) | 0x20;

	uint8_t res = __lcd1602_instruction_write(write_data);

	if (res == RES_OK && wait_bf) {
		res = __lcd1602_wait(WAIT_TMT);

		if (res == RES_OK) {
			settings.type_interface = interface;
			settings.num_line = num_line;
			settings.font_size = font_size;
		}
	}

	return res;
}

uint8_t lcd1602_init(lcd1602_settings_t *init_settings)
{
	if (!init_settings)
		return RES_INVALID_PAR;

	ext_code = LCD1602_NO_CODES;

	if (!TYPE_MOVE_CURSOR_IS_VALID(init_settings->type_move_cursor) || 
		!SHIFT_ENTIRE_IS_VALID(init_settings->shift_entire_disp) ||
		!DISP_STATE_IS_VALID(init_settings->disp_state) || 
		!CURSOR_STATE_IS_VALID(init_settings->cursor_state) || 
		!CURSOR_BLINK_STATE_IS_VALID(init_settings->cursor_blink_state) ||
		!TYPE_SHIFT_IS_VALID(init_settings->type_shift) ||
		!NUM_LINE_IS_VALID(init_settings->num_line) || 
		!FONT_SIZE_IS_VALID(init_settings->font_size) || 
		!TYPE_INTERFACE_IS_VALID(init_settings->type_interface)) {
		return RES_INVALID_PAR;
	}

	if (init_settings->type_interface == LCD1602_INTERFACE_4BITS)
		return RES_NOT_SUPPORTED;

	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* Enable periphery */
	if (__HAL_RCC_GPIOA_IS_CLK_DISABLED())
		__HAL_RCC_GPIOA_CLK_ENABLE();

	if (__HAL_RCC_GPIOB_IS_CLK_DISABLED())
		__HAL_RCC_GPIOB_CLK_ENABLE();

	if (__HAL_RCC_GPIOC_IS_CLK_DISABLED())
		__HAL_RCC_GPIOC_CLK_ENABLE();

	/* Set E pin */
	GPIO_InitStruct.Pin = GPIO_PIN_6;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
	
	HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	/* Set RS pin */
	GPIO_InitStruct.Pin = GPIO_PIN_0;
	HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, GPIO_PIN_RESET);
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	/* Set R/W pin */
	GPIO_InitStruct.Pin = GPIO_PIN_4;
	HAL_GPIO_WritePin(GPIOC, GPIO_PIN_4, GPIO_PIN_RESET);
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	/* Set data pins */
	GPIO_InitStruct.Pin = LCD1602_DATA_PINS;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_PULLDOWN;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
	
	__lc1602_delay_us(15000);

	/* Initial sequence */
	__lcd1602_function_set(init_settings->type_interface,
						init_settings->num_line,
						init_settings->font_size, false);

	__lc1602_delay_us(5000);

	__lcd1602_function_set(init_settings->type_interface,
						init_settings->num_line,
						init_settings->font_size, false);

	__lc1602_delay_us(200);

	__lcd1602_function_set(init_settings->type_interface,
						init_settings->num_line,
						init_settings->font_size, false);

	uint8_t res = __lcd1602_function_set(init_settings->type_interface,
										init_settings->num_line,
										init_settings->font_size, true);

	if (res != RES_OK)
		return res;

	res = lcd1602_display_on_off(LCD1602_DISPLAY_OFF, LCD1602_CURSOR_OFF, LCD1602_CURSOR_BLINK_OFF);

	if (res != RES_OK)
		return res;

	res = lcd1602_display_clear();

	if(res != RES_OK)
		return res;

	res = lcd1602_entry_mode_set(init_settings->type_move_cursor, init_settings->shift_entire_disp);

	if (res != RES_OK)
		return res;

	res = lcd1602_display_on_off(init_settings->disp_state, 
								init_settings->cursor_state, 
								init_settings->cursor_blink_state);

	if (res != RES_OK)
		return res;

	//res = lcd1602_cursor_disp_shift(init_settings->type_shift);

	return res;
}

uint8_t lcd1602_display_clear(void)
{
	uint8_t res = __lcd1602_instruction_write(LCD1602_DATA_CLEAR_DISPLAY);

	if (res == RES_OK)
		return __lcd1602_wait(WAIT_TMT);

	return res;
}

uint8_t lcd1602_return_home(void)
{
	uint8_t res = __lcd1602_instruction_write(LCD1602_DATA_RETURN_HOME);

	if (res == RES_OK)
		return __lcd1602_wait(WAIT_TMT);

	return res;
}

uint8_t lcd1602_entry_mode_set( const lcd1602_type_move_cursor_e cursor,
								const lcd1602_shift_entire_disp_e shift_entire)
{
	if (!TYPE_MOVE_CURSOR_IS_VALID(cursor) || !SHIFT_ENTIRE_IS_VALID(shift_entire))
		return RES_INVALID_PAR;

	uint8_t write_data = ((uint8_t)cursor << 1) | ((uint8_t)shift_entire) | 0x4;
	uint8_t res = __lcd1602_instruction_write(write_data);

	if (res == RES_OK) {
		res = __lcd1602_wait(WAIT_TMT);

		if (res == RES_OK) {
			settings.type_move_cursor = cursor;
			settings.shift_entire_disp = shift_entire;
		}
	}

	return res;
}

uint8_t lcd1602_display_on_off(const lcd1602_disp_state_e disp_state,
								const lcd1602_cursor_state_e cursor_state, 
								const lcd1602_cursor_blink_state_e cursor_blink_state)
{
	if (!DISP_STATE_IS_VALID(disp_state) || 
		!CURSOR_STATE_IS_VALID(cursor_state) || 
		!CURSOR_BLINK_STATE_IS_VALID(cursor_blink_state)) {
		return RES_INVALID_PAR;
	}

	uint8_t write_data = ((uint8_t)disp_state << 2) | ((uint8_t)cursor_state << 1) | ((uint8_t)cursor_blink_state) | 0x8;

	uint8_t res = __lcd1602_instruction_write(write_data);

	if (res == RES_OK) {
		res = __lcd1602_wait(WAIT_TMT);

		if (res == RES_OK) {
			settings.disp_state = disp_state;
			settings.cursor_state = cursor_state;
			settings.cursor_blink_state = cursor_blink_state;
		}
	}
 
	return res;
}

uint8_t lcd1602_cursor_disp_shift(const lcd1602_type_shift_e shift)
{
	if (!TYPE_SHIFT_IS_VALID(shift))
		return RES_INVALID_PAR;

	uint8_t write_data = ((uint8_t)shift << 2) | 0x10;

	uint8_t res = __lcd1602_instruction_write(write_data);

	if (res == RES_OK) {
		res = __lcd1602_wait(WAIT_TMT);

		if (res == RES_OK)
			settings.type_shift = shift;
	}

	return res;
}

uint8_t lcd1602_cgram_address_set(const uint8_t address)
{
	if (address > MAX_CGRAM_ADDRESS)
		return RES_INVALID_PAR;

	uint8_t write_data = address | 0x40;

	uint8_t res = __lcd1602_instruction_write(write_data);

	if (res == RES_OK)
		return __lcd1602_wait(WAIT_TMT);

	return res;
}

uint8_t lcd1602_ddram_address_set(const uint8_t address)
{
	if (address > MAX_DDRAM_ADDRESS)
		return RES_INVALID_PAR;

	uint8_t write_data = address | 0x80;

	uint8_t res = __lcd1602_instruction_write(write_data);

	if (res == RES_OK)
		return __lcd1602_wait(WAIT_TMT);

	return res;
}

uint8_t lcd1602_printf(const char* line1, const char* line2, ...)
{
	va_list args;

	if (!line1 && !line2)
		return RES_INVALID_PAR;

	if (line2 && settings.num_line != LCD1602_NUM_LINE_2)
		return RES_NOT_SUPPORTED;

	va_start(args, line2);

	char first_line[LCD1602_LENGTH_LINE + 1] = {0};
	char second_line[LCD1602_LENGTH_LINE + 1] = {0};

	if (line1) {
		if (vsnprintf(first_line, LCD1602_LENGTH_LINE + 1, line1, args) < 0) {
			va_end(args);
			return RES_NOK;
		}
	}

	if (line2) {
		if (vsnprintf(second_line, LCD1602_LENGTH_LINE + 1, line2, args) < 0) {
			va_end(args);
			return RES_NOK;
		}
	}

	va_end(args);

	uint8_t res = lcd1602_display_clear();

	if(res != RES_OK)
		return res;

	if (line1) {
		res = lcd1602_ddram_address_set(0x0);

		if(res != RES_OK)
			return res;

		for (uint8_t i = 0; i < strlen(first_line); i++) {
			__lcd1602_data_write((uint8_t)first_line[i]);
			__lcd1602_wait(WAIT_TMT);
		}
	}

	if (line2) {
		res = lcd1602_ddram_address_set(LCD1602_DDRAM_START_LINE2);

		if(res != RES_OK)
			return res;

		for (uint8_t i = 0; i < strlen(second_line); i++) {
			__lcd1602_data_write((uint8_t)second_line[i]);
			__lcd1602_wait(WAIT_TMT);
		}
	}

	return res;
}