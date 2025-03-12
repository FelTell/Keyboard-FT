#include "RtosUtils.hpp"

#include <utility>

namespace rtos {

bool IsInIsr() {
    return xPortInIsrContext() == pdTRUE;
}

void Delay(const uint32_t msToDelay) {
    if (IsInIsr()) {
        return;
    }
    vTaskDelay(msToDelay / portTICK_PERIOD_MS);
}

bool DelayUntil(uint32_t& previousTime, const uint32_t timeIncrement) {
    if (IsInIsr()) {
        return false;
    }
    return xTaskDelayUntil(&previousTime, timeIncrement) == pdTRUE;
}

uint32_t GetTickCount() {
    if (IsInIsr()) {
        return xTaskGetTickCountFromISR();
    }
    return xTaskGetTickCount();
}

Event::Event() : m_handle(nullptr) {};

Event::~Event() {
    if (!m_handle) {
        return;
    }
    vEventGroupDelete(m_handle);
    m_handle = nullptr;
}

Event::Event(Event&& other) noexcept
    : m_handle(std::exchange(other.m_handle, nullptr)) {}

Event& Event::operator=(Event&& other) noexcept {
    if (&other != this) {
        std::swap(m_handle, other.m_handle);
    }
    return *this;
}

bool Event::Setup() {
    if (m_handle) {
        vEventGroupDelete(m_handle);
    }
    m_handle = xEventGroupCreate();
    if (!m_handle) {
        ESP_LOGE("EventSetup", "Insufficient heap");
        return false;
    }
    return true;
}

uint32_t Event::Set(uint32_t bitsToSet) {
    if (!m_handle) {
        if (!IsInIsr()) {
            ESP_LOGI("Event", "Handle is null, check if setup was done");
        }
        return false;
    }
    if (bitsToSet >= (1 << AVAILABLE_BITS)) {
        if (!IsInIsr()) {
            ESP_LOGE("EventSet",
                     "A bit to set was higher than the allowed: %d",
                     AVAILABLE_BITS);
        }
        return 0;
    }

    if (!IsInIsr()) {
        return xEventGroupSetBits(m_handle, bitsToSet);
    }

    auto higherPriorityTaskWoken = pdFALSE;
    auto result                  = xEventGroupSetBitsFromISR(m_handle,
                                            bitsToSet,
                                            &higherPriorityTaskWoken);
    if (result != pdFALSE) {
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    }

    return 0;
};

uint32_t Event::Clear(uint32_t bitsToClear) {
    if (!m_handle) {
        if (!IsInIsr()) {
            ESP_LOGI("Event", "Handle is null, check if setup was done");
        }
        return false;
    }
    if (bitsToClear >= (1 << AVAILABLE_BITS)) {
        if (!IsInIsr()) {
            ESP_LOGE("EventClear",
                     "A bit to clear was higher than the allowed: %d",
                     AVAILABLE_BITS);
        }
        return 0;
    }

    if (!IsInIsr()) {
        return xEventGroupClearBits(m_handle, bitsToClear);
    }

    xEventGroupClearBitsFromISR(m_handle, bitsToClear);
    return 0;
};

std::optional<uint32_t> Event::Wait(uint32_t bitsToWait,
                                    bool clearOnExit,
                                    bool waitForAll,
                                    uint32_t timeout) {
    if (!m_handle) {
        if (!IsInIsr()) {
            ESP_LOGI("Event", "Handle is null, check if setup was done");
        }
        return false;
    }
    if (IsInIsr()) {
        return std::nullopt;
    }

    if (bitsToWait >= (1 << AVAILABLE_BITS)) {
        if (!IsInIsr()) {
            ESP_LOGE("EventWait",
                     "A bit to wait was higher than the allowed: %d",
                     AVAILABLE_BITS);
        }
        return std::nullopt;
    }
    if (!bitsToWait) {
        return std::nullopt;
    }
    auto result = xEventGroupWaitBits(m_handle,
                                      bitsToWait,
                                      clearOnExit ? pdTRUE : pdFALSE,
                                      waitForAll ? pdTRUE : pdFALSE,
                                      timeout);

    // All bits to wait have been set, so the timeout has not been reached.
    // Return the event group bits.
    if ((bitsToWait & result) == bitsToWait) {
        return result;
    }
    // Some of the bits to wait have been set, if waitForAll is not set it
    // means the timeout has not been reached. Return the event group bits.
    if (!waitForAll && (result & bitsToWait)) {
        return result;
    }
    // The timeout expired.
    return std::nullopt;
}

uint32_t Event::Get() {
    if (!m_handle) {
        if (!IsInIsr()) {
            ESP_LOGI("Event", "Handle is null, check if setup was done");
        }
        return 0;
    }
    return IsInIsr() ? xEventGroupGetBitsFromISR(m_handle)
                     : xEventGroupGetBits(m_handle);
}

EventGroupHandle_t& Event::GetHandle() {
    return m_handle;
}

Timer::Timer(std::string_view name,
             uint32_t periodMs,
             bool autoReload,
             void (*callback)())
    : m_handle(nullptr),
      m_name(name),
      m_periodMs(periodMs),
      m_autoReload(autoReload),
      m_callback(callback) {}

Timer::~Timer() {
    if (!m_handle) {
        return;
    }
    xTimerDelete(m_handle, 0);
    m_handle = nullptr;
}

Timer::Timer(Timer&& other) noexcept
    : m_handle(std::exchange(other.m_handle, nullptr)),
      m_name(std::exchange(other.m_name, "")),
      m_periodMs(std::exchange(other.m_periodMs, 0)),
      m_autoReload(std::exchange(other.m_autoReload, false)),
      m_callback(std::exchange(other.m_callback, nullptr)) {}

Timer& Timer::operator=(Timer&& other) noexcept {
    if (&other != this) {
        std::swap(m_handle, other.m_handle);
        std::swap(m_name, other.m_name);
        std::swap(m_periodMs, other.m_periodMs);
        std::swap(m_autoReload, other.m_autoReload);
        std::swap(m_callback, other.m_callback);
    }
    return *this;
}

bool Timer::Start() {
    if (!m_handle) {
        if (!Setup()) {
            return false;
        }
    }

    if (!IsInIsr()) {
        if (xTimerStart(m_handle, 0) != pdPASS) {
            ESP_LOGE(m_name.data(), "Start failed");
            return false;
        }
        return true;
    }

    auto higherPriorityTaskWoken = pdFALSE;
    auto result = xTimerStartFromISR(m_handle, &higherPriorityTaskWoken);
    if (result == pdFALSE) {
        return false;
    } else {
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    }
    return true;
}

bool Timer::Stop() {
    if (!m_handle) {
        return true;
    }

    if (!IsInIsr()) {
        return xTimerStop(m_handle, 0) == pdPASS ? true : false;
    }

    auto higherPriorityTaskWoken = pdFALSE;
    auto result = xTimerStopFromISR(m_handle, &higherPriorityTaskWoken);
    if (result == pdFALSE) {
        return false;
    } else {
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    }
    return true;
}

bool Timer::ChangePeriod(uint32_t period) {
    if (!IsInIsr()) {
        return xTimerChangePeriod(m_handle, period, 0);
    }
    auto higherPriorityTaskWoken = pdFALSE;
    auto result =
        xTimerChangePeriodFromISR(m_handle, period, &higherPriorityTaskWoken);
    if (result == pdFALSE) {
        return false;
    } else {
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    }
    return true;
}

TimerHandle_t& Timer::GetHandle() {
    return m_handle;
};

bool Timer::Setup() {
    m_handle = xTimerCreate(m_name.data(),
                            m_periodMs / portTICK_PERIOD_MS,
                            m_autoReload,
                            this,
                            Callback);
    if (!m_handle) {
        ESP_LOGE(m_name.data(), "Setup failed");
        return false;
    }
    return true;
}

void Timer::Callback(TimerHandle_t xTimer) {
    Timer* obj = static_cast<Timer*>(pvTimerGetTimerID(xTimer));
    obj->m_callback();
}

Task::Task(std::string_view name,
           uint32_t size,
           uint32_t priority,
           bool (*initFunction)(),
           void (*handlerFunction)(),
           int32_t coreId)
    : m_name(name),
      m_size(size),
      m_priority(priority),
      m_initFunction(initFunction),
      m_handlerFunction(handlerFunction),
      m_coreId(coreId) {}

Task::~Task() {
    if (!m_handle) {
        return;
    }
    vTaskDelete(m_handle);
    m_handle = nullptr;
}

Task::Task(Task&& other) noexcept
    : m_handle(std::exchange(other.m_handle, nullptr)),
      m_name(std::exchange(other.m_name, "")),
      m_size(std::exchange(other.m_size, 0)),
      m_priority(std::exchange(other.m_priority, 0)),
      m_initFunction(std::exchange(other.m_initFunction, nullptr)),
      m_handlerFunction(std::exchange(other.m_handlerFunction, nullptr)),
      m_coreId(std::exchange(other.m_coreId, tskNO_AFFINITY)) {}

Task& Task::operator=(Task&& other) noexcept {
    if (&other != this) {
        std::swap(m_handle, other.m_handle);
        std::swap(m_name, other.m_name);
        std::swap(m_size, other.m_size);
        std::swap(m_priority, other.m_priority);
        std::swap(m_initFunction, other.m_initFunction);
        std::swap(m_handlerFunction, other.m_handlerFunction);
        std::swap(m_coreId, other.m_coreId);
    }
    return *this;
}

bool Task::Setup() {
    if (m_handle) {
        vTaskDelete(m_handle);
        m_handle = nullptr;
    }
    if (!m_initFunction && !m_handlerFunction) {
        ESP_LOGE(m_name.data(), "Invalid functions");
        return false;
    }
    if (m_size == 0) {
        ESP_LOGE(m_name.data(), "Invalid stack size");
        return false;
    }
    if (m_priority >= configMAX_PRIORITIES) {
        ESP_LOGE(m_name.data(), "Invalid priority");
        return false;
    }

    if (xTaskCreatePinnedToCore(TaskFunction,
                                m_name.data(),
                                m_size,
                                this,
                                m_priority,
                                &m_handle,
                                m_coreId) != pdPASS) {
        ESP_LOGE(m_name.data(), "Insufficient heap");
        return false;
    }
    return true;
}

void Task::Destroy() {
    if (m_handle) {
        vTaskDelete(m_handle);
        m_handle = nullptr;
    }
}

TaskHandle_t& Task::GetHandle() {
    return m_handle;
}

void Task::TaskFunction(void* arg) {
    Task* obj = static_cast<Task*>(arg);

    while (obj->m_initFunction && obj->m_initFunction() == false) {
        ESP_LOGE(obj->m_name.data(), "Init failed");
        rtos::Delay(100);
    }
    ESP_LOGI(obj->m_name.data(), "Init Successful");

    if (!obj->m_handlerFunction) {
        vTaskDelete(obj->GetHandle());
        return;
    }

    while (1) {
        obj->m_handlerFunction();
    }
}

}  // namespace rtos
