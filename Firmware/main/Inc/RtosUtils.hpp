/**
 * @author Felipe Telles (felipe.melo.telles@gmail.com)
 * @brief This file helps develop with freeRTOS in C++ by enclosing some of the
 * most used functions provided by freeRTOS with classes. There are also some
 * extra code that is added on some of the freeRTOS commands that are required
 * for proper RTOS implementation. For now it's only supported on ESP32 devices.
 * The documentation here is based on the documentation available in
 * freertos.org
 *
 *
 */

#pragma once

#ifndef ESP_PLATFORM
#error This file is on a non supported platform
#endif

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/queue.h>
#include <freertos/task.h>
#include <freertos/timers.h>
#include <optional>

static_assert(sizeof(BaseType_t) == sizeof(uint32_t),
              "Unexpected RTOS base type, check configuration");

namespace rtos {

/**
 * @brief Delay a task for a given number of milliseconds.
 *
 * @param msToDelay Delay in milliseconds. If configTICK_RATE_HZ is lower than
 * 1000 the delay might not work as expected for small values.
 */
void Delay(const uint32_t msToDelay);

/**
 * @brief Delay a task until a specified time.
 * This function differs from Delay() in one important aspect: Delay()
 * specifies a time at which the task wishes to unblock relative to the time at
 * which vTaskDelay() is called, whereas vTaskDelayUntil() specifies an absolute
 * time at which the task wishes to unblock.
 *
 * @param previousTime A variable to store the last time the task was released.
 * The variable must be initialised with the current time using GetTickCount(),
 * after that it will be automatically updated.
 * @param timeIncrement The time to delay the task
 * @return true The task was delayed.
 * @return false The task was not delayed, meaning the given time was in the
 * past.
 */
bool DelayUntil(uint32_t& previousTime, const uint32_t timeIncrement);

/**
 * @brief Get the count of ticks since the scheduler was started. Do not call
 * from a interruption.
 *
 * @return uint32_t Count of ticks
 */
uint32_t GetTickCount();

/**
 * @brief Get the count of ticks since the scheduler was started. Call only from
 * an interruption
 *
 * @return uint32_t Count of ticks
 */
uint32_t GetTickCountFromIsr();

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

class Event {
  public:
    /**
     * @brief An event group is a set of event bits. Event bits are used to
     * indicate if an event has occurred or not. Event bits are often referred
     * to as event flags.
     *
     */
    Event(){};

    /**
     * @brief Creates an RTOS event group
     *
     * @return true Event was created successfully
     * @return false Event was not created. Insufficient heap.
     */
    bool Setup() {
        m_handle = xEventGroupCreate();
        if (!m_handle) {
            ESP_LOGE("EventSetup", "Insufficient heap");
            return false;
        }
        return true;
    }

    /**
     * @brief Set one or multiple bits. Do not call from a interruption.
     *
     * @param bitsToSet Bitwise value. 24 bits available.
     * @return uint32_t The value of the event group at the end of this function
     * execution. If a higher priority task is called the returned value might
     * have the bits specified by the bitsToSet parameter cleared.
     */
    uint32_t Set(uint32_t bitsToSet) {
        if (bitsToSet >= (1 << m_AVAILABLE_BITS)) {
            ESP_LOGE("EventSet",
                     "A bit to set was higher than the allowed: %d",
                     m_AVAILABLE_BITS);
            return 0;
        }
        return xEventGroupSetBits(m_handle, bitsToSet);
    };

    /**
     * @brief Set one or multiple bits. Call only from an interruption.
     *
     * @param bitsToSet Bitwise value. 24 bits available.
     */
    void SetFromIsr(uint32_t bitsToSet) {
        if (bitsToSet >= (1 << m_AVAILABLE_BITS)) {
            ESP_LOGE("EventSet",
                     "A bit to set was higher than the allowed: %d",
                     m_AVAILABLE_BITS);
            return;
        }
        auto higherPriorityTaskWoken = pdFALSE;
        auto result                  = xEventGroupSetBitsFromISR(m_handle,
                                                bitsToSet,
                                                &higherPriorityTaskWoken);
        if (result == pdFALSE) {
            ESP_LOGE("EventSetFromIsr", "Timer command queue is full");
            return;
        }
        portYIELD_FROM_ISR(higherPriorityTaskWoken);
    };

    /**
     * @brief Clear one or multiple bits. Do not call from a interruption.
     *
     * @param bitsToClear Bitwise value.
     * @return uint32_t The value of the event group before the specified bits
     * were cleared.
     */
    uint32_t Clear(uint32_t bitsToClear) {
        if (bitsToClear >= (1 << m_AVAILABLE_BITS)) {
            ESP_LOGE("EventClear",
                     "A bit to clear was higher than the allowed: %d",
                     m_AVAILABLE_BITS);
            return 0;
        }
        return xEventGroupClearBits(m_handle, bitsToClear);
    };

