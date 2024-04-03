#include "RtosUtils.hpp"

#include "Controller.hpp"
#include "Leds.hpp"
#include "Matrix.hpp"
#include "UsbHid.hpp"

extern "C" void app_main(void) {
    controller::SetupTask();
    leds::SetupTask();
    matrix::SetupTask();
    usb_hid::SetupTask();

    while (1) {
        rtos::Delay(1000);
    }
}
