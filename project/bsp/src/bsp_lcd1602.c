/**
\file
\author JavaLandau
\copyright MIT License
\brief BSP LCD1602 module

The file includes implementation of BSP layer of the LCD1602 display
*/

#include <stdarg.h>
#include <string.h>
#include <stdio.h>

#include "bsp_lcd1602.h"
#include "bsp_gpio.h"
#include "common.h"
#include "stm32f4xx_hal.h"
#include <stdbool.h>

/** 
 * \defgroup bsp_lcd1602 BSP LCD1602
 * \brief Module of BSP <a href="https://www.openhacks.com/uploadsproductos/eone-1602a1.pdf">LCD1602</a>
 * 
 *      The module does communication with display LCD1602 via 8-bit parallel interface using GPIO
 * \ingroup bsp
 * @{
*/

/// Maximum address of CGRAM memory
#define MAX_CGRAM_ADDRESS               0x3F

/// Maximum address of DDRAM memory
#define MAX_DDRAM_ADDRESS               0x7F

/// Length of the line of LCD1602 in symbols
#define LCD1602_LENGTH_LINE             16

/// Maximum length of buffered string used within the module
#define LCD1602_MAX_STR_LEN             (4 * LCD1602_LENGTH_LINE)

/// DDRAM address of start of first line
#define LCD1602_DDRAM_START_LINE1       0x00

/// DDRAM address of end of first line (display is used in 2-line mode)
#define LCD1602_DDRAM_END_LINE1         0x27

/// DDRAM address of start of second line
#define LCD1602_DDRAM_START_LINE2       0x40

/// DDRAM address of end of second line
#define LCD1602_DDRAM_END_LINE2         0x67

/// Level on signal RS to choose instruction register
#define LCD1602_INSTR_REG               0x0

/// Level on signal RS to choose data register
#define LCD1602_DATA_REG                0x1

/// Level on signal R/W to set read mode
#define LCD1602_READ_MODE               0x1

/// Level on signal R/W to set write mode
#define LCD1602_WRITE_MODE              0x0

/// Time delay in us while waiting for BUSY flag, used in \ref __lcd1602_wait
#define TIME_FOR_DELAY                  1

/// Timeout in ms for waiting for BUSY flag
#define WAIT_TMT                        500

/** MACRO Type shift is valid
 * 
 * The macro decides whether \p X is valid type shift
 *
 * \param[in] X type shift
 * \return true if \p X is valid type shift false otherwise
*/
#define TYPE_SHIFT_IS_VALID(X)          (((uint8_t)(X)) < LCD1602_SHIFT_MAX)

/** MACRO Number of line is valid
 * 
 * The macro decides whether \p X is valid number of line
 *
 * \param[in] X number of line
 * \return true if \p X is valid number of line false otherwise
*/
#define NUM_LINE_IS_VALID(X)            (((uint8_t)(X)) < LCD1602_NUM_LINE_MAX)

/** MACRO Font size is valid
 * 
 * The macro decides whether \p X is valid font size
 *
 * \param[in] X font size
 * \return true if \p X is valid font size false otherwise
*/
#define FONT_SIZE_IS_VALID(X)           (((uint8_t)(X)) < LCD1602_FONT_SIZE_MAX)

/** MACRO Move type of cursor is valid
 * 
 * The macro decides whether \p X is valid move type of cursor
 *
 * \param[in] X move type of cursor
 * \return true if \p X is valid move type of cursor false otherwise
*/
#define TYPE_MOVE_CURSOR_IS_VALID(X)    (((uint8_t)(X)) < LCD1602_CURSOR_MOVE_MAX)

/** MACRO Type shift of entire display is valid
 * 
 * The macro decides whether \p X is valid type shift of entire display
 *
 * \param[in] X type shift of entire display
 * \return true if \p X is valid type shift of entire display false otherwise
*/
#define SHIFT_ENTIRE_IS_VALID(X)        (((uint8_t)(X)) < LCD1602_SHIFT_ENTIRE_MAX)

/** MACRO Type interface is valid
 * 
 * The macro decides whether \p X is valid type interface
 *
 * \param[in] X type interface
 * \return true if \p X is valid type interface false otherwise
*/
#define TYPE_INTERFACE_IS_VALID(X)      (((uint8_t)(X)) < LCD1602_INTERFACE_MAX)

/** MACRO Display state is valid
 * 
 * The macro decides whether \p X is valid display state
 *
 * \param[in] X display state
 * \return true if \p X is valid display state false otherwise
*/
#define DISP_STATE_IS_VALID(X)          (((uint8_t)(X)) < LCD1602_DISPLAY_MAX)

/** MACRO Cursor state is valid
 * 
 * The macro decides whether \p X is valid cursor state
 *
 * \param[in] X cursor state
 * \return true if \p X is valid cursor state false otherwise
*/
#define CURSOR_STATE_IS_VALID(X)        (((uint8_t)(X)) < LCD1602_CURSOR_MAX)

/** MACRO Cursor blink state is valid
 * 
 * The macro decides whether \p X is valid cursor blink state
 *
 * \param[in] X cursor blink state
 * \return true if \p X is valid cursor blink state false otherwise
*/
#define CURSOR_BLINK_STATE_IS_VALID(X)  (((uint8_t)(X)) < LCD1602_CURSOR_BLINK_MAX)


/// Array of GPIO pins used for 8-bit parallel interface
static const uint16_t lcd1602_data_pins[] = {GPIO_PIN_15, GPIO_PIN_14, GPIO_PIN_13, GPIO_PIN_7, 
                                             GPIO_PIN_8, GPIO_PIN_9, GPIO_PIN_12, GPIO_PIN_11, 0};