    /**
     * @brief Clear one or multiple bits. Call only from an interruption.
     *
     * @param bitsToClear Bitwise value.
     */
    void ClearFromIsr(uint32_t bitsToClear) {
        if (bitsToClear >= (1 << m_AVAILABLE_BITS)) {
            ESP_LOGE("EventClearFromIsr",
                     "A bit to clear was higher than the allowed: %d",
                     m_AVAILABLE_BITS);
            return;
        }
        auto result = xEventGroupClearBitsFromISR(m_handle, bitsToClear);
        if (result == pdFALSE) {
            ESP_LOGE("EventClearFromIsr", "Timer command queue is full");
            return;
        }
    };

    /**
     * @brief Wait one or more bits to be set.
     *
     * @param bitsToWait Bitwise value. Do not set to 0.
     * @param clearOnExit If true the bits passed in bitsToWait will be
     * cleared if not returned because of a timeout. Default = false.
     * @param waitForAll If true all bits must be set (AND). If false just one
     * of the bits needs to be set (OR). Default = false.
     * @param timeout Maximum amount of time to wait. Default = infinite.
     * @return std::optional<uint32_t> nullopt if timeout has expired or not
     * bits were given. The current event group if the bits were set.
     */
    std::optional<uint32_t> Wait(uint32_t bitsToWait,
                                 bool clearOnExit = false,
                                 bool waitForAll  = false,
                                 uint32_t timeout = 0xFFFFFFFF) {
        if (bitsToWait >= (1 << m_AVAILABLE_BITS)) {
            ESP_LOGE("EventWait",
                     "A bit to wait was higher than the allowed: %d",
                     m_AVAILABLE_BITS);
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

    /**
     * @brief Get the value of the event group. Do not call from a interruption.
     *
     * @return uint32_t Value of the event group.
     */
    uint32_t Get() {
        return xEventGroupGetBits(m_handle);
    }

    /**
     * @brief Get the value of the event group. Call only from an interruption.
     *
     * @return uint32_t Value of the event group.
     */
    uint32_t GetFromIsr() {
        return xEventGroupGetBitsFromISR(m_handle);
    }

  private:
    static constexpr uint8_t m_AVAILABLE_BITS = 24;
    EventGroupHandle_t m_handle;
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
    /**
     * @brief An RTOS is structured as a set of independent tasks. Each task
     * executes within its own context with no coincidental dependency on other
     * tasks within the system or the RTOS scheduler itself.
     *
     * @param name Name of the task.
     * @param size Task's stack size in words (4 bytes).
     * @param priority A value from 0 to configMAX_PRIORITIES - 1, with 0 being
     * the lowest priority available (same as idle task) and
     * configMAX_PRIORITIES - 1 the highest priority.
     * @param initFunction Pointer to a function that will be called until it
     * returns true.
     * @param handlerFunction Pointer to function that will be called inside a
     * infinite loop. Ensure that the proper timing control is implemented.
     */
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

    /**
     * @brief Create a new task and add it to the list of tasks that are ready
     * to run
     *
     * @return true Task was created successfully
     * @return false Task was not created. Insufficient heap or incorrect
     * parameters.
     */
    bool Setup() {
        if (!m_initFunction) {
            ESP_LOGE(m_name, "Invalid init function");
            return false;
        }
        if (!m_handlerFunction) {
            ESP_LOGE(m_name, "Invalid handler function");
            return false;
        }
        if (m_size == 0) {
            ESP_LOGE(m_name, "Invalid stack size");
            return false;
        }
        if (m_priority >= configMAX_PRIORITIES) {
            ESP_LOGE(m_name, "Invalid priority");
            return false;
        }

        if (xTaskCreate(TaskFunction,
                        m_name,
                        m_size,
                        this,
                        m_priority,
                        &m_handle) != pdPASS) {
            ESP_LOGE(m_name, "Insufficient heap");
            return false;
        }
        return true;
    }

    /**
     * @brief Getter for the task handler
     *
     * @return TaskHandle_t*
     */
    TaskHandle_t* GetHandle() {
        return &m_handle;
    };

  private:
    TaskHandle_t m_handle;
    const char* m_name;
    const uint32_t m_size;
    const uint32_t m_priority;
    bool (*m_initFunction)();
    void (*m_handlerFunction)();

    /**
     * @brief This is the actual task function. It's a static function that gets
     * as argument the class of the task. To make it more similar to normal bare
     * metal development it calls the init function once (if sucessfull) and a
     * handler function inside an infinite loop.
     *
     * @param arg
     */
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
