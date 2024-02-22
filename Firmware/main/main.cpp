#include "RtosUtils.hpp"

#include "Matrix.hpp"
#include "StatusLed.hpp"
#include "UsbHid.hpp"

extern "C" void app_main(void) {
    status_led::SetupTask();
    matrix::SetupTask();
    usb_hid::SetupTask();

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
