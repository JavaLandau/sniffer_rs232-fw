#include "cli.h"
#include "menu.h"
#include "config.h"
#include "bsp_uart.h"
#include "sniffer_rs232.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>

#define UART_BUFF_SIZE     256

static struct {
    bool uart_error;
    bool uart_overflow;
} cli_state = {0};

static struct flash_config old_config;
static struct flash_config *flash_config = NULL;
static bool is_config_changed = false;

static struct menu_color_config color_config_select = MENU_COLOR_CONFIG_DEFAULT();
static struct menu_color_config color_config_choose = {.active = {.foreground = MENU_COLOR_YELLOW, .background = MENU_COLOR_RED},
                                                       .inactive = {.foreground = MENU_COLOR_WHITE, .background = MENU_COLOR_BLUE}};

static uint8_t __cli_menu_entry(char *input, void *param);
static uint8_t __cli_menu_set_defaults(char *input, void *param);
static uint8_t __cli_menu_exit(char *input, void *param);
static uint8_t __cli_menu_cfg_set(char *input, void *param);

static char * __cli_prompt_generator(const char *menu_item_label);

static char *rs232_trace_type_str[] = {
    "HEX",
    "HEX/ASCII",
    "INVALID"
};

static char *rs232_interspace_type_str[] = {
    "NONE",
    "SPACE",
    "NEW LINE",
    "INVALID"
};

static char *uart_parity_str[] = {
    "NONE",
    "EVEN",
    "ODD",
    "INVALID"
};

static char *rs232_channel_type_str[] = {
    "TX",
    "RX",
    "ANY",
    "ALL"
};

static struct {
    char *label;
    struct menu_color_config *color_config;
} init_menus[] = {
    {"MAIN MENU",           &color_config_select},
    {"CONFIGURATION",       &color_config_select},
    {"PRESETTINGS",         &color_config_select},
    {"SAVE CONFIGURATION",  &color_config_choose},
    {"ALGORITHM",           &color_config_select},
    {"CHANNEL TYPE",        &color_config_select},
    {"RESET TO DEFAULTS",   &color_config_choose},
    {"TRACE TYPE",          &color_config_select},
    {"IDLE PRESENCE",       &color_config_select},
    {"TX/RX DELIMITER",     &color_config_select},
    {"WORD LENGTH",         &color_config_select},
    {"PARITY",              &color_config_select},
    {"STOP BITS",           &color_config_select},
    {"PRESETTINGS ENABLE",  &color_config_choose},
};

