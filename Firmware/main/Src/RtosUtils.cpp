#include "RtosUtils.hpp"

namespace rtos {

void Delay(const TickType_t ticksToDelay) {
    vTaskDelay(ticksToDelay / portTICK_PERIOD_MS);
}

} // namespace rtos
