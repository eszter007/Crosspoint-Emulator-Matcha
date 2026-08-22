#include "HalGPIO.h"

#include <BoardConfig.h>
#include <SDL.h>

#include <cstdint>
#include <cstdlib>

#include "sim_gpio.h"

// Buttons and touch for the emulator.
//
// Buttons are read from the board profile the same way the SDK's InputManager
// does: a key only reports if BoardConfig::ACTIVE.input assigns it a pin. A
// simulated X4 Pro therefore has no Back or Confirm key -- exactly like the
// hardware, where both come from the GT911 (touch plus the capacitive Home
// key) -- so a UI that is unreachable on the device is unreachable here too.
//
// Touch reimplements the SDK's single-contact classifier (tap / swipe /
// long-press / hold) against the same thresholds, so a gesture that registers
// in the emulator registers on hardware. Multi-contact gestures are not
// simulated: HalGPIO exposes none of them.

namespace {

// Thresholds mirrored from InputManager (freeink-sdk InputManager.h). Kept as
// their own constants rather than reaching into the SDK class because those are
// private, and the emulator has no InputManager instance to read them from.
constexpr int TOUCH_TAP_SLOP_PX = 28;
constexpr int TOUCH_SWIPE_MIN_PX = 60;
constexpr int TOUCH_TAP_RELEASE_SLOP_PX = TOUCH_SWIPE_MIN_PX - 1;
constexpr unsigned long TOUCH_SWIPE_MAX_MS = 700;
constexpr unsigned long TOUCH_LONG_PRESS_MS = 500;
constexpr unsigned long TOUCH_IRQ_PULSE_MS = 120;
constexpr unsigned long HOME_KEY_LONG_PRESS_MS = 700;

int absInt(int v) { return v < 0 ? -v : v; }

// --- buttons ---------------------------------------------------------------

uint8_t g_lastState = 0;
uint8_t g_prevState = 0;
// Set by the SDL pump; sampled into g_lastState by update(). On-screen buttons
// have no keyboard state to re-read, so their level lives here.
uint8_t g_buttonLevel = 0;
// Buttons pressed since the last update() that have not been reported yet.
// SDL can deliver a press and its release in one batch -- a quick tap on a
// bezel key does exactly that -- and a level-only view would net to zero and
// lose the press entirely. Held down, these bits change nothing: the level is
// already set. Physical keys do not need this, since SDL_GetKeyboardState
// reports a real key as held across many frames.
uint8_t g_buttonLatched = 0;
bool g_anyPressed = false;
bool g_anyReleased = false;
unsigned long g_pressStartMs = 0;
unsigned long g_powerPressStartMs = 0;

// Pin assignment for a logical button in the active profile, or -1.
int8_t buttonPin(uint8_t buttonIndex) {
  const auto& in = BoardConfig::ACTIVE.input;
  switch (buttonIndex) {
    case HalGPIO::BTN_BACK: return in.back;
    case HalGPIO::BTN_CONFIRM: return in.confirm;
    case HalGPIO::BTN_LEFT: return in.left;
    case HalGPIO::BTN_RIGHT: return in.right;
    case HalGPIO::BTN_UP: return in.up;
    case HalGPIO::BTN_DOWN: return in.down;
    case HalGPIO::BTN_POWER: return in.power;
    default: return -1;
  }
}

bool boardHasButton(uint8_t buttonIndex) { return buttonPin(buttonIndex) >= 0; }

uint8_t readButtonState() {
  const Uint8* keys = SDL_GetKeyboardState(nullptr);
  uint8_t state = static_cast<uint8_t>(g_buttonLevel | g_buttonLatched);
  if (keys[SDL_SCANCODE_LEFT]) state |= (1 << HalGPIO::BTN_LEFT);
  if (keys[SDL_SCANCODE_RIGHT]) state |= (1 << HalGPIO::BTN_RIGHT);
  if (keys[SDL_SCANCODE_UP]) state |= (1 << HalGPIO::BTN_UP);
  if (keys[SDL_SCANCODE_DOWN]) state |= (1 << HalGPIO::BTN_DOWN);
  if (keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER]) state |= (1 << HalGPIO::BTN_CONFIRM);
  if (keys[SDL_SCANCODE_BACKSPACE] || keys[SDL_SCANCODE_ESCAPE]) state |= (1 << HalGPIO::BTN_BACK);
  if (keys[SDL_SCANCODE_P]) state |= (1 << HalGPIO::BTN_POWER);

