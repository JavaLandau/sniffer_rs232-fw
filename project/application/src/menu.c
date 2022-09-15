#include "menu.h"
#include <string.h>
#include <stdlib.h>

#define MAX_STR_LEN         256

#define MENU_COLOR_INACTIVE         "\33[37;44m"
#define MENU_COLOR_ACTIVE           "\33[34;47m"
#define MENU_COLOR_RESET            "\33[37;40m"

#define MENU_RETURN_HOME            "\33[H"
#define MENU_LINE_UP                "\33[A"
#define MENU_LINE_DOWN              "\33[B"
#define MENU_LINE_ERASE             "\33[2K"
#define MENU_SCREEN_ERASE           "\33[2J"

#define MENU_PASS_TYPE_IS_VALID(X)  (((uint32_t)(X)) < MENU_PASS_MAX)
#define MENU_NUM_TYPE_IS_VALID(X)   (((uint32_t)(X)) < MENU_NUM_MAX)

#define IS_PRINTABLE(X)             (X >= ' ' && X <= '~')

static struct menu_config menu_config = {0};
static struct menu_item *cur_item = NULL;
static struct menu_item *prev_item = NULL;
static struct menu *cur_menu = NULL;

static bool __exit = false;

static uint32_t __menu_strlen(const char *str)
{
    return str ? strnlen(str, MAX_STR_LEN) : 0;
}

static struct menu_item * __menu_get_last_item(void)
{
    if (!cur_menu || !cur_menu->items)
        return NULL;

    struct menu_item *loc_item = cur_menu->items;

    while (loc_item->next)
        loc_item = loc_item->next;

    return loc_item;
}

static uint8_t __menu_enumerator_inc(enum menu_num_type num_type, char *enumerator, uint8_t enum_len)
{
    char enum_start = 0;
    char enum_init = 0;
    char enum_end = 0;

    switch(num_type) {
    case MENU_NUM_DIGITAL:
        enum_init = '1';
        enum_start = '0';
        enum_end = '9';
        break;
    case MENU_NUM_UPPER_LETTER:
        enum_init = 'A';
        enum_start = 'A';
        enum_end = 'Z';
        break;
    case MENU_NUM_LOWER_LETTER:
        enum_init = 'a';
        enum_start = 'a';
        enum_end = 'z';
        break;
    default:
        return RES_NOT_ALLOWED;
    }

    uint8_t i = enum_len - 1;
    for (; i > 0; i--) {
        if (enumerator[i])
            break;
    }

    uint8_t enum_cur_len = i + 1;

    if (!enumerator[i])
        enumerator[i] = enum_init;
    else
        enumerator[i]++;

    do {
        if (enumerator[i] > enum_end) {
            if (i) {
                enumerator[i] = enum_start;
                enumerator[i - 1]++;
            } else {
                if (enum_cur_len != enum_len) {
                    enumerator[0] = enum_start;
                    for (uint8_t j = enum_len - 1; j > 0; j--)
                        enumerator[j] = enumerator[j - 1];
                    enumerator[0] = enum_init;
                } else {
                    enumerator[0] = enum_end;
                }
            }
        }
    } while (i--);

    return RES_OK;
}

