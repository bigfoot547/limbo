#include "chat.h"

#include <stddef.h> // for NULL

const char *const chat_color_names[] = {
#define CCN_DEF(_unused, _name, _ch) #_name
    COLOR_FOREACH(CCN_DEF),
#undef CCN_DEF
    NULL
};

const char chat_color_chars[] = "0123456789abcdef";

const char *const chat_click_event_names[] = {
    "open_url",
    "run_command",
    "suggest_command",
    "change_page",
    "copy_to_clipboard",
    NULL
};

const char *const chat_hover_event_names[] = {
    "show_text",
    "show_item",
    "show_entity",
    "show_achievement",
    NULL
};