  // Drop anything the active board does not wire, matching
  // InputManager::getDigitalState(), which reads a pin per button and skips the
  // unassigned ones.
  uint8_t masked = 0;
  for (uint8_t i = 0; i <= HalGPIO::BTN_POWER; ++i) {
    if ((state & (1u << i)) && boardHasButton(i)) masked |= static_cast<uint8_t>(1u << i);
  }
  return masked;
}

// --- touch -----------------------------------------------------------------

struct TouchSample {
  int x = 0;
  int y = 0;
  unsigned long timestamp = 0;
};

struct TouchState {
  bool pressed = false;
  bool pressedEvent = false;
  bool releasedEvent = false;
  TouchSample downPoint;
  TouchSample upPoint;  // latest sample while down; last sample at release
  unsigned long lastHeldDurationMs = 0;
  bool movedBeyondTapSlop = false;
  bool movedBeyondTapReleaseSlop = false;
  bool longPressEvent = false;
  bool longPressFired = false;
  bool suppressed = false;

  bool homeKeyDown = false;
  bool homeKeyTapEvent = false;
  bool homeKeyLongEvent = false;
  bool homeKeyLongFired = false;
  unsigned long homeKeyDownAt = 0;
};

// Live state, written by the SDL pump between frames.
TouchState g_live;
// Frame-stable snapshot the firmware reads. update() moves the accumulated
// edges here and clears them from g_live, so an event raised mid-frame is seen
// exactly once, on the next frame -- the role serviceTouch() plays on hardware.
TouchState g_frame;

void normalizePoint(int x, int y, float& nx, float& ny) {
  const auto& t = BoardConfig::ACTIVE.touch;
  const uint16_t w = (t.rawMaxX > t.rawMinX) ? static_cast<uint16_t>(t.rawMaxX - t.rawMinX) : 1;
  const uint16_t h = (t.rawMaxY > t.rawMinY) ? static_cast<uint16_t>(t.rawMaxY - t.rawMinY) : 1;
  const auto clamp01 = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
  nx = clamp01(static_cast<float>(x) / w);
  ny = clamp01(static_cast<float>(y) / h);
}

}  // namespace

HalGPIO gpio;

void HalGPIO::begin() {}

