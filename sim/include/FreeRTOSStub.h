#pragma once

#include <cstdint>

using TaskHandle_t = void*;
using SemaphoreHandle_t = void*;
using BaseType_t = int;
using UBaseType_t = unsigned;
using TickType_t = uint32_t;

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

int xTaskCreate(void (*fn)(void*), const char* name, unsigned stack, void* param, int prio,
                TaskHandle_t* handle);
void vTaskDelete(TaskHandle_t h);
TaskHandle_t xTaskGetCurrentTaskHandle();

BaseType_t xTaskNotify(TaskHandle_t task, uint32_t value, eNotifyAction action);
uint32_t ulTaskNotifyTake(BaseType_t clearCountOnExit, TickType_t ticksToWait);

SemaphoreHandle_t xSemaphoreCreateMutex();
void xSemaphoreTake(SemaphoreHandle_t m, unsigned timeout);
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
