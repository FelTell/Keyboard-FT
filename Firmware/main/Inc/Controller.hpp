#pragma once

#include <cstdint>

namespace controller {

enum class Modes : uint8_t {
    Usb,
    Ble
};

bool SetMode(Modes);
Modes GetMode();

void SetUsb(bool isPressed);
void SetBle(bool isPressed);

bool SetupTask();

} // namespace controller
