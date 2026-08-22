#include "sim_window.h"

// Desktop has no system UI overlaying the window, so nothing is inset. iOS
// supplies its own version of this from sim/ios/sim_ios_platform.m.

extern "C" void sim_platform_safe_area(float* top, float* bottom, float* left, float* right) {
  if (top) *top = 0.0f;
  if (bottom) *bottom = 0.0f;
  if (left) *left = 0.0f;
  if (right) *right = 0.0f;
}

// Desktop keeps the working-directory-relative ./sdcard it has always used.
extern "C" const char* sim_platform_sdcard_root(void) { return nullptr; }
