#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "bsp_lcd1602.h"
#include "bsp_gpio.h"
#include "common.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>

#define MAX_CGRAM_ADDRESS               0x3F
#define MAX_DDRAM_ADDRESS               0x7F

#define LCD1602_LENGTH_LINE             16
#define LCD1602_MAX_STR_LEN             (4 * LCD1602_LENGTH_LINE)

#define LCD1602_DDRAM_START_LINE        0x00
#define LCD1602_DDRAM_END_LINE          0x4F
#define LCD1602_DDRAM_START_LINE1       0x00
#define LCD1602_DDRAM_END_LINE1         0x27
#define LCD1602_DDRAM_START_LINE2       0x40
#define LCD1602_DDRAM_END_LINE2         0x67

#define LCD1602_INSTR_REG               0x0
#define LCD1602_DATA_REG                0x1

#define LCD1602_READ_MODE               0x1
#define LCD1602_WRITE_MODE              0x0

#define LCD1602_DATA_CLEAR_DISPLAY      0x1
#define LCD1602_DATA_RETURN_HOME        0x2

#define TIME_FOR_DELAY                  1

#define WAIT_TMT                        500

#define TYPE_SHIFT_IS_VALID(X)          (((uint8_t)(X)) < LCD1602_SHIFT_MAX)
#define NUM_LINE_IS_VALID(X)            (((uint8_t)(X)) < LCD1602_NUM_LINE_MAX)
#define FONT_SIZE_IS_VALID(X)           (((uint8_t)(X)) < LCD1602_FONT_SIZE_MAX)
#define TYPE_MOVE_CURSOR_IS_VALID(X)    (((uint8_t)(X)) < LCD1602_CURSOR_MOVE_MAX)
#define SHIFT_ENTIRE_IS_VALID(X)        (((uint8_t)(X)) < LCD1602_SHIFT_ENTIRE_MAX)
#define TYPE_INTERFACE_IS_VALID(X)      (((uint8_t)(X)) < LCD1602_INTERFACE_MAX)
#define DISP_STATE_IS_VALID(X)          (((uint8_t)(X)) < LCD1602_DISPLAY_MAX)
#define CURSOR_STATE_IS_VALID(X)        (((uint8_t)(X)) < LCD1602_CURSOR_MAX)
#define CURSOR_BLINK_STATE_IS_VALID(X)  (((uint8_t)(X)) < LCD1602_CURSOR_BLINK_MAX)

static uint16_t lcd1602_data_pins[] = {GPIO_PIN_15, GPIO_PIN_14, GPIO_PIN_13, GPIO_PIN_7, 
                                        GPIO_PIN_8, GPIO_PIN_9, GPIO_PIN_12, GPIO_PIN_11, 0};

#define LCD1602_DATA_PINS               (lcd1602_data_pins[0] | lcd1602_data_pins[1] | lcd1602_data_pins[2] | lcd1602_data_pins[3] | \
                                        lcd1602_data_pins[4] | lcd1602_data_pins[5] | lcd1602_data_pins[6] | lcd1602_data_pins[7])

static struct lcd1602_settings settings;

static void __lc1602_delay_us(uint32_t delay_us)
{
    __IO uint32_t clock_delay = delay_us * (HAL_RCC_GetSysClockFreq() / 8 / 1000000);
    do {
        __NOP();
    } while (--clock_delay);
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

        bsp_gpio_bulk_write(GPIOC, lcd1602_data_pins, data_u16);
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    }

    /* Set Enable pin */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
    __lc1602_delay_us(1);

    if (type_mode == LCD1602_READ_MODE) {
        bsp_gpio_bulk_read(GPIOC, lcd1602_data_pins, &data_u16);
        *data = (uint8_t)data_u16;
    }

    /* Reset Enable pin */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
    __lc1602_delay_us(2);

    /* Come back to read mode if necessary */
    if (type_mode == LCD1602_WRITE_MODE) {
        GPIO_InitStruct.Pin = LCD1602_DATA_PINS;
        GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;

        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    }

    return RES_OK;
}

static uint8_t __lcd1602_instruction_write(uint8_t instruction)
{
    return __lcd1602_read_write(&instruction, LCD1602_INSTR_REG, LCD1602_WRITE_MODE);
}

static uint8_t __lcd1602_read_busy_flag(uint8_t *busy_flag, uint8_t *address_counter)
{
    uint8_t read_data = 0;
    uint8_t res = __lcd1602_read_write(&read_data, LCD1602_INSTR_REG, LCD1602_READ_MODE);

    if (res != RES_OK)
        return res;

    if (busy_flag)
        *busy_flag = (read_data >> 7) & 0x1;

    if (address_counter)
        *address_counter = read_data & 0x7F;

    return RES_OK;
}

