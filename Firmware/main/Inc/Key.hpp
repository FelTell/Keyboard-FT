#pragma once

#include <class/hid/hid_device.h>
#include <cstdint>

class Key {
  public:
    using FnFunction = void (*)(bool);

    Key(const char* keyText,
        uint8_t hidCode,
        uint8_t fnCode         = 0,
        uint8_t fnConsumerCode = 0,
        FnFunction fnFunction  = nullptr)
        : m_keyText(keyText),
          m_modifier(0),
          m_hidCode(hidCode),
          m_fnKeyCode(fnCode),
          m_fnConsumerCode(fnConsumerCode),
          m_fnFunction(fnFunction),
          m_state(false) {}

    Key(const char* keyText, hid_keyboard_modifier_bm_t modifier)
        : m_keyText(keyText),
          m_modifier(modifier),
          m_hidCode(0),
          m_fnKeyCode(0),
          m_fnConsumerCode(0),
          m_fnFunction(nullptr),
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

    void DoFnFunction(bool state) const {
        if (m_fnFunction) {
            m_fnFunction(state);
        }
    }

    bool HandleStateChange(bool currentState) {
        if (!m_state && currentState) {
            return HandlePress();
        }
        if (m_state && !currentState) {
            return HandleRelease();
        }
        return false;
    }

  private:
    static constexpr uint32_t DEBOUNCE_COUNT_PRESS = 10;
    static constexpr uint32_t DEBOUNCE_COUNT_RELEASE = 50;
    const char* m_keyText;
    const uint8_t m_modifier;
    const uint8_t m_hidCode;
    const uint8_t m_fnKeyCode;
    const uint16_t m_fnConsumerCode;
    const FnFunction m_fnFunction;
    uint32_t m_pressCount;
    uint32_t m_releaseCount;
    bool m_state;

    bool HandlePress() {
        m_releaseCount = 0;
        if (m_pressCount < DEBOUNCE_COUNT_PRESS) {
            m_pressCount++;
            return false;
        }
        m_state = true;
        return true;
    }

    bool HandleRelease() {
        m_pressCount = 0;
        if (m_releaseCount < DEBOUNCE_COUNT_RELEASE) {
            m_releaseCount++;
            return false;
        }
        m_state = false;
        return true;
    }
};
