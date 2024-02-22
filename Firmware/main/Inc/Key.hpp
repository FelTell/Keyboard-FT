#pragma once

#include <class/hid/hid_device.h>
#include <cstdint>

class Key {
  public:
    Key(const char* keyText)
        : m_keyText(keyText),
          m_modifier(0),
          m_hidCode(0),
          m_isFunction(true),
          m_state(false) {}

    Key(const char* keyText, uint8_t hidCode)
        : m_keyText(keyText),
          m_modifier(0),
          m_hidCode(hidCode),
          m_isFunction(false),
          m_state(false) {}

    Key(const char* keyText, uint8_t modifier, bool isModifier)
        : m_keyText(keyText),
          m_modifier(modifier),
          m_hidCode(0),
          m_isFunction(false),
          m_state(false) {}

    const char* GetText() {
        return m_keyText;
    }

    uint8_t GetCode() const {
        return m_hidCode;
    }

    uint8_t GetModifier() const {
        return m_modifier;
    }

    bool GetIsFunction() const {
        return m_isFunction;
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
    const bool m_isFunction;
    bool m_state;
};
