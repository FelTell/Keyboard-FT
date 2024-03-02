#pragma once

#include <class/hid/hid_device.h>
#include <cstdint>

class Key {
  public:
    Key(const char* keyText,
        uint8_t hidCode,
        uint8_t fnCode         = 0,
        uint8_t fnConsumerCode = 0)
        : m_keyText(keyText),
          m_modifier(0),
          m_hidCode(hidCode),
          m_fnKeyCode(fnCode),
          m_fnConsumerCode(fnConsumerCode),
          m_state(false) {}

    Key(const char* keyText, hid_keyboard_modifier_bm_t modifier)
        : m_keyText(keyText),
          m_modifier(modifier),
          m_hidCode(0),
          m_fnKeyCode(0),
          m_fnConsumerCode(0),
          m_state(false) {}

    const char* GetText() {
        return m_keyText;
    }

    uint8_t GetCode() const {
        return m_hidCode;
    }

    uint8_t GetFnKeyCode() const {
        return m_fnKeyCode;
    }

    uint8_t GetFnConsumerCode() const {
        return m_fnConsumerCode;
    }

    uint8_t GetModifier() const {
        return m_modifier;
    }

    bool GetState() const {
        return m_state;
    }

    void SetState(bool state) {
        m_state = state;
    }

  private:
    const char* m_keyText;
    const uint8_t m_modifier;
    const uint8_t m_hidCode;
    const uint8_t m_fnKeyCode;
    const uint16_t m_fnConsumerCode;
    bool m_state;
};
