#ifndef __MENU_H__
#define __MENU_H__

#include "common.h"
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#define MENU_MAX_STR_LEN                256

enum menu_color_type {
    MENU_COLOR_BLACK = 0,
    MENU_COLOR_RED,
    MENU_COLOR_GREEN,
    MENU_COLOR_YELLOW,
    MENU_COLOR_BLUE,
    MENU_COLOR_MAGENTA,
    MENU_COLOR_CYAN,
    MENU_COLOR_WHITE,
    MENU_COLOR_MAX
};

enum menu_pass_type {
    MENU_PASS_NONE = 0,
    MENU_PASS_WITH_PROMPT,
    MENU_PASS_ALWAYS,
    MENU_PASS_MAX
};

enum menu_num_type {
    MENU_NUM_NONE = 0,
    MENU_NUM_DIGITAL,
    MENU_NUM_UPPER_LETTER,
    MENU_NUM_LOWER_LETTER,
    MENU_NUM_MAX
};

struct menu_color {
    enum menu_color_type foreground;
    enum menu_color_type background;
};

struct menu_color_config {
    struct menu_color active;
    struct menu_color inactive;
};

struct menu_item {
    struct menu_item *next;
    struct menu_item *prev;

    struct menu {
        char *label;
        char filler;
        struct menu_color_config color_config;
        struct menu_item *items;
        struct menu *next;
    } *menu_entry;

    uint8_t (*callback) (char *input, void *param);
    void *param;
    char *prompt;
    char *label;
    char *value_lower_border;
    char *value_upper_border;
    char *value;
    uint32_t value_len;
};

struct menu_config {
    bool is_looped;
    uint32_t width;
    uint32_t indent;
    enum menu_pass_type pass_type;
    enum menu_num_type num_type;
    char num_delim;
    uint8_t (*read_callback) (char **read_str);
    uint8_t (*write_callback) (char *write_str);
};

#define MENU_COLOR_CONFIG_DEFAULT()  {\
    .active = {.foreground = MENU_COLOR_BLUE, .background = MENU_COLOR_WHITE},\
    .inactive = {.foreground = MENU_COLOR_WHITE, .background = MENU_COLOR_BLUE}\
}

void menu_all_destroy(void);
struct menu *menu_create(char *label, char filler, struct menu_color_config *color_config);

uint8_t menu_entry(struct menu *menu);
uint8_t menu_item_value_set(struct menu_item *menu_item, const char *value);
struct menu_item *menu_current_item_get(void);
char *menu_item_label_get(struct menu_item *menu_item);
struct menu *menu_by_label_get(const char *label);
struct menu_item *menu_item_by_label_get(struct menu *menu, const char *label);
struct menu_item *menu_item_by_label_only_get(const char *label);
uint8_t menu_start(struct menu_config *config, struct menu *menu);
uint8_t menu_exit(void);
struct menu_item * menu_item_add(struct menu *menu,
                                const char *label,
                                const char *prompt,
                                const char *value_border,
                                uint8_t (*callback) (char *input, void *param),
                                void *param,
                                struct menu *menu_entry);

#endif //__MENU_H__