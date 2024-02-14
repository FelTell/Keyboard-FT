#include "Matrix.hpp"

#include "RtosUtils.hpp"
#include <array>
#include <esp_log.h>

#include "class/hid/hid_device.h"
#include <driver/gpio.h>
#include <esp_bit_defs.h>

namespace matrix {

static bool Init();
static void Handler();

class Key {
  public:
    Key(const char* keyText, uint8_t hidCode)
        : m_keyText(keyText), m_hidCode(hidCode), m_state(false) {}

    const char* GetText() {
        return m_keyText;
    }
    uint8_t GetCode() {
        return m_hidCode;
    }
    bool Get() {
        return m_state;
    }
    void Set(bool state) {
        m_state = state;
    }

  private:
    const char* m_keyText;
    uint8_t m_hidCode;
    bool m_state;
};

static rtos::Task task("MatrixTask", 4096, 24, Init, Handler);

static constexpr uint8_t NUMBER_OF_ROWS                  = 6;
static constexpr uint8_t NUMBER_OF_COLUMNS               = 15;
static const std::array<gpio_num_t, NUMBER_OF_ROWS> rows = {
    GPIO_NUM_14,
    GPIO_NUM_2,
    GPIO_NUM_7,
    GPIO_NUM_6,
    GPIO_NUM_5,
    GPIO_NUM_4,
};
static const std::array<gpio_num_t, NUMBER_OF_COLUMNS> columns = {GPIO_NUM_37,
                                                                  GPIO_NUM_36,
                                                                  GPIO_NUM_35,
                                                                  GPIO_NUM_21,
                                                                  GPIO_NUM_1,
                                                                  GPIO_NUM_15,
                                                                  GPIO_NUM_16,
                                                                  GPIO_NUM_17,
                                                                  GPIO_NUM_18,
                                                                  GPIO_NUM_8,
                                                                  GPIO_NUM_9,
                                                                  GPIO_NUM_10,
                                                                  GPIO_NUM_11,
                                                                  GPIO_NUM_12,
                                                                  GPIO_NUM_13};
static std::array<std::array<Key, NUMBER_OF_ROWS>, NUMBER_OF_COLUMNS> matrix = {
    {{{
         Key("ESCAPE", HID_KEY_ESCAPE),
         Key("LANG3", HID_KEY_LANG3),
         Key("TAB", HID_KEY_TAB),
         Key("CAPS_LOCK", HID_KEY_CAPS_LOCK),
         Key("SHIFT_LEFT", HID_KEY_SHIFT_LEFT),
         Key("CONTROL_LEFT", HID_KEY_CONTROL_LEFT),
     }},
     {{
         Key("F1", HID_KEY_F1),
         Key("1", HID_KEY_1),
         Key("Q", HID_KEY_Q),
         Key("A", HID_KEY_A),
         Key("BACKSLASH", HID_KEY_BACKSLASH),
         Key("FUNCTION", HID_KEY_NONE),
     }},
     {{
         Key("F2", HID_KEY_F2),
         Key("2", HID_KEY_2),
         Key("W", HID_KEY_W),
         Key("S", HID_KEY_S),
         Key("Z", HID_KEY_Z),
         Key("GUI_LEFT", HID_KEY_GUI_LEFT),
     }},
     {{
         Key("F3", HID_KEY_F3),
         Key("3", HID_KEY_3),
         Key("E", HID_KEY_E),
         Key("D", HID_KEY_D),
         Key("X", HID_KEY_X),
         Key("ALT_LEFT", HID_KEY_ALT_LEFT),
     }},
     {{
         Key("F4", HID_KEY_F4),
         Key("4", HID_KEY_4),
         Key("R", HID_KEY_R),
         Key("F", HID_KEY_F),
         Key("C", HID_KEY_C),
         Key("none", HID_KEY_NONE),
     }},
     {{
         Key("F5", HID_KEY_F5),
         Key("5", HID_KEY_5),
         Key("T", HID_KEY_T),
         Key("G", HID_KEY_G),
         Key("V", HID_KEY_V),
         Key("none", HID_KEY_NONE),
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
         Key("none", HID_KEY_NONE),
     }},
     {{
         Key("F8", HID_KEY_F8),
         Key("8", HID_KEY_8),
         Key("I", HID_KEY_I),
         Key("K", HID_KEY_K),
         Key("M", HID_KEY_M),
         Key("ALT_RIGHT", HID_KEY_ALT_RIGHT),
     }},
     {{
         Key("F9", HID_KEY_F9),
         Key("9", HID_KEY_9),
         Key("O", HID_KEY_O),
         Key("L", HID_KEY_L),
         Key("COMMA", HID_KEY_COMMA),
         Key("SLASH", HID_KEY_SLASH),
     }},
     {{
         Key("F10", HID_KEY_F10),
         Key("0", HID_KEY_0),
         Key("P", HID_KEY_P),
         Key("LANG1", HID_KEY_LANG1),
         Key("PERIOD", HID_KEY_PERIOD),
         Key("CONTROL_RIGHT", HID_KEY_CONTROL_RIGHT),
     }},
     {{
         Key("F11", HID_KEY_F11),
         Key("MINUS", HID_KEY_MINUS),
         Key("LANG4", HID_KEY_LANG4),
         Key("LANG7", HID_KEY_LANG7),
         Key("SEMICOLON", HID_KEY_SEMICOLON),
         Key("ARROW_LEFT ", HID_KEY_ARROW_LEFT),
     }},
     {{
         Key("F12", HID_KEY_F12),
         Key("EQUAL", HID_KEY_EQUAL),
         Key("LANG6", HID_KEY_LANG6),
         Key("none", HID_KEY_NONE),
         Key("SHIFT_RIGHT", HID_KEY_SHIFT_RIGHT),
         Key("ARROW_DOWN", HID_KEY_ARROW_DOWN),
     }},
     {{
         Key("PRINT_SCREEN", HID_KEY_PRINT_SCREEN),
         Key("none", HID_KEY_NONE),
         Key("none", HID_KEY_NONE),
         Key("none", HID_KEY_NONE),
         Key("none", HID_KEY_NONE),
         Key("none", HID_KEY_NONE),
     }},
     {{
         Key("DELETE", HID_KEY_DELETE),
         Key("BACKSPACE", HID_KEY_BACKSPACE),
         Key("LANG5", HID_KEY_LANG5),
         Key("ENTER", HID_KEY_ENTER),
         Key("ARROW_DOWN", HID_KEY_ARROW_DOWN),
         Key("ARROW_RIGHT", HID_KEY_ARROW_RIGHT),
     }}}};

static bool Init() {
    gpio_config_t config;
    config.pull_up_en = GPIO_PULLUP_DISABLE;
    config.intr_type  = GPIO_INTR_DISABLE;

    config.pull_down_en = GPIO_PULLDOWN_ENABLE;
    config.mode         = GPIO_MODE_INPUT;
    for (gpio_num_t gpioNum : rows) {
        config.pin_bit_mask = BIT64(gpioNum);
        gpio_config(&config);
    }

    config.pull_down_en = GPIO_PULLDOWN_DISABLE;
    config.mode         = GPIO_MODE_OUTPUT;
    for (gpio_num_t gpioNum : columns) {
        config.pin_bit_mask = BIT64(gpioNum);
        gpio_config(&config);
    }
    return true;
}

static void Handler() {
    for (uint8_t column = 0; column < NUMBER_OF_COLUMNS; ++column) {
        gpio_set_level(columns[column], true);
        for (uint8_t row = 0; row < NUMBER_OF_ROWS; ++row) {
            Key& key   = matrix[column][row];
            bool state = gpio_get_level(rows[row]);
            if (state != key.Get()) {
                key.Set(state);
                ESP_LOGI(key.GetText(),
                         "has been %s. ID = %d. Row = %d, Column = %d. GPIO = "
                         "%d and %d.",
                         state ? "pressed" : "released",
                         key.GetCode(),
                         row,
                         column,
                         rows[row],
                         columns[column]);
            }
        }
        gpio_set_level(columns[column], false);
    }
    rtos::Delay(5);
}

bool SetupTask() {
    if (!task.Setup()) {
        return false;
    }
    // if (!requests.Setup()) {
    //     return false;
    // }
    return true;
}

} // namespace matrix
