#pragma once

#include <cstdint>

namespace status_led {

enum class Modes : uint8_t {
    CapsOnUsb,
    CapsOnBle,
    Usb,
    BluetoothSearching,
    BluetoothConnected,
    Rainbow,
    Error,
};

bool SetMode(Modes);

bool SetupTask();

} // namespace status_led
