#include "RtosUtils.hpp"

#include "Bluetooth/BluetoothController.hpp"
#include "Controller.hpp"
#include "Leds.hpp"
#include "Matrix.hpp"
#include "UsbHid.hpp"

extern "C" void app_main(void) {
    controller::SetupTask();
    leds::SetupTask();
    matrix::SetupTask();
    usb_hid::SetupTask();
    bluetooth::controller::SetupTask();

    while (1) {
        rtos::Delay(1000);
    }
}