static uint8_t __menu_redraw(struct menu_item *prev_item_active, struct menu_item *new_item_active)
{
    if (!menu_config.write_callback || !cur_menu || !cur_item)
        return RES_NOT_INITIALIZED;

    bool full_redraw = !prev_item_active && !new_item_active;
    struct menu_item *item = cur_menu->items;

    char single_char_buff[2] = {0};
    char enumerator[4] = {0};

    menu_config.write_callback(MENU_RETURN_HOME);

    if (full_redraw) {
        menu_config.write_callback(MENU_SCREEN_ERASE);

        if (__menu_strlen(cur_menu->label) >= menu_config.width) {
            menu_config.write_callback(cur_menu->label);
        } else {
            single_char_buff[0] = cur_menu->filler;
            uint32_t filler_len = (menu_config.width - __menu_strlen(cur_menu->label)) / 2;

            for (uint32_t i = 0; i < filler_len; i++)
                menu_config.write_callback(single_char_buff);

            menu_config.write_callback(cur_menu->label);

            filler_len = menu_config.width - __menu_strlen(cur_menu->label) - filler_len;

            for (uint32_t i = 0; i < filler_len; i++)
                menu_config.write_callback(single_char_buff);
        }
    }
    menu_config.write_callback(MENU_LINE_DOWN "\r");
    uint8_t res_enum = RES_OK;

    while (item) {
        res_enum = __menu_enumerator_inc(menu_config.num_type, enumerator, sizeof(enumerator) - 1);

        if (full_redraw || item == prev_item_active || item == new_item_active) {
            if (cur_item == item)
                menu_config.write_callback(MENU_COLOR_ACTIVE);
            else
                menu_config.write_callback(MENU_COLOR_INACTIVE);

            uint32_t cur_len = 0;

            if (res_enum == RES_OK) {
                menu_config.write_callback(enumerator);

                if (menu_config.num_delim) {
                    single_char_buff[0] = menu_config.num_delim;
                    menu_config.write_callback(single_char_buff);
                    cur_len++;
                }

                menu_config.write_callback(" ");
                cur_len += __menu_strlen(enumerator) + 1;
            }

            menu_config.write_callback(item->label);
            cur_len += __menu_strlen(item->label);

            if (item->value_lower_border) {
                menu_config.write_callback(" ");
                menu_config.write_callback(item->value_lower_border);
                cur_len += __menu_strlen(item->value_lower_border) + 1;
            }

            if (__menu_strlen(item->value))
                menu_config.write_callback(item->value);

            cur_len += __menu_strlen(item->value);

            if (item->value_upper_border)
                menu_config.write_callback(item->value_upper_border);

            cur_len += __menu_strlen(item->value_upper_border);

            for (uint32_t i = cur_len; i < menu_config.width; i++)
                menu_config.write_callback(" ");
        }

        menu_config.write_callback(MENU_LINE_DOWN "\r");
        item = item->next;
    }

    menu_config.write_callback(MENU_COLOR_RESET);
    menu_config.write_callback(MENU_LINE_ERASE);

    if (cur_item->prompt)
        menu_config.write_callback(cur_item->prompt);

    return RES_OK;
}

uint8_t menu_exit(void)
{
    __exit = true;
    return RES_OK;
}

struct menu_item * menu_current_item_get(void)
{
    return cur_item;
}

char *menu_item_label_get(struct menu_item *menu_item)
{
    if (!menu_item)
        return NULL;

    return menu_item->label;
}

struct menu_item *menu_item_by_label_get(struct menu *menu, char *label)
{
    if (!menu)
        return NULL;

    if (!label)
        return NULL;

    struct menu_item *item = menu->items;
    while (item) {
        if (!strncmp(item->label, label, MAX_STR_LEN))
            break;
        item = item->next;
    }

    return item;
}

uint8_t menu_item_value_set(struct menu_item *menu_item, const char *value)
{
    if (!menu_item)
        return RES_INVALID_PAR;

    if (!menu_item->value_lower_border)
        return RES_NOT_ALLOWED;

    uint32_t len = __menu_strlen(value);

    if (menu_item->value_len < len) {
        if (menu_item->value_len) {
            free(menu_item->value);
            menu_item->value_len = 0;
        }

        menu_item->value = (char*)malloc(len + 1);

        if (!menu_item->value)
            return RES_MEMORY_ERR;

        menu_item->value_len = len;
    }

    memset(menu_item->value, 0, menu_item->value_len + 1);
    memcpy(menu_item->value, value, len);

    uint8_t res = __menu_redraw(NULL, menu_item);

    return res;
}

