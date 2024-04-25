#include "RtosUtils.hpp"

namespace rtos {

void Delay(const uint32_t msToDelay) {
    if (xPortInIsrContext()) {
        ESP_LOGE("Delay", "Do not call a delay function from ISR");
        return;
    }
    vTaskDelay(msToDelay / portTICK_PERIOD_MS);
}

bool DelayUntil(uint32_t& previousTime, const uint32_t timeIncrement) {
    if (xPortInIsrContext()) {
        ESP_LOGE("Delay", "Do not call a delay function from ISR");
        return false;
    }
    return xTaskDelayUntil(&previousTime, timeIncrement) == pdTRUE;
}

uint32_t GetTickCount() {
    if (xPortInIsrContext()) {
        return xTaskGetTickCountFromISR();
    }
    return xTaskGetTickCount();
}

} // namespace rtos
