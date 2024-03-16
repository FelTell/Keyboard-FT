#pragma once

#include <cstdint>

namespace leds {

enum class Commands : uint8_t {
    CapsOnUsb = 0,
    CapsOnBle,
    Usb,
    BluetoothSearching,
    BluetoothConnected,
    NotConnected,
    Error,

    DecreaseBrightness = 20,
    IncreaseBrightness,
};

bool SendCommand(Commands);
Commands GetMode();

bool SetupTask();

void DecreaseBrightness(bool);
void IncreaseBrightness(bool);

} // namespace leds
