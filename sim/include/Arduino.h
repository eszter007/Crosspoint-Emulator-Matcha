#pragma once

#ifndef PROGMEM
#define PROGMEM
#endif

#include <cstddef>
#include <cstdint>

#include "ArduinoStub.h"
// Arduino-ESP32's Arduino.h pulls the FreeRTOS headers in, and firmware code
// reaches vTaskDelay() and friends through it.
#include "FreeRTOSStub.h"
#include "WString.h"

typedef uint8_t byte;