static struct {
    char *menu_label;
    char *menu_item_label;
    char *value_border;
    uint8_t (*callback) (char *input, void *param);
    char *menu_entry_label;
} init_menu_items[] = {
    {"MAIN MENU", "Configuration", NULL, __cli_menu_entry, "CONFIGURATION"},
    {"MAIN MENU", "Presettings", "[]", __cli_menu_entry, "PRESETTINGS"},
    {"MAIN MENU", "Start", NULL, __cli_menu_exit, "SAVE CONFIGURATION"},
    {"CONFIGURATION", "Algorithm", NULL, __cli_menu_entry, "ALGORITHM"},
    {"CONFIGURATION", "Trace type", "[]", __cli_menu_entry, "TRACE TYPE"},
    {"CONFIGURATION", "IDLE presence", "[]", __cli_menu_entry, "IDLE PRESENCE"},
    {"CONFIGURATION", "TX/RX delimiter", "[]", __cli_menu_entry, "TX/RX DELIMITER"},
    {"CONFIGURATION", "Exit", NULL, __cli_menu_entry, "MAIN MENU"},
    {"ALGORITHM", "Channel type", "[]", __cli_menu_entry, "CHANNEL TYPE"},
    {"ALGORITHM", "Valid packets", "[]", __cli_menu_cfg_set, NULL},
    {"ALGORITHM", "UART errors", "[]", __cli_menu_cfg_set, NULL},
    {"ALGORITHM", "Tolerance", "[]", __cli_menu_cfg_set, NULL},
    {"ALGORITHM", "Minimum bits", "[]", __cli_menu_cfg_set, NULL},
    {"ALGORITHM", "Timeout", "[]", __cli_menu_cfg_set, NULL},
    {"ALGORITHM", "Attempts", "[]", __cli_menu_cfg_set, NULL},
    {"ALGORITHM", "Defaults", NULL, __cli_menu_entry, "RESET TO DEFAULTS"},
    {"ALGORITHM", "Exit", NULL, __cli_menu_entry, "CONFIGURATION"},
    {"CHANNEL TYPE", "TX", NULL, __cli_menu_cfg_set, "ALGORITHM"},
    {"CHANNEL TYPE", "RX", NULL, __cli_menu_cfg_set, "ALGORITHM"},
    {"CHANNEL TYPE", "ANY", NULL, __cli_menu_cfg_set, "ALGORITHM"},
    {"CHANNEL TYPE", "ALL", NULL, __cli_menu_cfg_set, "ALGORITHM"},
    {"RESET TO DEFAULTS", "YES", NULL, __cli_menu_set_defaults, "ALGORITHM"},
    {"RESET TO DEFAULTS", "NO", NULL, __cli_menu_entry, "ALGORITHM"},
    {"TRACE TYPE", "HEX", NULL, __cli_menu_cfg_set, "CONFIGURATION"},
    {"TRACE TYPE", "HEX/ASCII", NULL, __cli_menu_cfg_set, "CONFIGURATION"},
    {"IDLE PRESENCE", "NONE", NULL, __cli_menu_cfg_set, "CONFIGURATION"},
    {"IDLE PRESENCE", "SPACE", NULL, __cli_menu_cfg_set, "CONFIGURATION"},
    {"IDLE PRESENCE", "NEW LINE", NULL, __cli_menu_cfg_set, "CONFIGURATION"},
    {"TX/RX DELIMITER", "NONE", NULL, __cli_menu_cfg_set, "CONFIGURATION"},
    {"TX/RX DELIMITER", "SPACE", NULL, __cli_menu_cfg_set, "CONFIGURATION"},
    {"TX/RX DELIMITER", "NEW LINE", NULL, __cli_menu_cfg_set, "CONFIGURATION"},
    {"PRESETTINGS", "Baudrate", "[]", __cli_menu_cfg_set, NULL},
    {"PRESETTINGS", "Word length", "[]", __cli_menu_entry, "WORD LENGTH"},
    {"PRESETTINGS", "Parity", "[]", __cli_menu_entry, "PARITY"},
    {"PRESETTINGS", "Stop bits", "[]", __cli_menu_entry, "STOP BITS"},
    {"PRESETTINGS", "Enable", "[]", __cli_menu_entry, "PRESETTINGS ENABLE"},
    {"PRESETTINGS", "Exit", NULL, __cli_menu_entry, "MAIN MENU"},
    {"WORD LENGTH", "8 BITS", NULL, __cli_menu_cfg_set, "PRESETTINGS"},
    {"WORD LENGTH", "9 BITS", NULL, __cli_menu_cfg_set, "PRESETTINGS"},
    {"PARITY", "NONE", NULL, __cli_menu_cfg_set, "PRESETTINGS"},
    {"PARITY", "EVEN", NULL, __cli_menu_cfg_set, "PRESETTINGS"},
    {"PARITY", "ODD", NULL, __cli_menu_cfg_set, "PRESETTINGS"},
    {"STOP BITS", "1 BIT", NULL, __cli_menu_cfg_set, "PRESETTINGS"},
    {"STOP BITS", "2 BITS", NULL, __cli_menu_cfg_set, "PRESETTINGS"},
    {"PRESETTINGS ENABLE", "Enable", NULL, __cli_menu_cfg_set, "PRESETTINGS"},
    {"PRESETTINGS ENABLE", "Disable", NULL, __cli_menu_cfg_set, "PRESETTINGS"},
    {"SAVE CONFIGURATION", "YES", NULL, __cli_menu_exit, NULL},
    {"SAVE CONFIGURATION", "NO", NULL, __cli_menu_exit, NULL},
};

static uint8_t *__menu_rx_buff = NULL;

