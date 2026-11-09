#pragma once

#ifndef PROGMEM
#define PROGMEM
#endif

#include <cstddef>
#include <cstdint>

#include "ArduinoStub.h"
// Arduino-ESP32's Arduino.h pulls in the FreeRTOS API, and firmware sources
// rely on that transitively (vTaskDelay after a plain #include <Arduino.h>).
#include "FreeRTOSStub.h"
#include "WString.h"

typedef uint8_t byte;
