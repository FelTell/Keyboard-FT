#pragma once

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <functional>
#include <optional>

namespace rtos {

void Delay(const TickType_t ticksToDelay);

template <typename T>
class Queue {
  public:
    Queue(uint32_t size) : m_size(size) {}

    bool Setup() {
        m_handle = xQueueCreate(m_size, sizeof(T));
        return (m_handle != nullptr);
    }

    bool Send(T& value) {
        return (xQueueSend(m_handle, &value, 0) == pdTRUE);
    }

    std::optional<T> Get() {
        T value;
        if (xQueueReceive(m_handle, &value, 0) == pdTRUE) {
            return value;
        };
        return std::nullopt;
    }
    std::optional<T> Wait(uint32_t timeout = 0xFFFFFFFF) {
        T value;
        if (xQueueReceive(m_handle, &value, timeout) == pdTRUE) {
            return value;
        };
        return std::nullopt;
    }

  private:
    QueueHandle_t m_handle;
    uint32_t m_size;
};

class Timer {
  public:
    Timer(const char* name,
          uint32_t period,
          bool autoReload,
          void (*callback)())
        : m_name(name),
          m_period(period),
          m_autoReload(autoReload),
          m_callback(callback) {}

    bool Start() {
        if (!m_handle) {
            if (!Setup()) {
                return false;
            }
        }

        if (xTimerStart(m_handle, 0) != pdPASS) {
            ESP_LOGE(m_name, "Start failed");
            return false;
        }
        return true;
    }

    bool Stop() {
        if (!m_handle) {
            return true;
        }
        return xTimerStop(m_handle, 0) == pdPASS ? true : false;
    }

    bool Setup() {
        m_handle = xTimerCreate(m_name, m_period, m_autoReload, this, Callback);
        if (!m_handle) {
            ESP_LOGE(m_name, "Setup failed");
            return false;
        }
        return true;
    }

    TimerHandle_t* GetHandle() {
        return &m_handle;
    };

  private:
    TimerHandle_t m_handle;
    const char* m_name;
    uint32_t m_period;
    bool m_autoReload;
    void (*m_callback)();

    static void Callback(TimerHandle_t xTimer) {
        Timer* obj = static_cast<Timer*>(pvTimerGetTimerID(xTimer));
        obj->m_callback();
    }
};

class Task {
  public:
    Task(const char* name,
         uint32_t size,
         uint32_t priority,
         bool (*initFunction)(),
         void (*handlerFunction)())
        : m_name(name),
          m_size(size),
          m_priority(priority),
          m_initFunction(initFunction),
          m_handlerFunction(handlerFunction) {}

    bool Setup() {
        if (xTaskCreate(TaskFunction,
                        m_name,
                        m_size,
                        this,
                        m_priority,
                        &m_handle) != pdPASS) {
            return false;
        }
        return true;
    }

    TaskHandle_t* GetHandle() {
        return &m_handle;
    };

  private:
    TaskHandle_t m_handle;
    const char* m_name;
    uint32_t m_size;
    uint32_t m_priority;
    bool (*m_initFunction)();
    void (*m_handlerFunction)();

    static void TaskFunction(void* arg) {
        Task* obj = static_cast<Task*>(arg);
        while (obj->m_initFunction() == false) {
            ESP_LOGE(obj->m_name, "Init failed");
            rtos::Delay(100);
        }
        ESP_LOGI(obj->m_name, "Init Successful");
        while (1) {
            obj->m_handlerFunction();
        }
    }
};

} // namespace rtos
