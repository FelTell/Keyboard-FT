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
 * @brief Checks if the code currently running is inside an ISR
 *
 */
bool IsInIsr();

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
 * @brief Get the count of ticks since the scheduler was started.
 *
 * @return uint32_t Count of ticks
 */
uint32_t GetTickCount();

template <typename T>
class Queue {
  public:
    /**
     * @brief Queues should be used to send messages between tasks, and between
     * interrupts and tasks. In most cases they are used as thread safe FIFO
     * buffers, although data can also be sent to the front.
     *
     * @param size The size of the queue.
     */
    Queue(uint32_t size) : m_size(size) {}

    /**
     * @brief Creates a new queue.
     *
     * @return true Event was created successfully
     * @return false Event was not created. Insufficient heap.
     */
    bool Setup() {
        m_handle = xQueueCreate(m_size, sizeof(T));
        return (m_handle != nullptr);
    }

    /**
     * @brief Post an item on the back of a queue. The item is queued by copy,
     * not by reference.
     *
     * @param value The item that is to be placed on the queue.
     * @return true Item was added successfully to the queue.
     * @return false Item was not added to the queue. Queue is full.
     */
    bool Send(T& value) {
        if (!IsInIsr()) {
            return (xQueueSend(m_handle, &value, 0) == pdTRUE);
        }

        auto higherPriorityTaskWoken = pdFALSE;
        auto result =
            xQueueSendFromISR(m_handle, &value, &higherPriorityTaskWoken);
        if (result == pdFALSE) {
            return false;
        } else {
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
            return true;
        }
    }

    /**
     * @brief Post an item on the front of a queue. The item is queued by copy,
     * not by reference.
     *
     * @param value The item that is to be placed on the queue.
     * @return true Item was added successfully to the queue.
     * @return false Item was not added to the queue. Queue is full.
     */
    bool SendToFront(T& value) {
        if (!IsInIsr()) {
            return (xQueueSendToFront(m_handle, &value, 0) == pdTRUE);
        }

        auto higherPriorityTaskWoken = pdFALSE;
        auto result                  = xQueueSendToFrontFromISR(m_handle,
                                               &value,
                                               &higherPriorityTaskWoken);
        if (result == pdFALSE) {
            return false;
        } else {
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
            return true;
        }
    }

    /**
     * @brief Get a item from the queue
     *
     * @param value Item that will be received, will not change if queue is
     * empty
     * @return true Item was retrieved successfully from the queue.
     * @return false Item was not retried from the queue. The queue is empty.
     */
    bool Get(T& value) {
        if (!IsInIsr()) {
            return Wait(value, 0);
        }

        auto higherPriorityTaskWoken = pdFALSE;
        if (xQueueReceiveFromISR(m_handle, &value, &higherPriorityTaskWoken) ==
            pdTRUE) {
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
            return true;
        };
        return false;
    }

