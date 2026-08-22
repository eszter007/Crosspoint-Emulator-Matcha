#pragma once

// ESP-IDF GPIO shim. BoardConfig.h calls gpio_hold_dis() when asserting a
// board's power-rail latches; the emulator has no pins to latch, so the whole
// API is inert here. Kept as real declarations rather than macros so the SDK
// header compiles unmodified.

#include <cstdint>

typedef int gpio_num_t;

inline int gpio_hold_dis(gpio_num_t) { return 0; }
inline int gpio_hold_en(gpio_num_t) { return 0; }
