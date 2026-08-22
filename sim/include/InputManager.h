#pragma once

// Stub for InputManager. The emulator drives HalGPIO straight from SDL rather
// than through the SDK's ADC-ladder/I2C reader, so none of the real class is
// needed -- but BoardConfig.h must still come in through this header, exactly
// as it does from the SDK's InputManager.h. Firmware headers reach the
// FREEINK_CAP_* capability macros this way (HalGPIO.h -> InputManager.h), and a
// header that misses them silently compiles away every touch-gated declaration
// while the matching .cpp still defines it.
#include <BoardConfig.h>

enum class ButtonEvent {
  IDLE,
  PRESSED,
  RELEASED,
  LONG_PRESSED,
};

class InputManager {
 public:
  static InputManager& getInstance() {
    static InputManager instance;
    return instance;
  }
  
  void begin() {}
  ButtonEvent update() { return ButtonEvent::IDLE; }
};

extern InputManager inputManager;
