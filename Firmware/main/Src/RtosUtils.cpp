#include "RtosUtils.hpp"

namespace rtos {

void Delay(const uint32_t msToDelay) {
    vTaskDelay(msToDelay / portTICK_PERIOD_MS);
}

bool DelayUntil(uint32_t& previousTime, const uint32_t timeIncrement) {
    return xTaskDelayUntil(&previousTime, timeIncrement);
}

uint32_t GetTickCount() {
    return xTaskGetTickCount();
}
uint32_t GetTickCountFromIsr() {
    return xTaskGetTickCountFromISR();
}

} // namespace rtos