static char *__cli_prompt_generator(const char *menu_item_label)
{
    if (!menu_item_label)
        return NULL;

    uint32_t min = 0, max = 0;
    static char prompt[64] = {0};

    if (!strncmp("Valid packets", menu_item_label, UART_BUFF_SIZE)) {
        snprintf(prompt, sizeof(prompt), "Valid packets count: ");
    } else if (!strncmp("UART errors", menu_item_label, UART_BUFF_SIZE)) {
        snprintf(prompt, sizeof(prompt), "UART errors count: ");
    } else if (!strncmp("Tolerance", menu_item_label, UART_BUFF_SIZE)) {
        min = SNIFFER_RS232_CFG_PARAM_MIN(baudrate_tolerance);
        max = SNIFFER_RS232_CFG_PARAM_MAX(baudrate_tolerance);
        snprintf(prompt, sizeof(prompt), "Tolerance [%u-%u %%]: ", min, max);
    } else if (!strncmp("Minimum bits", menu_item_label, UART_BUFF_SIZE)) {
        min = SNIFFER_RS232_CFG_PARAM_MIN(min_detect_bits);
        max = SNIFFER_RS232_CFG_PARAM_MAX(min_detect_bits);
        snprintf(prompt, sizeof(prompt), "Minimum bits count [%u-%u]: ", min, max);
    } else if (!strncmp("Timeout", menu_item_label, UART_BUFF_SIZE)) {
        snprintf(prompt, sizeof(prompt), "Timeout [sec]: ");
    } else if (!strncmp("Attempts", menu_item_label, UART_BUFF_SIZE)) {
        snprintf(prompt, sizeof(prompt), "Attempts: ");
    } else if (!strncmp("Baudrate", menu_item_label, UART_BUFF_SIZE)) {
        snprintf(prompt, sizeof(prompt), "Baudrate [bps]: ");
    } else {
        return NULL;
    }

    return prompt;
}

static uint8_t __cli_menu_entry(char *input, void *param)
{
    return menu_entry(NULL);
}

static uint8_t __cli_menu_exit(char *input, void *param)
{
    struct menu_item *menu_item = menu_current_item_get();

    if (menu_item_by_label_only_get("MAIN MENU\\Start") == menu_item) {
        if (is_config_changed)
            __cli_menu_entry(NULL, NULL);
        else
            menu_exit();
    } else if (menu_item_by_label_only_get("SAVE CONFIGURATION\\YES") == menu_item) {
        uint8_t res  = config_save(flash_config);

        if (res != RES_OK)
            menu_entry(menu_by_label_get("MAIN MENU"));
        else
            menu_exit();
    } else {
        *flash_config = old_config;
        menu_exit();
    }

    return RES_OK;
}

