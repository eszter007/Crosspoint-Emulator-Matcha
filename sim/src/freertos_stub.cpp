#include "FreeRTOSStub.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <exception>
#include <mutex>
#include <thread>
#include <unordered_map>

namespace {

struct TaskExit : std::exception {};

struct TaskInfo {
  std::thread thread;
  std::atomic<bool> cancelled{false};
};

struct SimMutex {
  std::mutex mtx;
  std::atomic<TaskHandle_t> owner{nullptr};
};

std::unordered_map<TaskHandle_t, TaskInfo*> s_tasks;
std::mutex s_tasksMutex;

std::unordered_map<TaskHandle_t, uint32_t> s_notifyCounts;
std::mutex s_notifyMutex;
std::condition_variable s_notifyCv;

thread_local TaskHandle_t t_currentHandle = nullptr;
thread_local TaskInfo* t_currentInfo = nullptr;

const TaskHandle_t kMainPseudoTask = reinterpret_cast<TaskHandle_t>(static_cast<uintptr_t>(1));

inline TaskHandle_t currentHandleOrMain() {
  return t_currentHandle ? t_currentHandle : kMainPseudoTask;
}

inline void checkCancelled() {
  if (t_currentInfo && t_currentInfo->cancelled.load()) throw TaskExit();
}

}  // namespace

void vTaskDelay(unsigned ms) {
  checkCancelled();
  constexpr unsigned kSliceMs = 5;
  unsigned remaining = ms;
  while (remaining > 0) {
    const unsigned sleepMs = remaining < kSliceMs ? remaining : kSliceMs;
    std::this_thread::sleep_for(std::chrono::milliseconds(sleepMs));
    remaining -= sleepMs;
    checkCancelled();
  }
}

int xTaskCreate(void (*fn)(void*), const char*, unsigned, void* param, int, TaskHandle_t* handle) {
  auto h = reinterpret_cast<TaskHandle_t>(new uintptr_t(0));
  auto* info = new TaskInfo();
  {
    std::lock_guard<std::mutex> lock(s_tasksMutex);
    s_tasks[h] = info;
  }

  info->thread = std::thread([fn, param, h, info]() {
    t_currentHandle = h;
    t_currentInfo = info;
    try {
      fn(param);
    } catch (const TaskExit&) {
    }
    t_currentHandle = nullptr;
    t_currentInfo = nullptr;
  });

  if (handle) *handle = h;
  return pdPASS;
}

void vTaskDelete(TaskHandle_t h) {
  if (!h) return;
  TaskInfo* info = nullptr;
  {
    std::lock_guard<std::mutex> lock(s_tasksMutex);
    const auto it = s_tasks.find(h);
    if (it != s_tasks.end()) {
      info = it->second;
      s_tasks.erase(it);
    }
  }
  if (!info) return;

  info->cancelled.store(true);
  {
    std::lock_guard<std::mutex> lock(s_notifyMutex);
    s_notifyCounts[h] = 1;
  }
  s_notifyCv.notify_all();

  if (info->thread.joinable()) info->thread.join();
  delete info;
  delete reinterpret_cast<uintptr_t*>(h);
}

TaskHandle_t xTaskGetCurrentTaskHandle() { return currentHandleOrMain(); }

BaseType_t xTaskNotify(TaskHandle_t task, uint32_t value, eNotifyAction action) {
  if (!task) return pdFALSE;
  std::lock_guard<std::mutex> lock(s_notifyMutex);
  uint32_t& count = s_notifyCounts[task];
  if (action == eIncrement) {
    count += value ? value : 1;
  } else {
    count += 1;
  }
  s_notifyCv.notify_all();
  return pdTRUE;
}

uint32_t ulTaskNotifyTake(BaseType_t clearCountOnExit, TickType_t ticksToWait) {
  const TaskHandle_t self = currentHandleOrMain();

  std::unique_lock<std::mutex> lock(s_notifyMutex);
  auto ready = [&]() { return s_notifyCounts[self] > 0; };

  if (ticksToWait == portMAX_DELAY) {
    while (!ready()) {
      lock.unlock();
      checkCancelled();
      lock.lock();
      s_notifyCv.wait_for(lock, std::chrono::milliseconds(5));
    }
  } else if (!ready()) {
    const auto timeout = std::chrono::milliseconds(ticksToWait * portTICK_PERIOD_MS);
    s_notifyCv.wait_for(lock, timeout, ready);
  }

  uint32_t& count = s_notifyCounts[self];
  if (count == 0) return 0;

  const uint32_t ret = count;
  if (clearCountOnExit == pdTRUE)
    count = 0;
  else
    count -= 1;
  return ret;
}

SemaphoreHandle_t xSemaphoreCreateMutex() { return reinterpret_cast<SemaphoreHandle_t>(new SimMutex()); }

void xSemaphoreTake(SemaphoreHandle_t m, unsigned) {
  auto* mutexObj = static_cast<SimMutex*>(m);
  if (!mutexObj) return;

  while (!mutexObj->mtx.try_lock()) {
    checkCancelled();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  mutexObj->owner.store(currentHandleOrMain());
}

void xSemaphoreGive(SemaphoreHandle_t m) {
  auto* mutexObj = static_cast<SimMutex*>(m);
  if (!mutexObj) return;
  mutexObj->owner.store(nullptr);
  mutexObj->mtx.unlock();
}

TaskHandle_t xSemaphoreGetMutexHolder(SemaphoreHandle_t m) {
  auto* mutexObj = static_cast<SimMutex*>(m);
  if (!mutexObj) return nullptr;
  return mutexObj->owner.load();
}

BaseType_t xQueuePeek(SemaphoreHandle_t q, void*, TickType_t) {
  auto* mutexObj = static_cast<SimMutex*>(q);
  if (!mutexObj) return pdFALSE;
  if (mutexObj->mtx.try_lock()) {
    mutexObj->mtx.unlock();
    return pdTRUE;
  }
  return pdFALSE;
}

void vSemaphoreDelete(SemaphoreHandle_t m) { delete static_cast<SimMutex*>(m); }
