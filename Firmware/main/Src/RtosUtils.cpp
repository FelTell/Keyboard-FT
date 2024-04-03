#include "RtosUtils.hpp"

namespace rtos {

void Delay(const TickType_t msToDelay) {
    vTaskDelay(msToDelay / portTICK_PERIOD_MS);
}

} // namespace rtos
