#pragma once

// Stub for InputManager - not needed for emulator

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
