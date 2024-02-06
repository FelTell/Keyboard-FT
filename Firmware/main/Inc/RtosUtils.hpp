#pragma once
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace rtos {

void Delay(const TickType_t ticksToDelay) {
    vTaskDelay(ticksToDelay / portTICK_PERIOD_MS);
}

} // namespace rtos
