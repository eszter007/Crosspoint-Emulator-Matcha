#pragma once

#include <cstdint>

// SDL window, panel presentation, and the on-screen bezel controls.
//
// Split from sim_display.cpp so the EInkDisplay/HalDisplay implementations stay
// about the panel, and everything that is really "the case the panel is mounted
// in" -- scaling, letterboxing, the simulated hardware keys, event routing --
// lives here. This is the layer that differs between a desktop window and a
// phone screen.

bool sim_window_init();
void sim_window_shutdown();

// Publish a framebuffer as the panel's new contents. `inverted` applies the
// display's output polarity (night mode), which on hardware the panel driver
// applies on the way out rather than in the framebuffer.
//
// Callable from any thread: the firmware renders on a task, and the emulator
// runs FreeRTOS tasks as real threads. These only convert into a staging buffer
// -- every SDL call happens on the main thread, in present()/service().
void sim_window_upload_bw(const uint8_t* bw, bool inverted);
void sim_window_upload_gray(const uint8_t* bw, const uint8_t* lsb, const uint8_t* msb, bool inverted);

// Draw the panel plus the bezel controls and swap. MAIN THREAD ONLY.
void sim_window_present();

// Present if a staged frame is waiting. Called once per main-loop pass, so a
// frame published by the render task reaches the screen even though that thread
// cannot draw. MAIN THREAD ONLY.
void sim_window_service();

// Frontlight state, as a 0-100 brightness and a 0-100 warm/cool mix (0 = fully
// cool, 100 = fully warm). Tints the panel so the firmware's brightness and
// warmth controls are visible. An emulator affordance, not a physical model:
// a real frontlight adds light to a reflective panel.
void sim_window_set_frontlight(int brightnessPercent, int warmPercent);

// Drain SDL events. Returns false when the user asked to quit.
bool sim_window_pump();

// Write the panel area (only -- not the bezel) to screenshots/ at exact panel
// resolution.
void sim_window_save_screenshot();

// Safe-area insets in points, for platforms that overlay system UI on the
// window (the iPhone's home indicator and sensor housing). Zero elsewhere:
// see sim/src/sim_platform_desktop.cpp and sim/ios/sim_ios_platform.m.
extern "C" void sim_platform_safe_area(float* top, float* bottom, float* left, float* right);

// Absolute path the SD card should be backed by, or NULL to use the emulator's
// default (./sdcard beside the binary). iOS returns the app's Documents
// directory, which is the only writable location it has and the one the Files
// app shows -- so dropping an EPUB in there is how a book gets "onto the card".
extern "C" const char* sim_platform_sdcard_root(void);