uint8_t menu_entry(struct menu *menu)
{
    if (!menu_config.write_callback)
        return RES_NOT_INITIALIZED;

    if (!menu && (!cur_item || !cur_item->menu_entry || !cur_item->menu_entry->items))
        return RES_INVALID_PAR;

    if (menu && !menu->items)
        return RES_INVALID_PAR;

    cur_menu = menu ? menu : cur_item->menu_entry;
    cur_item = cur_menu->items;
    prev_item = cur_item;

    uint8_t res = __menu_redraw(NULL, NULL);

    return res;
}

uint8_t menu_start(struct menu_config *config, struct menu *menu)
{
    if (!config || !config->write_callback || !config->read_callback)
        return RES_INVALID_PAR;

    if (!MENU_PASS_TYPE_IS_VALID(config->pass_type))
        return RES_INVALID_PAR;

    if (!MENU_NUM_TYPE_IS_VALID(config->num_type))
        return RES_INVALID_PAR;

    if (!config->width)
        return RES_INVALID_PAR;

    if (!menu)
        return RES_INVALID_PAR;

    __exit = false;
    memcpy(&menu_config, config, sizeof(menu_config));

    uint8_t res = menu_entry(menu);

    if (res != RES_OK)
        return res;

    char input_pass_str[MAX_STR_LEN] = {0};
    uint32_t pass_count = 0;
    char *input_str = NULL;

    while (!__exit) {
        bool pass_allowed = (menu_config.pass_type == MENU_PASS_ALWAYS) || 
                            (menu_config.pass_type == MENU_PASS_WITH_PROMPT && cur_item->prompt);

        if (menu_config.read_callback(&input_str) == RES_OK) {
            uint32_t len = __menu_strlen(input_str);

            bool redraw_flag = false;
            if (strstr(input_str, MENU_LINE_UP)) {
                if (cur_item != cur_menu->items || (cur_item == cur_menu->items && menu_config.is_looped)) {
                    redraw_flag = true;
                    prev_item = cur_item;

                    if (cur_item == cur_menu->items)
                        cur_item = __menu_get_last_item();
                    else
                        cur_item = cur_item->prev;

                    pass_count = 0;
                    memset(input_pass_str, 0, MAX_STR_LEN);
                }
            } else if (strstr(input_str, MENU_LINE_DOWN)) {
                if (cur_item->next || (!cur_item->next && menu_config.is_looped)) {
                    redraw_flag = true;
                    prev_item = cur_item;

                    if (!cur_item->next)
                        cur_item = cur_menu->items;
                    else
                        cur_item = cur_item->next;

                    pass_count = 0;
                    memset(input_pass_str, 0, MAX_STR_LEN);
                }
            } else {
                char *p = strstr(input_str, "\x0D");
                if (p)
                    *p = 0;

                if (pass_allowed) {
                    for (uint32_t i = 0; i < __menu_strlen(input_str); i++) {
                        if (!IS_PRINTABLE(input_str[i]))
                            input_str[i] = ' ';
                    }

                    if (pass_count < MAX_STR_LEN) {
                        uint32_t count = MIN(__menu_strlen(input_str), MAX_STR_LEN - pass_count);
                        memcpy(&input_pass_str[pass_count], input_str, count);
                        pass_count += count;
                    }
                    menu_config.write_callback(input_str);
                }

                if (p) {
                    __menu_redraw(cur_item, cur_item);

                    if (cur_item->callback)
                        cur_item->callback(input_pass_str, cur_item->param);

                    pass_count = 0;
                    memset(input_pass_str, 0, MAX_STR_LEN);
                }
            }

            if (redraw_flag)
                __menu_redraw(prev_item, cur_item);
        }
    }

    return RES_OK;
}

struct menu * menu_create(char *label, char filler)
{
    if (!__menu_strlen(label) || !filler)
        return NULL;

    uint8_t res = RES_OK;
    struct menu *menu = NULL;

