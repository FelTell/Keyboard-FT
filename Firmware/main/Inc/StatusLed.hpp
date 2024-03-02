#pragma once

#include <cstdint>

namespace leds {

enum class Modes : uint8_t {
    CapsOnUsb,
    CapsOnBle,
    Usb,
    BluetoothSearching,
    BluetoothConnected,
    NotConnected,
    Error,
};

bool SetMode(Modes);
Modes GetMode();

bool SetupTask();

} // namespace leds
