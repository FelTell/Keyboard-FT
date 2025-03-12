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
 
 #include <optional>
 #include <string_view>
 
 #include <esp_log.h>
 #include <freertos/FreeRTOS.h>
 #include <freertos/event_groups.h>
 #include <freertos/queue.h>
 #include <freertos/task.h>
 #include <freertos/timers.h>
 
 static_assert(sizeof(BaseType_t) == sizeof(uint32_t),
               "Unexpected RTOS base type, check configuration");
 
 namespace rtos {
 
 static constexpr uint32_t INFINITE_TIMEOUT = 0xFFFFFFFF;
 
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
     Queue(uint32_t size) : m_size(size), m_handle(nullptr) {}
 
     /**
      * @brief Creates a new queue.
      *
      * @return true Queue was created successfully
      * @return false Queue was not created. Insufficient heap.
      */
     bool Setup() {
         if (m_handle) {
             vQueueDelete(m_handle);
         }
         m_handle = xQueueCreate(m_size, sizeof(T));
         if (!m_handle) {
             ESP_LOGE("VariableSetup", "Insufficient heap");
             return false;
         }
         return true;
     }
 
     /**
      * @brief Post an item on the back of a queue. The item is queued by copy,
      * not by reference.
      *
      * @param value The item that is to be placed on the queue.
      * @return true Item was added successfully to the queue.
      * @return false Item was not added to the queue. Queue is full.
      */
     bool Send(const T& value) {
         if (!m_handle) {
             if (!IsInIsr()) {
                 ESP_LOGE("Queue", "Handle is null, check if setup was done");
             }
             return false;
         }
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
     bool SendToFront(const T& value) {
         if (!m_handle) {
             if (!IsInIsr()) {
                 ESP_LOGE("Queue", "Handle is null, check if setup was done");
             }
             return false;
         }
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
         if (!m_handle) {
             if (!IsInIsr()) {
                 ESP_LOGE("Queue", "Handle is null, check if setup was done");
             }
             return false;
         }
         if (!IsInIsr()) {
             return Wait(value, 0);
         }
 
         auto higherPriorityTaskWoken = pdFALSE;
         if (xQueueReceiveFromISR(m_handle, &value, &higherPriorityTaskWoken) ==
             pdTRUE) {
             portYIELD_FROM_ISR(higherPriorityTaskWoken);
             return true;
         }
         return false;
     }
 
     /**
      * @brief Get a item from the queue without removing it from the queue
      *
      * @param value Item that will be received, will not change if queue is
      * empty
      * @return true Item was retrieved successfully from the queue.
      * @return false Item was not retried from the queue. The queue is empty.
      */
     bool Peek(T& value) {
         if (!m_handle) {
             if (!IsInIsr()) {
                 ESP_LOGE("Queue", "Handle is null, check if setup was done");
             }
             return false;
         }
         if (!IsInIsr()) {
             return xQueuePeek(m_handle, &value, 0) == pdTRUE;
         }
 
         return (xQueuePeekFromISR(m_handle, &value) == pdTRUE);
     }
 
     uint32_t GetQueueSize() {
         return uxQueueMessagesWaiting(m_handle);
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
     bool Wait(T& value, uint32_t timeout = INFINITE_TIMEOUT) {
         if (!m_handle) {
             if (!IsInIsr()) {
                 ESP_LOGE("Queue", "Handle is null, check if setup was done");
             }
             return false;
         }
         if (IsInIsr()) {
             return false;
         }
 
         if (xQueueReceive(m_handle, &value, timeout) == pdTRUE) {
             return true;
         }
         return false;
     }
 
     /**
      * @brief Getter for the queue handler
      *
      * @return QueueHandle_t*
      */
     QueueHandle_t& GetHandle() {
         return m_handle;
     }
 
     /**
      * @brief Get the queue max size
      *
      * @return uint32_t
      */
     uint32_t GetMaxSize() {
         return m_size;
     }
 
    private:
     uint32_t m_size;
     QueueHandle_t m_handle;
 };
 
 template <typename T>
 class Variable {
    public:
     /**
      * @brief Variable is a queue with size 1. It will always have a value, and
      * after setup it should never fail. It should be used as a simple and safe
      * way to share a value between tasks.
      *
      * @param defaultValue The value that the variable will be initialized as.
      */
     Variable(T defaultValue)
         : m_defaultValue(defaultValue), m_handle(nullptr) {}
 
     /**
      * @brief Creates a new queue with size 1.
      *
      * @return true Queue was created successfully
      * @return false Queue was not created. Insufficient heap.
      */
     bool Setup() {
         if (m_handle) {
             vQueueDelete(m_handle);
         }
         m_handle = xQueueCreate(1, sizeof(T));
         if (!m_handle) {
             ESP_LOGE("QueueSetup", "Insufficient heap");
             return false;
         }
         Set(m_defaultValue);
         return true;
     }
 
     /**
      * @brief Set an item on the queue. The item is queued by copy, not by
      * reference. Will always overwrite the queue, so no error return is
      * possible.
      *
      * @param value The item that is to be placed on the queue.
      */
     void Set(const T& value) {
         if (!m_handle) {
             if (!IsInIsr()) {
                 ESP_LOGI("Variable", "Handle is null, check if setup was done");
             }
             return;
         }
         if (!IsInIsr()) {
             xQueueOverwrite(m_handle, &value);
         }
 
         auto higherPriorityTaskWoken = pdFALSE;
         auto result =
             xQueueOverwriteFromISR(m_handle, &value, &higherPriorityTaskWoken);
         if (result != pdFALSE) {
             portYIELD_FROM_ISR(higherPriorityTaskWoken);
         }
     }
 
     /**
      * @brief Get a item from the queue
      *
      * @return value
      */
     T Get() {
         if (!m_handle) {
             if (!IsInIsr()) {
                 ESP_LOGI("Variable", "Handle is null, check if setup was done");
             }
             return m_defaultValue;
         }
 
         T value;
 
         if (!IsInIsr()) {
             xQueuePeek(m_handle, &value, 0);
         } else {
             xQueuePeekFromISR(m_handle, &value);
         }
         return value;
     }
 
     /**
      * @brief Getter for the queue handler
      *
      * @return QueueHandle_t*
      */
     QueueHandle_t& GetHandle() {
         return m_handle;
     }
 
    private:
     const T m_defaultValue;
     QueueHandle_t m_handle;
 };
 
 class Event {
    public:
     /**
      * @brief An event group is a set of event bits. Event bits are used to
      * indicate if an event has occurred or not. Event bits are often referred
      * to as event flags.
      *
      */
     Event();
 
     ~Event();
 
     Event(const Event&)            = delete;
     Event& operator=(const Event&) = delete;
 
     Event(Event&&);
     Event& operator=(Event&&);
 
     static constexpr uint8_t AVAILABLE_BITS = 24;
 
     /**
      * @brief Creates an RTOS event group. Resets event group if setup has been
      * called before.
      *
      * @return true Event was created successfully
      * @return false Event was not created. Insufficient heap.
      */
     bool Setup();
 
     /**
      * @brief Set one or multiple bits.
      *
      * @param bitsToSet Bitwise value. 24 bits available.
      * @return uint32_t The value of the event group at the end of this function
      * execution. If a higher priority task is called the returned value might
      * have the bits specified by the bitsToSet parameter cleared. If called
      * from a interruption it will always return 0.
      */
     uint32_t Set(uint32_t bitsToSet);
 
     /**
      * @brief Clear one or multiple bits.
      *
      * @param bitsToClear Bitwise value.
      * @return uint32_t The value of the event group before the specified bits
      * were cleared. If called from a interruption it will always return 0.
      */
     uint32_t Clear(uint32_t bitsToClear);
 
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
                                  uint32_t timeout = INFINITE_TIMEOUT);
 
     /**
      * @brief Get the value of the event group.
      *
      * @return uint32_t Value of the event group.
      */
     uint32_t Get();
 
     /**
      * @brief Getter for the event queue handler
      *
      * @return EventGroupHandle_t*
      */
     EventGroupHandle_t& GetHandle();
 
    private:
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
     Timer(std::string_view name,
           uint32_t periodMs,
           bool autoReload,
           void (*callback)());
 
     ~Timer();
 
     Timer(const Timer&)            = delete;
     Timer& operator=(const Timer&) = delete;
 
     Timer(Timer&&);
     Timer& operator=(Timer&&);
 
     /**
      * @brief Start the timer. If it has already been started this function will
      * reset the current countdown.
      *
      * @return true Timer was started successfully
      * @return false Timer was not started. Insufficient heap or timer command
      * queue is full.
      */
     bool Start();
 
     /**
      * @brief Stops a timer that was previously started
      *
      * @return true Timer was stopped successfully
      * @return false Timer was not stopped. Timer command queue is full.
      */
     bool Stop();
 
     /**
      * @brief Changed period of a timer
      *
      * @param period value in ms
      * @return true Period was changed successfully
      * @return false Period was not Changed. Timer command queue is full.
      */
     bool ChangePeriod(uint32_t period);
 
     /**
      * @brief Getter for the timer handler
      *
      * @return TimerHandle_t*
      */
     TimerHandle_t& GetHandle();
 
     bool Setup();
 
    private:
     TimerHandle_t m_handle;
     std::string_view m_name;
     uint32_t m_periodMs;
     bool m_autoReload;
     void (*m_callback)();
 
     static void Callback(TimerHandle_t xTimer);
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
     Task(std::string_view name,
          uint32_t size,
          uint32_t priority,
          bool (*initFunction)(),
          void (*handlerFunction)(),
          int32_t coreId = tskNO_AFFINITY);
 
     ~Task();
 
     Task(const Task&)            = delete;
     Task& operator=(const Task&) = delete;
 
     Task(Task&&);
     Task& operator=(Task&&);
 
     /**
      * @brief Create a new task and add it to the list of tasks that are ready
      * to run
      *
      * @return true Task was created successfully
      * @return false Task was not created. Insufficient heap or incorrect
      * parameters.
      */
     bool Setup();
 
     /**
      * @brief Destroy task so that it's no longer called. Setup will be needed
      * to restart task operation
      *
      */
     void Destroy();
 
     /**
      * @brief Getter for the task handler
      *
      * @return TaskHandle_t*
      */
     TaskHandle_t& GetHandle();
 
    private:
     TaskHandle_t m_handle;
     std::string_view m_name;
     uint32_t m_size;
     uint32_t m_priority;
     bool (*m_initFunction)();
     void (*m_handlerFunction)();
     int32_t m_coreId;
 
     static void TaskFunction(void* arg);
 };
 
 }  // namespace rtos
 