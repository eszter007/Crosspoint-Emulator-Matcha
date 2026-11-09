#pragma once

#include <atomic>
#include <cstdint>

using TaskHandle_t = void*;
using SemaphoreHandle_t = void*;
using BaseType_t = int;
using UBaseType_t = unsigned;
using TickType_t = uint32_t;
// The emulator runs FreeRTOS tasks as real std::threads, so a critical section
// has to be genuine mutual exclusion -- a no-op would let the render task read a
// half-published struct, the very race the firmware uses these for. ESP-IDF's
// portENTER_CRITICAL is a spinlock plus an interrupt disable; the spinlock is
// the part that has meaning on the host, and the guarded regions are a few
// dozen bytes, so spinning is cheaper than a mutex.
struct portMUX_TYPE {
  std::atomic<bool> locked{false};
};

enum eNotifyAction {
  eNoAction = 0,
  eSetBits,
  eIncrement,
  eSetValueWithOverwrite,
  eSetValueWithoutOverwrite,
};

constexpr BaseType_t pdFALSE = 0;
constexpr BaseType_t pdTRUE = 1;
constexpr BaseType_t pdPASS = 1;
constexpr TickType_t portMAX_DELAY = 0xFFFFFFFFu;
constexpr int portTICK_PERIOD_MS = 1;
#define portMUX_INITIALIZER_UNLOCKED \
  {}

inline void portENTER_CRITICAL(portMUX_TYPE* mux) {
  while (mux->locked.exchange(true, std::memory_order_acquire)) {
  }
}
inline void portEXIT_CRITICAL(portMUX_TYPE* mux) { mux->locked.store(false, std::memory_order_release); }

int xTaskCreate(void (*fn)(void*), const char* name, unsigned stack, void* param, int prio,
                TaskHandle_t* handle);
int xTaskCreatePinnedToCore(void (*fn)(void*), const char* name, unsigned stack, void* param, int prio,
                            TaskHandle_t* handle, BaseType_t core);
void vTaskDelete(TaskHandle_t h);
TaskHandle_t xTaskGetCurrentTaskHandle();

BaseType_t xTaskNotify(TaskHandle_t task, uint32_t value, eNotifyAction action);
BaseType_t xTaskNotifyGive(TaskHandle_t task);
uint32_t ulTaskNotifyTake(BaseType_t clearCountOnExit, TickType_t ticksToWait);
uint32_t ulTaskNotifyValueClear(TaskHandle_t task, uint32_t bitsToClear);

SemaphoreHandle_t xSemaphoreCreateMutex();
BaseType_t xSemaphoreTake(SemaphoreHandle_t m, TickType_t timeout);
void xSemaphoreGive(SemaphoreHandle_t m);
TaskHandle_t xSemaphoreGetMutexHolder(SemaphoreHandle_t m);
BaseType_t xQueuePeek(SemaphoreHandle_t q, void* outItem, TickType_t timeout);
void vSemaphoreDelete(SemaphoreHandle_t m);

void vTaskDelay(unsigned ms);

#define taskENTER_CRITICAL(x) \
  do {                        \
    (void)(x);                \
  } while (0)

#define taskEXIT_CRITICAL(x) \
  do {                       \
    (void)(x);               \
  } while (0)
