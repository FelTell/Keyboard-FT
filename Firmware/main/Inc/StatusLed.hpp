#pragma once

#include <cstdint>

namespace leds {

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

} // namespace leds
