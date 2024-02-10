#pragma once
#include <cstdint>

namespace status_led {

enum class Modes : uint8_t {
    Usb,
    BluetoothSearching,
    BluetoothConnected,
    Rainbow,
    Error,
};

bool SetupTask();

} // namespace status_led