void HalGPIO::update() {
  g_prevState = g_lastState;
  g_lastState = readButtonState();
  // Reported once; a button still held stays set through its level bit, and one
  // released in the same batch now falls away, giving a clean release edge on
  // the next pass.
  g_buttonLatched = 0;

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

  // Long-press fires while the finger is still down, so it is classified here
  // rather than at release -- same placement as the SDK's serviceTouch().
  const unsigned long now = millis();
  if (g_live.pressed && !g_live.movedBeyondTapSlop && !g_live.longPressFired && !g_live.suppressed &&
      now - g_live.downPoint.timestamp >= TOUCH_LONG_PRESS_MS) {
    g_live.longPressFired = true;
    g_live.longPressEvent = true;
  }
  if (g_live.homeKeyDown && !g_live.homeKeyLongFired && now - g_live.homeKeyDownAt >= HOME_KEY_LONG_PRESS_MS) {
    g_live.homeKeyLongFired = true;
    g_live.homeKeyLongEvent = true;
  }

  g_frame = g_live;

  // One-shot events are consumed by this frame.
  g_live.pressedEvent = false;
  g_live.releasedEvent = false;
  g_live.longPressEvent = false;
  g_live.homeKeyTapEvent = false;
  g_live.homeKeyLongEvent = false;
  // suppressTouchContact() holds through the release-edge frame and clears once
  // the contact is over, as it does on hardware.
  if (!g_live.pressed) g_live.suppressed = false;

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
bool HalGPIO::anyButtonDownRaw() { return readButtonState() != 0; }

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

bool HalGPIO::hasTouch() const { return BoardConfig::hasTouch(); }
bool HalGPIO::hasHomeKey() const { return BoardConfig::hasHomeKey(); }
bool HalGPIO::wasHomeKeyTapped() const { return g_frame.homeKeyTapEvent; }
bool HalGPIO::wasHomeKeyLongPressed() const { return g_frame.homeKeyLongEvent; }

bool HalGPIO::wasTouchTap(float& nx, float& ny) const {
  if (!g_frame.releasedEvent || g_frame.suppressed) return false;
  // A released contact stays a tap until motion reaches the swipe distance --
  // the wider release slop, not the stationary one.
  if (g_frame.movedBeyondTapReleaseSlop) return false;
  // Route to the touch-down point, not the release point: a finger rolls as it
  // lifts, and small targets need the position the user aimed at.
  normalizePoint(g_frame.downPoint.x, g_frame.downPoint.y, nx, ny);
  return true;
}

bool HalGPIO::wasTouchDown(float& nx, float& ny) const {
  if (!g_frame.pressedEvent || g_frame.suppressed) return false;
  normalizePoint(g_frame.downPoint.x, g_frame.downPoint.y, nx, ny);
  return true;
}

bool HalGPIO::wasTouchReleased() const { return g_frame.releasedEvent; }

bool HalGPIO::isTouchTapCandidate(float& nx, float& ny, unsigned long& heldMs) const {
  if (!g_frame.pressed || g_frame.movedBeyondTapSlop || g_frame.suppressed) return false;
  normalizePoint(g_frame.downPoint.x, g_frame.downPoint.y, nx, ny);
  heldMs = millis() - g_frame.downPoint.timestamp;
  return true;
}

bool HalGPIO::isTouchHeldAt(float& nx, float& ny) const {
  // Live drag tracking: latest sample, no tap-slop gate.
  if (!g_frame.pressed || g_frame.suppressed) return false;
  normalizePoint(g_frame.upPoint.x, g_frame.upPoint.y, nx, ny);
  return true;
}

bool HalGPIO::wasTouchLongPress(float& nx, float& ny) const {
  if (!g_frame.longPressEvent) return false;
  normalizePoint(g_frame.downPoint.x, g_frame.downPoint.y, nx, ny);
  return true;
}

void HalGPIO::suppressTouchContact() {
  if (g_live.pressed || g_live.releasedEvent) g_live.suppressed = true;
  g_frame.suppressed = true;
}

unsigned long HalGPIO::lastTouchHeldMs() const { return g_frame.lastHeldDurationMs; }

bool HalGPIO::wasSwipe(float& nxStart, float& nyStart, float& nxEnd, float& nyEnd) const {
  if (!g_frame.releasedEvent || g_frame.suppressed) return false;
  if (g_frame.lastHeldDurationMs > TOUCH_SWIPE_MAX_MS) return false;
  const int dx = g_frame.upPoint.x - g_frame.downPoint.x;
  const int dy = g_frame.upPoint.y - g_frame.downPoint.y;
  if (absInt(dx) < TOUCH_SWIPE_MIN_PX && absInt(dy) < TOUCH_SWIPE_MIN_PX) return false;
  normalizePoint(g_frame.downPoint.x, g_frame.downPoint.y, nxStart, nyStart);
  normalizePoint(g_frame.upPoint.x, g_frame.upPoint.y, nxEnd, nyEnd);
  return true;
}

bool HalGPIO::wasTouchActivity() const { return g_frame.pressedEvent || g_frame.releasedEvent; }

void HalGPIO::setSharedConfirmPowerShortPressEmitsPower(bool) {}

// Both predicates are pure BoardConfig lookups with no hardware in them, but
// they live in lib/hal/HalGPIO.cpp, which the emulator excludes wholesale.
// Copied verbatim from there so a divergence is a visible edit rather than a
// silent behaviour difference.
bool HalGPIO::isXteinkDevice() const {
  return BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX3 ||
         BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX3Uc8279 ||
         BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX4;
}

bool HalGPIO::hasEdgeSideButtons() const {
  return BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX3 ||
         BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX3Uc8279 ||
         BoardConfig::ACTIVE.board == BoardConfig::Board::XteinkX4Pro;
}

bool HalGPIO::verifyPowerButtonWakeup(uint16_t requiredDurationMs, bool shortPressAllowed) {
  (void)requiredDurationMs;
  (void)shortPressAllowed;
  return true;
}

bool HalGPIO::isUsbConnected() const { return true; }

bool HalGPIO::wasUsbStateChanged() const { return usbStateChanged; }

HalGPIO::WakeupReason HalGPIO::getWakeupReason() const { return WakeupReason::PowerButton; }

// --- input feed ------------------------------------------------------------

void sim_input_touch_down(int panelX, int panelY) {
  if (!BoardConfig::hasTouch()) return;
  const unsigned long now = millis();
  g_live.pressed = true;
  g_live.pressedEvent = true;
  g_live.downPoint = {panelX, panelY, now};
  g_live.upPoint = g_live.downPoint;
  g_live.movedBeyondTapSlop = false;
  g_live.movedBeyondTapReleaseSlop = false;
  g_live.longPressFired = false;
  g_live.longPressEvent = false;
  g_live.suppressed = false;
}

void sim_input_touch_move(int panelX, int panelY) {
  if (!g_live.pressed) return;
  g_live.upPoint = {panelX, panelY, millis()};
  const int dx = g_live.upPoint.x - g_live.downPoint.x;
  const int dy = g_live.upPoint.y - g_live.downPoint.y;
  if (absInt(dx) > TOUCH_TAP_SLOP_PX || absInt(dy) > TOUCH_TAP_SLOP_PX) g_live.movedBeyondTapSlop = true;
  if (absInt(dx) > TOUCH_TAP_RELEASE_SLOP_PX || absInt(dy) > TOUCH_TAP_RELEASE_SLOP_PX) {
    g_live.movedBeyondTapReleaseSlop = true;
  }
}

void sim_input_touch_up() {
  if (!g_live.pressed) return;
  g_live.pressed = false;
  g_live.releasedEvent = true;
  // Hardware only notices the lift on the next I2C poll, so its measured
  // contact duration always carries TOUCH_IRQ_PULSE_MS of hold-over -- which is
  // what the swipe time window is really budgeted against. SDL reports the
  // release exactly, so add the hold-over to the duration rather than to the
  // event: the swipe window matches the device without the emulator feeling
  // 120ms laggier than it.
  g_live.lastHeldDurationMs = (millis() - g_live.downPoint.timestamp) + TOUCH_IRQ_PULSE_MS;
}

void sim_input_home_key(bool down) {
  if (!BoardConfig::hasHomeKey()) return;
  if (down) {
    if (g_live.homeKeyDown) return;
    g_live.homeKeyDown = true;
    g_live.homeKeyDownAt = millis();
    g_live.homeKeyLongFired = false;
    return;
  }
  if (!g_live.homeKeyDown) return;
  g_live.homeKeyDown = false;
  // A hold that already fired long does not also tap.
  if (!g_live.homeKeyLongFired) g_live.homeKeyTapEvent = true;
}

void sim_input_set_button(uint8_t buttonIndex, bool down) {
  if (buttonIndex > HalGPIO::BTN_POWER) return;
  const uint8_t bit = static_cast<uint8_t>(1u << buttonIndex);
  if (down) {
    g_buttonLevel |= bit;
    g_buttonLatched |= bit;
  } else {
    g_buttonLevel &= static_cast<uint8_t>(~bit);
  }
}

bool sim_input_touch_is_down() { return g_live.pressed; }
bool sim_input_home_key_is_down() { return g_live.homeKeyDown; }
bool sim_input_button_is_down(uint8_t buttonIndex) {
  if (buttonIndex > HalGPIO::BTN_POWER) return false;
  // The on-screen level, not the reported state: this drives the key's pressed
  // look, which should follow the finger rather than the latch.
  return (g_buttonLevel & (1u << buttonIndex)) != 0;
}

void sim_gpio_pump_events() {
  SDL_Event e;
  while (SDL_PollEvent(&e)) {
    if (e.type == SDL_QUIT) std::exit(0);
  }
}
