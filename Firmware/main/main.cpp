#include "RtosUtils.hpp"

#include "Hid.hpp"
#include "Leds.hpp"
#include "Matrix.hpp"

extern "C" void app_main(void) {
    leds::SetupTask();
    matrix::SetupTask();
    hid::SetupTask();

    while (1) {
        rtos::Delay(1000);
    }
}
