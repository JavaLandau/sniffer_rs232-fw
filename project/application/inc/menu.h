/**
\file
\author JavaLandau
\copyright MIT License
\brief Header of menu library
*/

#ifndef __MENU_H__
#define __MENU_H__

#include "common.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** 
 * \addtogroup menu
 * @{
*/

/// Maximum valid length of strings used within menu library
#define MENU_MAX_STR_LEN                256

/// Escape sequence to reset console colors
#define MENU_COLOR_RESET                "\33[0;37;40m"

/// Escape sequence to return cursor to left top corner of console
#define MENU_RETURN_HOME                "\33[H"

/// Escape sequence to move cursor one line up
#define MENU_LINE_UP                    "\33[A"

/// Escape sequence to move cursor one line down
#define MENU_LINE_DOWN                  "\33[B"

/// Escape sequence to erase current line
#define MENU_LINE_ERASE                 "\33[2K"

/// Escape sequence to erase screen of console
#define MENU_SCREEN_ERASE               "\33[2J"

/// Menu colors
enum menu_color_type {
    MENU_COLOR_BLACK = 0,       ///< Black
    MENU_COLOR_RED,             ///< Red
    MENU_COLOR_GREEN,           ///< Green
    MENU_COLOR_YELLOW,          ///< Yellow
    MENU_COLOR_BLUE,            ///< Blue
    MENU_COLOR_MAGENTA,         ///< Magenta
    MENU_COLOR_CYAN,            ///< Cyan
    MENU_COLOR_WHITE,           ///< White
    MENU_COLOR_MAX              ///< Count of menu colors
};

/// Type of passing input to \ref menu_item::callback
enum menu_pass_type {
    MENU_PASS_NONE = 0,         ///< No passed input
    MENU_PASS_WITH_PROMPT,      ///< Input passed only if current menu item has prompt
    MENU_PASS_ALWAYS,           ///< Input always passed
    MENU_PASS_MAX               ///< Count of passing types
};

/// Numbering types
enum menu_num_type {
    MENU_NUM_NONE = 0,          ///< List of menu items is not numbered
    MENU_NUM_DIGITAL,           ///< List of menu items is numbered by numbers
    MENU_NUM_UPPER_LETTER,      ///< List of menu items is numbered by letters A..Z
    MENU_NUM_LOWER_LETTER,      ///< List of menu items is numbered by letters a..z
    MENU_NUM_MAX                ///< Count of numbering types
};

/// Menu color data
struct menu_color {
    enum menu_color_type foreground;    ///< Color of foreground
    enum menu_color_type background;    ///< Color of background
};

/// Menu color settings
struct menu_color_config {
    struct menu_color active;           ///< Colors of selected menu item
    struct menu_color inactive;         ///< Colors of not selected menu item
};

/// Menu item context
struct menu_item {
    struct menu_item *next;                         ///< Next menu item in order
    struct menu_item *prev;                         ///< Previous menu item in order

    /// Menu context
    struct menu {
        char *label;                                ///< Label of menu
        char filler;                                ///< Filler for label of menu
        struct menu_color_config color_config;      ///< Color settings of menu
        struct menu_item *items;                    ///< Menu items which menu includes
        struct menu *next;                          ///< Next menu in \ref menu_list
    } *menu_entry;                                  ///< Menu to which user can enter from menu item

    uint8_t (*callback) (char *input, void *param); ///< User callback called by actions on menu item
    void *param;                                    ///< Optional parameters passed to \ref menu_item::callback
    char *prompt;                                   ///< Prompt of menu item
    char *label;                                    ///< Label of menu item
    char *value_left_border;                        ///< Left border for value of menu item
    char *value_right_border;                       ///< Right border for value of menu item
    char *value;                                    ///< Value of menu item
    uint32_t value_len;                             ///< Length of value of menu item
};

/// Menu library settings
struct menu_config {
    /** Flag whether list of menu items is looped:
     * position from first item moves to last one by moving up &  
     * position from last item moves to first one by moving down
    */
    bool is_looped;
    /** Width of menu in symbols */
    uint32_t width;
    /** With of vertical indent in symbols */
    uint32_t indent;
    /** Type of passing input */
    enum menu_pass_type pass_type;
    /** Numbering type */
    enum menu_num_type num_type;
    /** Delimiter between enumerator and label of menu item */
    char num_delim;
    /** Callback to provide reading from console */
    uint8_t (*read_callback) (char **read_str);
    /** Callback to provide writing to console */
    uint8_t (*write_callback) (char *write_str);
};

/** MACRO Default Menu color settings
 * 
 * \result initializer for \ref menu_color_config
*/
#define MENU_COLOR_CONFIG_DEFAULT()  {\
    .active = {.foreground = MENU_COLOR_BLUE, .background = MENU_COLOR_WHITE},\
    .inactive = {.foreground = MENU_COLOR_WHITE, .background = MENU_COLOR_BLUE}\
}