static uint8_t __cli_menu_cfg_values_set(struct flash_config *config)
{
    if (!config)
        return RES_INVALID_PAR;

    char value[32] = {0};

    snprintf(value, sizeof(value), "%s", config->presettings.enable ? "Enabled" : "Disabled");
    menu_item_value_set(menu_item_by_label_only_get("MAIN MENU\\Presettings"), value);

    snprintf(value, sizeof(value), "%s", rs232_trace_type_str[config->trace_type]);
    menu_item_value_set(menu_item_by_label_only_get("CONFIGURATION\\Trace type"), value);

    snprintf(value, sizeof(value), "%s", rs232_interspace_type_str[config->idle_presence]);
    menu_item_value_set(menu_item_by_label_only_get("CONFIGURATION\\IDLE presence"), value);

    snprintf(value, sizeof(value), "%s", rs232_interspace_type_str[config->txrx_delimiter]);
    menu_item_value_set(menu_item_by_label_only_get("CONFIGURATION\\TX/RX delimiter"), value);

    snprintf(value, sizeof(value), "%s", rs232_channel_type_str[config->alg_config.channel_type]);
    menu_item_value_set(menu_item_by_label_only_get("ALGORITHM\\Channel type"), value);

    snprintf(value, sizeof(value), "%u", config->alg_config.valid_packets_count);
    menu_item_value_set(menu_item_by_label_only_get("ALGORITHM\\Valid packets"), value);

    snprintf(value, sizeof(value), "%u", config->alg_config.uart_error_count);
    menu_item_value_set(menu_item_by_label_only_get("ALGORITHM\\UART errors"), value);

    snprintf(value, sizeof(value), "%u %%", config->alg_config.baudrate_tolerance);
    menu_item_value_set(menu_item_by_label_only_get("ALGORITHM\\Tolerance"), value);

    snprintf(value, sizeof(value), "%u", config->alg_config.min_detect_bits);
    menu_item_value_set(menu_item_by_label_only_get("ALGORITHM\\Minimum bits"), value);

    snprintf(value, sizeof(value), "%u sec", config->alg_config.exec_timeout);
    menu_item_value_set(menu_item_by_label_only_get("ALGORITHM\\Timeout"), value);

    snprintf(value, sizeof(value), "%u", config->alg_config.calc_attempts);
    menu_item_value_set(menu_item_by_label_only_get("ALGORITHM\\Attempts"), value);

    snprintf(value, sizeof(value), "%u", config->presettings.baudrate);
    menu_item_value_set(menu_item_by_label_only_get("PRESETTINGS\\Baudrate"), value);

    snprintf(value, sizeof(value), "%u", config->presettings.wordlen);
    menu_item_value_set(menu_item_by_label_only_get("PRESETTINGS\\Word length"), value);

    snprintf(value, sizeof(value), "%s", uart_parity_str[config->presettings.parity]);
    menu_item_value_set(menu_item_by_label_only_get("PRESETTINGS\\Parity"), value);

    snprintf(value, sizeof(value), "%u", config->presettings.stopbits);
    menu_item_value_set(menu_item_by_label_only_get("PRESETTINGS\\Stop bits"), value);

    snprintf(value, sizeof(value), "%s", config->presettings.enable ? "*" : "");
    menu_item_value_set(menu_item_by_label_only_get("PRESETTINGS\\Enable"), value);

    return RES_OK;
}

static uint8_t __cli_menu_set_defaults(char *input, void *param)
{
    struct flash_config def_config = FLASH_CONFIG_DEFAULT();

    if (memcmp(&def_config, flash_config, sizeof(def_config))) {
        is_config_changed = true;
        *flash_config = def_config;
        __cli_menu_cfg_values_set(flash_config);
    }

    return RES_OK;
}