static uint8_t __lcd1602_data_write(uint8_t data)
{
    return __lcd1602_read_write(&data, LCD1602_DATA_REG, LCD1602_WRITE_MODE);
}

/*static uint8_t __lcd1602_data_read(uint8_t *data)
{
    if (!data)
        return RES_INVALID_PAR;

    return __lcd1602_read_write(data, LCD1602_DATA_REG, LCD1602_READ_MODE);
}*/

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

        HAL_Delay(TIME_FOR_DELAY);
        time_counter = (time_counter > TIME_FOR_DELAY) ? (time_counter - TIME_FOR_DELAY) : 0;
    }

    return res;
}

uint8_t bsp_lcd1602_function_set(const enum lcd1602_type_interface interface,
                                const enum lcd1602_num_line num_line,
                                const enum lcd1602_font_size font_size)
{
    if (!NUM_LINE_IS_VALID(num_line) || 
        !FONT_SIZE_IS_VALID(font_size) || 
        !TYPE_INTERFACE_IS_VALID(interface)) {
        return RES_INVALID_PAR;
    }

    uint8_t write_data = ((uint8_t)interface << 4) | ((uint8_t)num_line << 3) |
                        ((uint8_t)font_size << 2) | 0x20;

    uint8_t res = __lcd1602_instruction_write(write_data);

    if (res == RES_OK) {
        res = __lcd1602_wait(WAIT_TMT);

        if (res == RES_OK) {
            settings.type_interface = interface;
            settings.num_line = num_line;
            settings.font_size = font_size;
        }
    }

    return res;
}

