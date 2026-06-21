#include "HalGPIO.h"

#include <SDL.h>
#include <cstdint>
#include <cstdlib>

namespace {
uint8_t g_lastState = 0;
uint8_t g_prevState = 0;
bool g_anyPressed = false;
bool g_anyReleased = false;
unsigned long g_pressStartMs = 0;
unsigned long g_powerPressStartMs = 0;

uint8_t readButtonState() {
  const Uint8* keys = SDL_GetKeyboardState(nullptr);
  uint8_t state = 0;
  if (keys[SDL_SCANCODE_LEFT]) state |= (1 << HalGPIO::BTN_LEFT);
  if (keys[SDL_SCANCODE_RIGHT]) state |= (1 << HalGPIO::BTN_RIGHT);
  if (keys[SDL_SCANCODE_UP]) state |= (1 << HalGPIO::BTN_UP);
  if (keys[SDL_SCANCODE_DOWN]) state |= (1 << HalGPIO::BTN_DOWN);
  if (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER]) state |= (1 << HalGPIO::BTN_CONFIRM);
  if (keys[SDL_SCANCODE_BACKSPACE] || keys[SDL_SCANCODE_ESCAPE]) state |= (1 << HalGPIO::BTN_BACK);
  if (keys[SDL_SCANCODE_P]) state |= (1 << HalGPIO::BTN_POWER);
  return state;
}
}  // namespace

HalGPIO gpio;

void HalGPIO::begin() {}

void HalGPIO::update() {
  g_prevState = g_lastState;
  g_lastState = readButtonState();

  g_anyPressed = false;
  g_anyReleased = false;
  for (int i = 0; i <= BTN_POWER; ++i) {
    const uint8_t bit = static_cast<uint8_t>(1u << i);
    if ((g_lastState & bit) && !(g_prevState & bit)) g_anyPressed = true;
    if (!(g_lastState & bit) && (g_prevState & bit)) g_anyReleased = true;
  }

  const bool confirmHeld = (g_lastState & (1 << BTN_CONFIRM)) != 0;
  if (confirmHeld) {
    if (g_pressStartMs == 0) g_pressStartMs = millis();
  } else {
    g_pressStartMs = 0;
  }

  const bool powerHeld = (g_lastState & (1 << BTN_POWER)) != 0;
  if (powerHeld) {
    if (g_powerPressStartMs == 0) g_powerPressStartMs = millis();
  } else {
    g_powerPressStartMs = 0;
  }

  const bool usbNow = isUsbConnected();
  usbStateChanged = (usbNow != lastUsbConnected);
  lastUsbConnected = usbNow;
}

bool HalGPIO::isPressed(uint8_t buttonIndex) const {
  if (buttonIndex > BTN_POWER) return false;
  return (g_lastState & (1 << buttonIndex)) != 0;
}

bool HalGPIO::wasPressed(uint8_t buttonIndex) const {
  if (buttonIndex > BTN_POWER) return false;
  return (g_lastState & (1 << buttonIndex)) != 0 && (g_prevState & (1 << buttonIndex)) == 0;
}

bool HalGPIO::wasAnyPressed() const { return g_anyPressed; }

bool HalGPIO::wasReleased(uint8_t buttonIndex) const {
  if (buttonIndex > BTN_POWER) return false;
  return (g_lastState & (1 << buttonIndex)) == 0 && (g_prevState & (1 << buttonIndex)) != 0;
}

bool HalGPIO::wasAnyReleased() const { return g_anyReleased; }

unsigned long HalGPIO::getHeldTime() const {
  if (g_pressStartMs == 0) return 0;
  return millis() - g_pressStartMs;
}

unsigned long HalGPIO::getPowerButtonHeldTime() const {
  if (g_powerPressStartMs == 0) return 0;
  return millis() - g_powerPressStartMs;
}

void HalGPIO::startDeepSleep() {}

void HalGPIO::verifyPowerButtonWakeup(uint16_t requiredDurationMs, bool shortPressAllowed) {
  (void)requiredDurationMs;
  (void)shortPressAllowed;
}

bool HalGPIO::isUsbConnected() const { return true; }

bool HalGPIO::wasUsbStateChanged() const { return usbStateChanged; }

HalGPIO::WakeupReason HalGPIO::getWakeupReason() const { return WakeupReason::PowerButton; }

void sim_gpio_pump_events() {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) std::exit(0);
  }
}
