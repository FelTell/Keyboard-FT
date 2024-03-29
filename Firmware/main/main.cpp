#include "RtosUtils.hpp"

#include "Leds.hpp"
#include "Matrix.hpp"
#include "UsbHid.hpp"

extern "C" void app_main(void) {
    leds::SetupTask();
    matrix::SetupTask();
    usb_hid::SetupTask();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
