#pragma once

#include <cstdint>

// Bridge between the SDL event pump (sim_display.cpp) and the HalGPIO
// implementation (sim_gpio.cpp).
//
// Touch coordinates crossing this boundary are in the panel's NATIVE frame:
// x over BoardConfig::ACTIVE.displayWidth, y over displayHeight, before any
// display-orientation rotation. That is the frame the GT911 driver reports in
// on hardware, so everything downstream -- normalization, GfxRenderer's
// tapToLogical, the app's hit rects -- behaves identically here.

void sim_gpio_pump_events();

// One contact, fed from mouse or finger events. move() is ignored unless a
// contact is down; up() is idempotent.
void sim_input_touch_down(int panelX, int panelY);
void sim_input_touch_move(int panelX, int panelY);
void sim_input_touch_up();

// Capacitive Home key below the panel (X4 Pro). Press and release edges; the
// tap/long-press split is classified from the hold duration, as the SDK does.
void sim_input_home_key(bool down);

// On-screen or keyboard button state. buttonIndex is a HalGPIO::BTN_* value.
// Presses on a button the active board leaves unassigned are dropped, so a
// simulated X4 Pro cannot report a Back or Confirm key it does not have.
void sim_input_set_button(uint8_t buttonIndex, bool down);

// True while a contact is down, for the on-screen press feedback.
bool sim_input_touch_is_down();
bool sim_input_home_key_is_down();
bool sim_input_button_is_down(uint8_t buttonIndex);
