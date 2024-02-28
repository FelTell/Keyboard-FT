#pragma once

#include <class/hid/hid_device.h>
#include <cstdint>

class Key {
  public:
    Key(const char* keyText, uint8_t hidCode, uint8_t fnCode = HID_KEY_NONE)
        : m_keyText(keyText),
          m_modifier(0),
          m_hidCode(hidCode),
          m_fnCode(fnCode),
          m_state(false) {}

    Key(const char* keyText, hid_keyboard_modifier_bm_t modifier)
        : m_keyText(keyText),
          m_modifier(modifier),
          m_hidCode(0),
          m_fnCode(0),
          m_state(false) {}

    const char* GetText() {
        return m_keyText;
    }

    uint8_t GetCode() const {
        return m_hidCode;
    }

    uint8_t GetFnCode() const {
        return m_fnCode;
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
    const uint8_t m_fnCode;
    bool m_state;
};