uint8_t bsp_lcd1602_init(struct lcd1602_settings *init_settings)
{
    if (!init_settings)
        return RES_INVALID_PAR;

    if (!TYPE_MOVE_CURSOR_IS_VALID(init_settings->type_move_cursor) || 
        !SHIFT_ENTIRE_IS_VALID(init_settings->shift_entire_disp) ||
        !DISP_STATE_IS_VALID(init_settings->disp_state) || 
        !CURSOR_STATE_IS_VALID(init_settings->cursor_state) || 
        !CURSOR_BLINK_STATE_IS_VALID(init_settings->cursor_blink_state) ||
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
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_MEDIUM;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* Check connection with display */
    uint8_t res = __lcd1602_wait(WAIT_TMT);

    if (res != RES_OK)
        return res;

    /* Make configuration */
    res = bsp_lcd1602_function_set(init_settings->type_interface,
                                    init_settings->num_line,
                                    init_settings->font_size);

    if (res != RES_OK)
        return res;

    res = bsp_lcd1602_display_clear();

    if(res != RES_OK)
        return res;

    res = bsp_lcd1602_entry_mode_set(init_settings->type_move_cursor,
                                    init_settings->shift_entire_disp);

    if (res != RES_OK)
        return res;

    res = bsp_lcd1602_display_on_off(init_settings->disp_state, 
                                    init_settings->cursor_state, 
                                    init_settings->cursor_blink_state);

    if (res != RES_OK)
        return res;

    return res;
}

uint8_t bsp_lcd1602_display_clear(void)
{
    uint8_t res = __lcd1602_instruction_write(LCD1602_DATA_CLEAR_DISPLAY);

    if (res == RES_OK)
        return __lcd1602_wait(WAIT_TMT);

    return res;
}

uint8_t bsp_lcd1602_return_home(void)
{
    uint8_t res = __lcd1602_instruction_write(LCD1602_DATA_RETURN_HOME);

    if (res == RES_OK)
        return __lcd1602_wait(WAIT_TMT);

    return res;
}

uint8_t bsp_lcd1602_entry_mode_set(const enum lcd1602_type_move_cursor cursor,
                                   const enum lcd1602_shift_entire_disp shift_entire)
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

uint8_t bsp_lcd1602_display_on_off(const enum lcd1602_disp_state disp_state,
                                   const enum lcd1602_cursor_state cursor_state,
                                   const enum lcd1602_cursor_blink_state cursor_blink_state)
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

uint8_t bsp_lcd1602_cursor_disp_shift(const enum lcd1602_type_shift shift)
{
    if (!TYPE_SHIFT_IS_VALID(shift))
        return RES_INVALID_PAR;

    uint8_t write_data = ((uint8_t)shift << 2) | 0x10;

    uint8_t res = __lcd1602_instruction_write(write_data);

    if (res == RES_OK)
        res = __lcd1602_wait(WAIT_TMT);

    return res;
}

uint8_t bsp_lcd1602_cgram_address_set(const uint8_t address)
{
    if (address > MAX_CGRAM_ADDRESS)
        return RES_INVALID_PAR;

    uint8_t write_data = address | 0x40;

    uint8_t res = __lcd1602_instruction_write(write_data);

    if (res == RES_OK)
        return __lcd1602_wait(WAIT_TMT);

    return res;
}

uint8_t bsp_lcd1602_ddram_address_set(const uint8_t address)
{
    if (address > MAX_DDRAM_ADDRESS)
        return RES_INVALID_PAR;

    uint8_t write_data = address | 0x80;

    uint8_t res = __lcd1602_instruction_write(write_data);

    if (res == RES_OK)
        return __lcd1602_wait(WAIT_TMT);

    return res;
}

uint8_t __lcd1602_printf(const char *line1, const char *line2, bool is_centered, va_list argp)
{
    uint32_t line1_len = strnlen(line1, LCD1602_MAX_STR_LEN);
    uint32_t line2_len = strnlen(line2, LCD1602_MAX_STR_LEN);

    if (!line1_len && !line2_len)
        return RES_INVALID_PAR;

    if (line2_len && settings.num_line != LCD1602_NUM_LINE_2)
        return RES_NOT_SUPPORTED;

    for (uint32_t i = 0; i < MAX(line1_len, line2_len); i++) {
        if (i < line1_len && !IS_PRINTABLE(line1[i]))
            return RES_NOT_SUPPORTED;

        if (i < line2_len && !IS_PRINTABLE(line2[i]))
            return RES_NOT_SUPPORTED;
    }

    uint8_t pos = 0;
    char lines[2 * LCD1602_MAX_STR_LEN + 2] = {0};
    char disp_lines[2 * LCD1602_LENGTH_LINE + 2] = {0};

    if (line1_len) {
        memcpy(&lines[pos], line1, line1_len);
        pos += line1_len;
    }
    lines[pos++] = '\33';

    if (line2_len)
        memcpy(&lines[pos], line2, line2_len);

    if (vsnprintf(disp_lines, sizeof(disp_lines), lines, argp) < 0)
        return RES_NOK;

    char *line_border = strstr(disp_lines, "\33") ;

    if (!line_border)
        return RES_NOK;

    if ((uint32_t)(line_border - disp_lines) > LCD1602_LENGTH_LINE)
        return RES_NOK;

    if ((uint32_t)(&disp_lines[strlen(disp_lines) - 1] - line_border) > LCD1602_LENGTH_LINE)
        return RES_NOK;

    if (is_centered) {
        pos = 0;
        uint32_t border_pos = (uint32_t)(line_border - disp_lines);

        for (uint8_t i = 0; i < LCD1602_NUM_LINE_MAX; i++) {
            if (border_pos - pos) {
                uint32_t shift_size = (LCD1602_LENGTH_LINE - (border_pos - pos)) / 2;

                memmove(&disp_lines[pos + shift_size], &disp_lines[pos], strlen(disp_lines) - pos);
                memset(&disp_lines[pos], ' ', shift_size);
                border_pos += shift_size;

                memmove(&disp_lines[border_pos + shift_size], &disp_lines[border_pos], strlen(disp_lines) - border_pos);
                memset(&disp_lines[border_pos], ' ', shift_size);
                border_pos += shift_size;
            }

            pos = border_pos + 1;
            border_pos = strlen(disp_lines);
        }
    }

    uint8_t res = bsp_lcd1602_display_clear();

    if (res != RES_OK)
        return res;

    res = bsp_lcd1602_ddram_address_set(LCD1602_DDRAM_START_LINE1);

    if (res != RES_OK)
        return res;

    for (uint8_t i = 0; i < strlen(disp_lines); i++) {
        if (disp_lines[i] == '\33') {
            res = bsp_lcd1602_ddram_address_set(LCD1602_DDRAM_START_LINE2);

            if (res != RES_OK)
                return res;
        } else {
            __lcd1602_data_write((uint8_t)disp_lines[i]);
            __lcd1602_wait(WAIT_TMT);
        }
    }

    return res;
}

uint8_t bsp_lcd1602_printf(const char *line1, const char *line2, ...)
{
    va_list argp;
    va_start(argp, line2);

    uint8_t res = __lcd1602_printf(line1, line2, false, argp);

    va_end(argp);

    return res;
}

uint8_t bsp_lcd1602_cprintf(const char *line1, const char *line2, ...)
{
    va_list argp;
    va_start(argp, line2);

    uint8_t res = __lcd1602_printf(line1, line2, true, argp);

    va_end(argp);

    return res;
}