    do {
        menu = (struct menu*)malloc(sizeof(struct menu));

        if (!menu) {
            res = RES_MEMORY_ERR;
            break;
        }

        memset(menu, 0, sizeof(struct menu));

        uint32_t label_len = __menu_strlen(label);

        menu->label = (char*)malloc(label_len + 1);

        if (!menu->label) {
            res = RES_MEMORY_ERR;
            break;
        }

        memset(menu->label, 0, label_len + 1);
        memcpy(menu->label, label, label_len);

        menu->filler = filler;
    } while(0);

    if (res != RES_OK) {
        if (menu) {
            if (menu->label)
                free(menu->label);

            free(menu);
            menu = NULL;
        }
    }

    return menu;
}

void menu_destroy(struct menu *menu)
{
    if (!menu)
        return;

    struct menu_item *item = menu->items;
    while(item) {
        struct menu_item * temp_item = item;

        free(item);
        item = temp_item->next;
    }

    free(menu);
}

struct menu_item * menu_item_add(struct menu *menu,
                                const char *label,
                                const char *prompt,
                                const char *value_border,
                                uint8_t (*callback) (char *input, void *param),
                                void *param,
                                struct menu *menu_entry)
{
    if (!menu || !__menu_strlen(label))
        return NULL;

    uint8_t res = RES_OK;
    struct menu_item *next_item = NULL;

    do {
        uint32_t label_len = __menu_strlen(label);
        uint32_t prompt_len = __menu_strlen(prompt);
        uint32_t value_border_len = __menu_strlen(value_border);

        if (value_border_len && (value_border_len % 2) && (value_border_len != 1)) {
            res = RES_INVALID_PAR;
            break;
        }

        next_item = (struct menu_item*)malloc(sizeof(struct menu_item));

        if (!next_item) {
            res = RES_MEMORY_ERR;
            break;
        }

        memset(next_item, 0, sizeof(struct menu_item));

        next_item->label = (char*)malloc(label_len + 1);

        if (!next_item->label) {
            res = RES_MEMORY_ERR;
            break;
        }

        memset(next_item->label, 0, label_len + 1);

        if (prompt_len) {
            next_item->prompt = (char*)malloc(prompt_len + 1);

            if (!next_item->prompt) {
                res = RES_MEMORY_ERR;
                break;
            }

            memset(next_item->prompt, 0, prompt_len + 1);
            memcpy(next_item->prompt, prompt, prompt_len);
        }

        if (value_border_len) {
            uint32_t value_lower_border_len = (value_border_len == 1) ? 1 : (value_border_len / 2);
            next_item->value_lower_border = (char*)malloc(value_lower_border_len + 1);

            if (!next_item->value_lower_border) {
                res = RES_MEMORY_ERR;
                break;
            }

            memset(next_item->value_lower_border, 0, value_lower_border_len + 1);
            memcpy(next_item->value_lower_border, value_border, value_lower_border_len);

            if (value_border_len > 1) {
                uint32_t value_upper_border_len = value_border_len / 2;
                next_item->value_upper_border = (char*)malloc(value_upper_border_len + 1);

                if (!next_item->value_upper_border) {
                    res = RES_MEMORY_ERR;
                    break;
                }

                memset(next_item->value_upper_border, 0, value_upper_border_len + 1);
                memcpy(next_item->value_upper_border, &value_border[value_lower_border_len], value_upper_border_len);
            }
        }

        memcpy(next_item->label, label, label_len);

        next_item->callback = callback;
        next_item->param = param;
        next_item->menu_entry = menu_entry;

        struct menu_item *cur_item = menu->items;

        while(cur_item && cur_item->next)
            cur_item = cur_item->next;

        if (cur_item) {
            cur_item->next = next_item;
            next_item->prev = cur_item;
        } else {
            menu->items = next_item;
        }
    } while(0);

    if (res != RES_OK) {
        if (next_item) {
            if (next_item->label)
                free(next_item->label);

            if (next_item->prompt)
                free(next_item->prompt);

            if (next_item->value_lower_border)
                free(next_item->value_lower_border);

            if (next_item->value_upper_border)
                free(next_item->value_upper_border);

            free(next_item);
        }

        return NULL;
    }

    return next_item;
}