static uint8_t __cli_menu_cfg_set(char *input, void *param)
{
    if (!flash_config)
        return RES_NOT_INITIALIZED;

    uint32_t len = !input ? 0 : strnlen(input, MENU_MAX_STR_LEN);
    bool is_digit = true;

    for (uint32_t i = 0; i < len; i++) {
        if (!isdigit(input[i])) {
            is_digit = false;
            break;
        }
    }

    if (!is_digit)
        return RES_NOK;

    uint32_t value = 0;

    if (len < MENU_MAX_STR_LEN)
        sscanf(input, "%u", &value);

    bool is_menu_entry = false;
    struct menu_item *menu_item = menu_current_item_get();
    struct flash_config loc_config = *flash_config;

    if (menu_item_by_label_only_get("ALGORITHM\\Valid packets") == menu_item) {
        loc_config.alg_config.valid_packets_count = value;
    } else if (menu_item_by_label_only_get("ALGORITHM\\UART errors") == menu_item) {
        loc_config.alg_config.uart_error_count = value;
    } else if (menu_item_by_label_only_get("ALGORITHM\\Tolerance") == menu_item) {
        loc_config.alg_config.baudrate_tolerance = value;
    } else if (menu_item_by_label_only_get("ALGORITHM\\Minimum bits") == menu_item) {
        loc_config.alg_config.min_detect_bits = value;
    } else if (menu_item_by_label_only_get("ALGORITHM\\Timeout") == menu_item) {
        loc_config.alg_config.exec_timeout = value;
    } else if (menu_item_by_label_only_get("ALGORITHM\\Attempts") == menu_item) {
        loc_config.alg_config.calc_attempts = value;
    } else if (menu_item_by_label_only_get("PRESETTINGS\\Baudrate") == menu_item) {
        loc_config.presettings.baudrate = value ? value : loc_config.presettings.baudrate;
    } else {
        is_menu_entry = true;

        if (menu_item_by_label_only_get("CHANNEL TYPE\\TX") == menu_item)
            loc_config.alg_config.channel_type = RS232_CHANNEL_TX;
        else if (menu_item_by_label_only_get("CHANNEL TYPE\\RX") == menu_item)
            loc_config.alg_config.channel_type = RS232_CHANNEL_RX;
        else if (menu_item_by_label_only_get("CHANNEL TYPE\\ANY") == menu_item)
            loc_config.alg_config.channel_type = RS232_CHANNEL_ANY;
        else if (menu_item_by_label_only_get("CHANNEL TYPE\\ALL") == menu_item)
            loc_config.alg_config.channel_type = RS232_CHANNEL_ALL;
        else if (menu_item_by_label_only_get("TRACE TYPE\\HEX") == menu_item)
            loc_config.trace_type = RS232_TRACE_HEX;
        else if (menu_item_by_label_only_get("TRACE TYPE\\HEX/ASCII") == menu_item)
            loc_config.trace_type = RS232_TRACE_HYBRID;
        else if (menu_item_by_label_only_get("IDLE PRESENCE\\NONE") == menu_item)
            loc_config.idle_presence = RS232_INTERSPCACE_NONE;
        else if (menu_item_by_label_only_get("IDLE PRESENCE\\SPACE") == menu_item)
            loc_config.idle_presence = RS232_INTERSPCACE_SPACE;
        else if (menu_item_by_label_only_get("IDLE PRESENCE\\NEW LINE") == menu_item)
            loc_config.idle_presence = RS232_INTERSPCACE_NEW_LINE;
        else if (menu_item_by_label_only_get("TX/RX DELIMITER\\NONE") == menu_item)
            loc_config.txrx_delimiter = RS232_INTERSPCACE_NONE;
        else if (menu_item_by_label_only_get("TX/RX DELIMITER\\SPACE") == menu_item)
            loc_config.txrx_delimiter = RS232_INTERSPCACE_SPACE;
        else if (menu_item_by_label_only_get("TX/RX DELIMITER\\NEW LINE") == menu_item)
            loc_config.txrx_delimiter = RS232_INTERSPCACE_NEW_LINE;
        else if (menu_item_by_label_only_get("WORD LENGTH\\8 BITS") == menu_item)
            loc_config.presettings.wordlen = BSP_UART_WORDLEN_8;
        else if (menu_item_by_label_only_get("WORD LENGTH\\9 BITS") == menu_item)
            loc_config.presettings.wordlen = BSP_UART_WORDLEN_9;
        else if (menu_item_by_label_only_get("PARITY\\NONE") == menu_item)
            loc_config.presettings.parity = BSP_UART_PARITY_NONE;
        else if (menu_item_by_label_only_get("PARITY\\EVEN") == menu_item)
            loc_config.presettings.parity = BSP_UART_PARITY_EVEN;
        else if (menu_item_by_label_only_get("PARITY\\ODD") == menu_item)
            loc_config.presettings.parity = BSP_UART_PARITY_ODD;
        else if (menu_item_by_label_only_get("STOP BITS\\1 BIT") == menu_item)
            loc_config.presettings.stopbits = BSP_UART_STOPBITS_1;
        else if (menu_item_by_label_only_get("STOP BITS\\2 BITS") == menu_item)
            loc_config.presettings.stopbits = BSP_UART_STOPBITS_2;
        else if (menu_item_by_label_only_get("PRESETTINGS ENABLE\\Enable") == menu_item)
            loc_config.presettings.enable = true;
        else if (menu_item_by_label_only_get("PRESETTINGS ENABLE\\Disable") == menu_item)
            loc_config.presettings.enable = false;
        else
            is_menu_entry = false;
    }

    if (memcmp(&loc_config, flash_config, sizeof(loc_config)) && sniffer_rs232_config_check(&loc_config.alg_config)) {
        is_config_changed = true;
        *flash_config = loc_config;
        __cli_menu_cfg_values_set(flash_config);
    }

    if (is_menu_entry)
        __cli_menu_entry(NULL, NULL);

    return RES_OK;
}

