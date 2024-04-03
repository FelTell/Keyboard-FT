#include "Controller.hpp"

#include "Leds.hpp"
#include "RtosUtils.hpp"

namespace controller {

static bool Init();
static void Handler();

static rtos::Task task("Controller", 4096, 24, Init, Handler);
static rtos::Queue<Modes> requests(1);

static Modes currentMode = Modes::Usb;

void SetUsb(bool isPressed) {
    static bool commandDone;

    if (!isPressed) {
        commandDone = false;
        return;
    }
    if (commandDone) {
        return;
    }

    SetMode(Modes::Usb);
    commandDone = true;
}

void SetBle(bool isPressed) {
    static bool commandDone;

    if (!isPressed) {
        commandDone = false;
        return;
    }
    if (commandDone) {
        return;
    }

    SetMode(Modes::Ble);
    commandDone = true;
}

static bool Init() {
    return true;
}

static void Handler() {
    auto newMode = requests.Wait();

    currentMode = newMode.value_or(Modes::Usb);

    leds::SendCommand(currentMode == Modes::Usb
                          ? leds::Commands::Usb
                          : leds::Commands::BluetoothSearching);
    ESP_LOGI("Mode", "Set to %s", currentMode == Modes::Usb ? "Usb" : "Ble");
}

bool SetMode(Modes mode) {
    return requests.Send(mode);
}

Modes GetMode() {
    return currentMode;
}

bool SetupTask() {
    if (!requests.Setup()) {
        return false;
    }
    if (!task.Setup()) {
        return false;
    }
    return true;
}

} // namespace controller