/// All mixed GPIO pins from \ref lcd1602_data_pins, used for (de-)initalizating purposes
#define LCD1602_DATA_PINS               (lcd1602_data_pins[0] | lcd1602_data_pins[1] | lcd1602_data_pins[2] | lcd1602_data_pins[3] | \
                                        lcd1602_data_pins[4] | lcd1602_data_pins[5] | lcd1602_data_pins[6] | lcd1602_data_pins[7])

/// Local copy of display settings
static struct lcd1602_settings settings;

/** Read/write operation with LCD1602
 * 
 * \param[in,out] data read/written value from/to insturction/data register
 * \param[in] type_reg type of register, can be \ref LCD1602_INSTR_REG or \ref LCD1602_DATA_REG
 * \param[in] type_mode read or write mode, can be \ref LCD1602_READ_MODE or \ref LCD1602_WRITE_MODE
 * \return \ref RES_OK on success error otherwise
 */
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

        bsp_gpio_bulk_write(GPIOC, (uint16_t*)lcd1602_data_pins, data_u16);
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
    }

    /* Set Enable pin */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_SET);
    INSTR_DELAY_US(1);

    if (type_mode == LCD1602_READ_MODE) {
        bsp_gpio_bulk_read(GPIOC, (uint16_t*)lcd1602_data_pins, &data_u16);
        *data = (uint8_t)data_u16;
    }

    /* Reset Enable pin */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, GPIO_PIN_RESET);
    INSTR_DELAY_US(2);

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

/** Write instruction to LCD1602
 * 
 * \param[in] instruction value of insturction register
 * \return \ref RES_OK on success error otherwise
 */
static uint8_t __lcd1602_instruction_write(uint8_t instruction)
{
    return __lcd1602_read_write(&instruction, LCD1602_INSTR_REG, LCD1602_WRITE_MODE);
}

/** Read BUSY flag from LCD1602
 * 
 * \param[out] busy_flag read BUSY flag, if NULL - not returned
 * \param[out] address_counter read address counter, if NULL - not returned
 * \return \ref RES_OK on success error otherwise
 */
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

/** Write data to LCD1602
 * 
 * \param[in] data value of data register
 * \return \ref RES_OK on success error otherwise
 */
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

/** Wait for finish LCD1602 operation
 * 
 * The function waits when BUSY flag is reset,  
 * used to ensure that display is ready for next operation
 * 
 * \param[in] timeout timeout for waiting in ms
 * \return \ref RES_OK on success error otherwise
 */
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

/* Set function to LCD1602, see header file for details */
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

/* LCD1602 initialization, see header file for details */
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

/* LCD1602 deinitialization, see header file for details */
uint8_t bsp_lcd1602_deinit(void)
{
    /* Display clear */
    uint8_t res = bsp_lcd1602_display_clear();

    if(res != RES_OK)
        return res;

    /* MSP deinitialization */
    HAL_GPIO_DeInit(GPIOA, GPIO_PIN_6);
    HAL_GPIO_DeInit(GPIOB, GPIO_PIN_0);
    HAL_GPIO_DeInit(GPIOC, LCD1602_DATA_PINS | GPIO_PIN_4);

    return RES_OK;
}

/* Display clear, see header file for details */
uint8_t bsp_lcd1602_display_clear(void)
{
    uint8_t res = __lcd1602_instruction_write(0x1);

    if (res == RES_OK)
        return __lcd1602_wait(WAIT_TMT);

    return res;
}

/* Return home, see header file for details */
uint8_t bsp_lcd1602_return_home(void)
{
    uint8_t res = __lcd1602_instruction_write(0x2);

    if (res == RES_OK)
        return __lcd1602_wait(WAIT_TMT);

    return res;
}

/* Entry mode set, see header file for details */
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

/* Display ON\OFF, see header file for details */
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

/* Cursor or display shift, see header file for details */
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

/* CGRAM address set, see header file for details */
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

/* DDRAM address set, see header file for details */
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

/** Print on display
 * 
 * The function prints formatted strings on LCD1602
 * 
 * \param[in] line1 formatted first line, if NULL - previous content remains
 * \param[in] line2 formatted second line, if NULL - previous content remains
 * \param[in] is_centered flag whether content within each line should be centered
 * \param[in] argp formatting arguments over two lines
 * \return \ref RES_OK on success error otherwise
 */
static uint8_t __lcd1602_printf(const char *line1, const char *line2, bool is_centered, va_list argp)
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

    uint8_t res = bsp_lcd1602_ddram_address_set(LCD1602_DDRAM_START_LINE1);

    if (res != RES_OK)
        return res;

    pos = 0;
    for (uint8_t i = 0; ; i++) {
        if (disp_lines[i] == '\33' || !disp_lines[i]) {
            const char *line = disp_lines[i] ? line1 : line2;

            if (line) {
                for (uint8_t j = 0; j < (LCD1602_LENGTH_LINE - (i - pos)); j++) {
                    __lcd1602_data_write(' ');
                    __lcd1602_wait(WAIT_TMT);
                }
            }
            pos = i + 1;

            if (!disp_lines[i])
                break;

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

/* Not centered print on display, see header file for details */
uint8_t bsp_lcd1602_printf(const char *line1, const char *line2, ...)
{
    va_list argp;
    va_start(argp, line2);

    uint8_t res = __lcd1602_printf(line1, line2, false, argp);

    va_end(argp);

    return res;
}

/* Centered print on display, see header file for details */
uint8_t bsp_lcd1602_cprintf(const char *line1, const char *line2, ...)
{
    va_list argp;
    va_start(argp, line2);

    uint8_t res = __lcd1602_printf(line1, line2, true, argp);

    va_end(argp);

    return res;
}

/** @} */