/** Free all allocated memory
 * 
 * The function frees all allocated memory within menu library
 */
void menu_all_destroy(void);

/** Create menu
 * 
 * The function creates new menu, adds it to \ref menu_list
 * 
 * \param[in] label label of menu
 * \param[in] filler filler for label of menu to fill rest part of \ref menu_config::width
 * \param[in] color_config color settings of new menu
 * \return new menu on success NULL otherwise
 */
struct menu *menu_create(char *label, char filler, struct menu_color_config *color_config);

/** Menu entry
 * 
 * The function executes entry to new menu (set as current one).
 * \note If \p menu is NULL menu from \ref menu_item::menu_entry is used
 * 
 * \param[in] menu new menu
 * \return \ref RES_OK on success error otherwise
 */
uint8_t menu_entry(struct menu *menu);

/** Set value of menu item
 * 
 * \param[in] menu_item menu item
 * \param[in] value set value
 * \return \ref RES_OK on success error otherwise
 */
uint8_t menu_item_value_set(struct menu_item *menu_item, const char *value);

/** Get current menu item of current menu
 * 
 * \return current menu item equaled to \ref cur_item
 */
struct menu_item *menu_current_item_get(void);

/** Get label of menu item
 * 
 * \param[in] menu_item menu item
 * \return label of menu item on success NULL otherwise
 */
char *menu_item_label_get(struct menu_item *menu_item);

/** Get menu by label
 * 
 * \param[in] label label of menu
 * \return menu on success NULL otherwise
 */
struct menu *menu_by_label_get(const char *label);

/** Get menu item from menu by label
 * 
 * \param[in] menu menu from which menu item is got
 * \param[in] label label of menu item
 * \return menu item on success NULL otherwise
 */
struct menu_item *menu_item_by_label_get(struct menu *menu, const char *label);

/** Get menu item within all menus by label
 * 
 * The function seeks menu item among all existed menus  
 * by combined label "<Menu label>\<Menu item label>"
 * 
 * \param[in] label combined label
 * \return menu item on success NULL otherwise
 */
struct menu_item *menu_item_by_label_only_get(const char *label);

/** Start status of menu library
 * 
 * \return true if menu library is started false otherwise
 */
bool menu_is_started(void);

/** Start menu library
 * 
 * \note The function makes menu routine until \ref __exit is set  
 * by one of the user callbacks \ref menu_item::callback
 * 
 * \param[in] config menu library settings
 * \param[in] menu start menu
 * \return \ref RES_OK on success error otherwise
 */
uint8_t menu_start(struct menu_config *config, struct menu *menu);

/** Exit menu library
 * 
 * The function closes console menu by using \ref __exit
 * 
 * \return \ref RES_OK on success error otherwise
 */
uint8_t menu_exit(void);

/** Add new menu item
 * 
 * The function adds menu item to selected menu
 * \note \p value_border is a string border of value of menu item  
 * \p value_border is represented in the format "<string>" = "<left border><right border>"  
 * if length of string is even so first half is \ref menu_item::value_left_border and  
 * second part is \ref menu_item::value_right_border  
 * if length equals to 1 then "<string>" = "<left border>" (no right border)  
 * In all other cases \p value_border is incorrect
 * 
 * \param[in] menu menu into which menu item is being added
 * \param[in] label label of menu item
 * \param[in] prompt prompt of menu item, NULL if no prompt
 * \param[in] value_border string border
 * \param[in] callback user callback by actions on menu item
 * \param[in] param optional parameters passed to \p callback
 * \param[in] menu_entry menu to which user can enter from menu item
 * \return menu item on success NULL otherwise 
 */
struct menu_item * menu_item_add(struct menu *menu,
                                const char *label,
                                const char *prompt,
                                const char *value_border,
                                uint8_t (*callback) (char *input, void *param),
                                void *param,
                                struct menu *menu_entry);

/** @} */

#endif //__MENU_H__