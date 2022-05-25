#ifndef LIMBO_CHAT_H_INCLUDED
#define LIMBO_CHAT_H_INCLUDED

#include <stdbool.h>

enum chat_font {
    FONT_UNIFORM,
    FONT_ALT,
    FONT_DEFAULT
};

#define COLOR_FOREACH(_call)          \
_call(BLACK, black, 0),               \
_call(DARK_BLUE, dark_blue, 1),       \
_call(DARK_GREEN, dark_green, 2),     \
_call(DARK_AQUA, dark_aqua, 3),       \
_call(DARK_RED, dark_red, 4),         \
_call(DARK_PURPLE, dark_purple, 5),   \
_call(GOLD, gold, 6),                 \
_call(GRAY, gray, 7),                 \
_call(DARK_GRAY, dark_gray, 8),       \
_call(BLUE, blue, 9),                 \
_call(GREEN, green, a),               \
_call(AQUA, aqua, b),                 \
_call(RED, red, c),                   \
_call(LIGHT_PURPLE, light_purple, d), \
_call(YELLOW, yellow, e),             \
_call(WHITE, white, f)

enum chat_colors {
#define CC_DEF(_name, _unused, _ch) CC_ ## _name
    COLOR_FOREACH(CC_DEF)
#undef CC_DEF
};

extern const char *const chat_color_names[];
extern const char chat_color_chars[];

enum chat_flags {
    CF_BOLD_SET          = 0x00000001,
    CF_ITALIC_SET        = 0x00000002,
    CF_UNDERLINED_SET    = 0x00000004,
    CF_STRIKETHROUGH_SET = 0x00000008,
    CF_OBFUSCATED_SET    = 0x00000010,
    CF_FONT_SET          = 0x00000020,
    CF_COLOR_SET         = 0x00000040,
    CF_COLOR_CUSTOM      = 0x00000080
};

enum chat_component_type {
    COMPONENT_TEXT,
    COMPONENT_TRANSLATE,
    COMPONENT_KEYBIND,
    COMPONENT_SCORE,
    COMPONENT_SELECTOR
};

enum chat_click_event_type {
    CLICK_OPEN_URL = 0,
    CLICK_RUN_COMMAND,
    CLICK_SUGGEST_COMMAND,
    CLICK_CHANGE_PAGE,
    CLICK_COPY_TO_CLIPBOARD
};

extern const char *const chat_click_event_names[];

enum chat_hover_event_type {
    HOVER_SHOW_TEXT = 0,
    HOVER_SHOW_ITEM,
    HOVER_SHOW_ENTITY,
    HOVER_SHOW_ACHIEVEMENT
};

extern const char *const chat_hover_event_names[];

#define CHAT_COMMON_FIELDS \
int type;                  \
unsigned flags;            \
bool bold;                 \
bool italic;               \
bool underlined;           \
bool strikethrough;        \
bool obfuscated;           \
int font;                  \
int color;                 \
const char *custom_color;  \
const char *insertion;

struct chat_component {
    CHAT_COMMON_FIELDS
};

#endif // include guard