static void __cli_uart_overflow_cb(enum uart_type type, void *params)
{
    cli_state.uart_overflow = true;
}

static void __cli_uart_error_cb(enum uart_type type, uint32_t error, void *params)
{
    cli_state.uart_error = true;
    bsp_uart_start(type);
}

static uint8_t __cli_menu_write_cb(char *data)
{
    if (!data)
        return RES_INVALID_PAR;

    if (strnlen(data, UART_BUFF_SIZE) == UART_BUFF_SIZE)
        return RES_INVALID_PAR;

    return bsp_uart_write(BSP_UART_TYPE_CLI, (uint8_t*)data, strlen(data), 1000);
}

static uint8_t __cli_menu_read_cb(char **data)
{
    if (!data)
        return RES_INVALID_PAR;

    uint16_t len = 0;
    uint8_t res = bsp_uart_read(BSP_UART_TYPE_CLI, __menu_rx_buff, &len, 1000);

    if (res == RES_OK)
        __menu_rx_buff[len] = 0;

    *data = (char*)__menu_rx_buff;

    return res;
}

uint8_t cli_init(void)
{
    memset(&cli_state, 0, sizeof(cli_state));

    __menu_rx_buff = (uint8_t*)malloc(UART_BUFF_SIZE + 1);

    if (!__menu_rx_buff)
        return RES_MEMORY_ERR;

    struct uart_init_ctx uart_init;
    uart_init.baudrate = 921600;
    uart_init.wordlen = BSP_UART_WORDLEN_8;
    uart_init.parity = BSP_UART_PARITY_NONE;
    uart_init.stopbits = BSP_UART_STOPBITS_1;
    uart_init.rx_size = UART_BUFF_SIZE;
    uart_init.tx_size = UART_BUFF_SIZE;
    uart_init.params = NULL;
    uart_init.error_isr_cb = __cli_uart_error_cb;
    uart_init.overflow_isr_cb = __cli_uart_overflow_cb;

    return bsp_uart_init(BSP_UART_TYPE_CLI, &uart_init);
}


void cli_trace(const char *format, ...)
{
    static char buffer[UART_BUFF_SIZE] = {0};

    va_list args;
    va_start(args, format);

    uint32_t len = vsnprintf(buffer, UART_BUFF_SIZE - 1, format, args);

    if (len > 0)
        bsp_uart_write(BSP_UART_TYPE_CLI, (uint8_t*)buffer, len, 1000);

    va_end(args);
}

uint8_t cli_menu_start(struct flash_config *config)
{
    if (!config)
        return RES_INVALID_PAR;

    old_config = *config;
    flash_config = config;

    is_config_changed = false;

    struct menu_config menu_config = {
        .is_looped = true,
        .num_delim = '.',
        .width = 64,
        .indent = 1,
        .num_type = MENU_NUM_DIGITAL,
        .pass_type = MENU_PASS_WITH_PROMPT,
        .read_callback = __cli_menu_read_cb,
        .write_callback = __cli_menu_write_cb
    };

    bool res = RES_OK;
    for (uint32_t i = 0; i < ARRAY_SIZE(init_menus); i++) {
        if (!menu_create(init_menus[i].label, '*', init_menus[i].color_config)) {
            res = RES_MEMORY_ERR;
            break;
        }
    }

    if (res != RES_OK) {
        menu_all_destroy();
        return res;
    }

    for (uint32_t i = 0; i < ARRAY_SIZE(init_menu_items); i++) {
        if (!menu_item_add(menu_by_label_get(init_menu_items[i].menu_label),
                           init_menu_items[i].menu_item_label,
                           __cli_prompt_generator(init_menu_items[i].menu_item_label),
                           init_menu_items[i].value_border,
                           init_menu_items[i].callback,
                           NULL,
                           menu_by_label_get(init_menu_items[i].menu_entry_label))) {
            res = RES_MEMORY_ERR;
            break;
        }
    }

    if (res != RES_OK) {
        menu_all_destroy();
        return res;
    }

    __cli_menu_cfg_values_set(flash_config);

    menu_start(&menu_config, menu_by_label_get("MAIN MENU"));

    menu_all_destroy();

    return RES_OK;
}