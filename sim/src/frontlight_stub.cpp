#include <BoardConfig.h>
#include <FrontlightManager.h>
#include <HalFrontlight.h>

#include "sim_window.h"

// Frontlight for the emulator.
//
// present() / hasColorTemperature() are inline in the SDK header and read the
// board profile alone, so a simulated X4 Pro reports its warm/cool frontlight
// and the firmware offers the same settings it does on hardware. Only the parts
// that would drive LEDC PWM channels are replaced here.
//
// The state is not merely recorded: sim_window tints the panel with it, so the
// brightness and warmth controls visibly do something. That is an emulator
// affordance rather than a physical model -- a real frontlight adds light to a
// reflective panel, which a monitor cannot reproduce.

HalFrontlight HalFrontlight::instance;

void FrontlightManager::begin() {}

void FrontlightManager::setBrightness(const uint8_t percent) {
  _brightness = percent > 100 ? 100 : percent;
  sim_window_set_frontlight(_brightness, _warmPercent);
}

void FrontlightManager::setBrightnessLevel(const uint8_t level) {
  _brightnessLevel = level;
  // The SDK maps an 8-bit level through a perceptual curve to duty; the
  // emulator only needs the rough percentage its tint is scaled by.
  setBrightness(static_cast<uint8_t>((static_cast<int>(level) * 100 + 127) / 255));
}

void FrontlightManager::off() { setBrightness(0); }

void FrontlightManager::on() { setBrightness(_brightness > 0 ? _brightness : 60); }

void FrontlightManager::setColorTemperature(const uint8_t warmPercent) {
  if (!hasColorTemperature()) return;
  _warmPercent = warmPercent > 100 ? 100 : warmPercent;
  sim_window_set_frontlight(_brightness, _warmPercent);
}

void HalFrontlight::begin(const uint8_t brightness, const uint8_t warmth, const bool on) {
  manager.begin();
  lastBrightness = brightness;
  lit = on;
  manager.setColorTemperature(warmth);
  manager.setBrightness(on ? brightness : 0);
}

void HalFrontlight::setBrightness(const uint8_t percent) {
  lastBrightness = percent;
  if (lit) manager.setBrightness(percent);
}

void HalFrontlight::setWarmth(const uint8_t warmPercent) { manager.setColorTemperature(warmPercent); }

void HalFrontlight::setOn(const bool on) {
  lit = on;
  manager.setBrightness(on ? lastBrightness : 0);
}
