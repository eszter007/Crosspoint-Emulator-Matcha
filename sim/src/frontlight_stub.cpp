// Host stand-in for the frontlight HAL. The emulated panel has no LEDs, so
// HalFrontlight::present() (inline in the header, backed by FrontlightManager)
// reports absent and every activity's frontlight row stays inert. The setters
// still record state so the control panel's slider and toggle behave, and
// main.cpp's begin() has something to call.

#include <HalFrontlight.h>

HalFrontlight HalFrontlight::instance;

void HalFrontlight::begin(uint8_t brightness, uint8_t warmth, bool on) {
  lastBrightness = brightness;
  lit = on;
  (void)warmth;
}

void HalFrontlight::setBrightness(uint8_t percent) { lastBrightness = percent; }
void HalFrontlight::setWarmth(uint8_t) {}
void HalFrontlight::setOn(bool on) { lit = on; }