    /**
     * @brief Get a item from the queue, wait if empty
     *
     * @param value Item that will be received, will not change if timout has
     * expired
     * @param timeout Time to wait to receive a new item from the queue
     * @return true Item was retrieved successfully from the queue.
     * @return false Item was not retried from the queue. The queue is empty and
     * the timeout has expired.
     */
    bool Wait(T& value, uint32_t timeout = 0xFFFFFFFF) {
        if (IsInIsr()) {
            ESP_LOGE("QueueWait", "Do not call a wait function from ISR");
            return false;
        }

        if (xQueueReceive(m_handle, &value, timeout) == pdTRUE) {
            return true;
        };
        return false;
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
     * @brief Set one or multiple bits.
     *
     * @param bitsToSet Bitwise value. 24 bits available.
     * @return uint32_t The value of the event group at the end of this function
     * execution. If a higher priority task is called the returned value might
     * have the bits specified by the bitsToSet parameter cleared. If called
     * from a interruption it will always return 0.
     */
    uint32_t Set(uint32_t bitsToSet) {
        if (bitsToSet >= (1 << m_AVAILABLE_BITS)) {
            ESP_LOGE("EventSet",
                     "A bit to set was higher than the allowed: %d",
                     m_AVAILABLE_BITS);
            return 0;
        }

        if (!IsInIsr()) {
            return xEventGroupSetBits(m_handle, bitsToSet);
        }

        auto higherPriorityTaskWoken = pdFALSE;
        auto result                  = xEventGroupSetBitsFromISR(m_handle,
                                                bitsToSet,
                                                &higherPriorityTaskWoken);
        if (result == pdFALSE) {
            ESP_LOGE("EventSetFromIsr", "Timer command queue is full");
        } else {
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
        }
        return 0;
    };

    /**
     * @brief Clear one or multiple bits.
     *
     * @param bitsToClear Bitwise value.
     * @return uint32_t The value of the event group before the specified bits
     * were cleared. If called from a interrution it will always return 0.
     */
    uint32_t Clear(uint32_t bitsToClear) {
        if (bitsToClear >= (1 << m_AVAILABLE_BITS)) {
            ESP_LOGE("EventClear",
                     "A bit to clear was higher than the allowed: %d",
                     m_AVAILABLE_BITS);
            return 0;
        }

        if (!IsInIsr()) {
            return xEventGroupClearBits(m_handle, bitsToClear);
        }

        auto result = xEventGroupClearBitsFromISR(m_handle, bitsToClear);
        if (result == pdFALSE) {
            ESP_LOGE("EventClearFromIsr", "Timer command queue is full");
        }
        return 0;
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
        if (IsInIsr()) {
            ESP_LOGE("EventWait", "Do not call a wait function from ISR");
            return std::nullopt;
        }

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
     * @brief Get the value of the event group. Call only from ans interruption.
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
    /**
     * @brief A software timer (or just a 'timer') allows a function to be
     * executed at a set time in the future.
     *
     * @param name Name of the timer.
     * @param periodMs Period in milliseconds. If configTICK_RATE_HZ is lower
     * than 1000 the timer might not work as expected for small periods.
     * @param autoReload If set to true the timer will automatically reset and
     * start again after it has expired. If set to false it will stop after it
     * has expired
     * @param callback A function to call after the timer has expired.
     */
    Timer(const char* name,
          uint32_t periodMs,
          bool autoReload,
          void (*callback)())
        : m_name(name),
          m_periodMs(periodMs),
          m_autoReload(autoReload),
          m_callback(callback) {}

    /**
     * @brief Start the timer. If it has already been started this function will
     * reset the current countdown.
     *
     * @return true Timer was started successfully
     * @return false Timer was not started. Insufficient heap or timer command
     * queue is full.
     */
    bool Start() {
        if (!m_handle) {
            if (!Setup()) {
                return false;
            }
        }

        if (!IsInIsr()) {
            if (xTimerStart(m_handle, 0) != pdPASS) {
                ESP_LOGE(m_name, "Start failed");
                return false;
            }
            return true;
        }

        auto higherPriorityTaskWoken = pdFALSE;
        auto result = xTimerStartFromISR(m_handle, &higherPriorityTaskWoken);
        if (result == pdFALSE) {
            ESP_LOGE("Start failed", "Timer command queue is full");
            return false;
        } else {
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
        }
        return true;
    }

    /**
     * @brief Stops a timer that was previously started
     *
     * @return true Timer was stopped successfully
     * @return false Timer was not stopped. Timer command queue is full.
     */
    bool Stop() {
        if (!m_handle) {
            return true;
        }

        if (!IsInIsr()) {
            return xTimerStop(m_handle, 0) == pdPASS ? true : false;
        }

        auto higherPriorityTaskWoken = pdFALSE;
        auto result = xTimerStopFromISR(m_handle, &higherPriorityTaskWoken);
        if (result == pdFALSE) {
            ESP_LOGE("Stop failed", "Timer command queue is full");
            return false;
        } else {
            portYIELD_FROM_ISR(higherPriorityTaskWoken);
        }
        return true;
    }

    /**
     * @brief Getter for the task handler
     *
     * @return TimerHandle_t*
     */
    TimerHandle_t* GetHandle() {
        return &m_handle;
    };

  private:
    TimerHandle_t m_handle;
    const char* m_name;
    uint32_t m_periodMs;
    bool m_autoReload;
    void (*m_callback)();

    bool Setup() {
        m_handle = xTimerCreate(m_name,
                                m_periodMs / portTICK_PERIOD_MS,
                                m_autoReload,
                                this,
                                Callback);
        if (!m_handle) {
            ESP_LOGE(m_name, "Setup failed");
            return false;
        }
        return true;
    }

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
