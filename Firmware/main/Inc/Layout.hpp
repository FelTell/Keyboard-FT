#pragma once

#include "Key.hpp"
#include <array>
#include <cstdint>

namespace layout {

static constexpr uint8_t ROWS_NUM    = 6;
static constexpr uint8_t COLUMNS_NUM = 15;

static std::array<std::array<Key, ROWS_NUM>, COLUMNS_NUM> keys = {
    {{{
         Key("ESCAPE", HID_KEY_ESCAPE),
         Key("GRAVE", HID_KEY_GRAVE),
         Key("TAB", HID_KEY_TAB),
         Key("CAPS_LOCK", HID_KEY_CAPS_LOCK),
         Key("LEFTSHIFT", KEYBOARD_MODIFIER_LEFTSHIFT),
         Key("LEFTCTRL", KEYBOARD_MODIFIER_LEFTCTRL),
     }},
     {{
         Key("F1", HID_KEY_F1, HID_KEY_NONE, HID_USAGE_CONSUMER_MUTE),
         Key("1", HID_KEY_1),
         Key("Q", HID_KEY_Q),
         Key("A", HID_KEY_A, HID_KEY_NONE, HID_USAGE_CONSUMER_SCAN_PREVIOUS),
         Key("EUROPE_2", HID_KEY_EUROPE_2),
         Key("FUNCTION", HID_KEY_NONE),
     }},
     {{
         Key("F2",
             HID_KEY_F2,
             HID_KEY_NONE,
             HID_USAGE_CONSUMER_VOLUME_DECREMENT),
         Key("2", HID_KEY_2),
         Key("W", HID_KEY_W),
         Key("S", HID_KEY_S, HID_KEY_NONE, HID_USAGE_CONSUMER_PLAY_PAUSE),
         Key("Z", HID_KEY_Z),
         Key("LEFTGUI", KEYBOARD_MODIFIER_LEFTGUI),
     }},
     {{
         Key("F3",
             HID_KEY_F3,
             HID_KEY_NONE,
             HID_USAGE_CONSUMER_VOLUME_INCREMENT),
         Key("3", HID_KEY_3),
         Key("E", HID_KEY_E),
         Key("D", HID_KEY_D, HID_KEY_NONE, HID_USAGE_CONSUMER_SCAN_NEXT),
         Key("X", HID_KEY_X),
         Key("LEFTALT", KEYBOARD_MODIFIER_LEFTALT),
     }},
     {{
         Key("F4",
             HID_KEY_F4,
             HID_KEY_NONE,
             HID_USAGE_CONSUMER_BRIGHTNESS_DECREMENT),
         Key("4", HID_KEY_4),
         Key("R", HID_KEY_R),
         Key("F", HID_KEY_F),
         Key("C", HID_KEY_C),
         Key("NONE", HID_KEY_NONE),
     }},
     {{
         Key("F5",
             HID_KEY_F5,
             HID_KEY_NONE,
             HID_USAGE_CONSUMER_BRIGHTNESS_INCREMENT),
         Key("5", HID_KEY_5),
         Key("T", HID_KEY_T),
         Key("G", HID_KEY_G),
         Key("V", HID_KEY_V),
         Key("NONE", HID_KEY_NONE),
     }},
     {{
         Key("F6", HID_KEY_F6),
         Key("6", HID_KEY_6),
         Key("Y", HID_KEY_Y),
         Key("H", HID_KEY_H),
         Key("B", HID_KEY_B),
         Key("SPACE", HID_KEY_SPACE),
     }},
     {{
         Key("F7", HID_KEY_F7),
         Key("7", HID_KEY_7),
         Key("U", HID_KEY_U),
         Key("J", HID_KEY_J),
         Key("N", HID_KEY_N),
         Key("NONE", HID_KEY_NONE),
     }},
     {{
         Key("F8", HID_KEY_F8),
         Key("8", HID_KEY_8),
         Key("I", HID_KEY_I),
         Key("K", HID_KEY_K),
         Key("M", HID_KEY_M),
         Key("RIGHTALT", KEYBOARD_MODIFIER_RIGHTALT),
     }},
     {{
         Key("F9", HID_KEY_F9),
         Key("9", HID_KEY_9),
         Key("O", HID_KEY_O),
         Key("L", HID_KEY_L),
         Key("COMMA", HID_KEY_COMMA),
         Key("/?", HID_KEY_KANJI1),
     }},
     {{
         Key("F10", HID_KEY_F10),
         Key("0", HID_KEY_0),
         Key("P", HID_KEY_P),
         Key("SEMICOLON", HID_KEY_SEMICOLON),
         Key("PERIOD", HID_KEY_PERIOD),
         Key("RIGHTCTRL", KEYBOARD_MODIFIER_RIGHTCTRL),
     }},
     {{
         Key("F11", HID_KEY_F11),
         Key("MINUS", HID_KEY_MINUS),
         Key("BRACKET_LEFT", HID_KEY_BRACKET_LEFT),
         Key("APOSTROPHE", HID_KEY_APOSTROPHE),
         Key("SLASH", HID_KEY_SLASH),
         Key("ARROW_LEFT ", HID_KEY_ARROW_LEFT, HID_KEY_HOME),
     }},
     {{
         Key("F12", HID_KEY_F12),
         Key("EQUAL", HID_KEY_EQUAL),
         Key("BRACKET_RIGHT", HID_KEY_BRACKET_RIGHT),
         Key("NONE", HID_KEY_NONE),
         Key("RIGHTSHIFT", KEYBOARD_MODIFIER_RIGHTSHIFT),
         Key("ARROW_DOWN", HID_KEY_ARROW_DOWN, HID_KEY_PAGE_DOWN),
     }},
     {{
         Key("PRINT_SCREEN", HID_KEY_PRINT_SCREEN),
         Key("NONE", HID_KEY_NONE),
         Key("NONE", HID_KEY_NONE),
         Key("NONE", HID_KEY_NONE),
         Key("NONE", HID_KEY_NONE),
         Key("NONE", HID_KEY_NONE),
     }},
     {{
         Key("DELETE", HID_KEY_DELETE),
         Key("BACKSPACE", HID_KEY_BACKSPACE),
         Key("BACKSLASH", HID_KEY_BACKSLASH),
         Key("ENTER", HID_KEY_ENTER),
         Key("ARROW_UP", HID_KEY_ARROW_UP, HID_KEY_PAGE_UP),
         Key("ARROW_RIGHT", HID_KEY_ARROW_RIGHT, HID_KEY_END),
     }}}};

} // namespace layout
