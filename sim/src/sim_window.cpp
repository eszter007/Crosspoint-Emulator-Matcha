#include "sim_window.h"

#include <BoardConfig.h>
#include <EInkDisplay.h>
#include <HalGPIO.h>
#include <SDL.h>
#include <sys/stat.h>

#include <algorithm>
#include <cmath>
#include <mutex>
#include <thread>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>

#include "sim_gpio.h"

// The panel texture is the display rotated a quarter turn: the 800x480 panel is
// read out into a portrait 480x800 image, which is how the device is held.
static constexpr int PANEL_W = static_cast<int>(EInkDisplay::DISPLAY_WIDTH);   // 800, native long axis
static constexpr int PANEL_H = static_cast<int>(EInkDisplay::DISPLAY_HEIGHT);  // 480, native short axis
static constexpr int TEX_W = PANEL_H;                                          // 480
static constexpr int TEX_H = PANEL_W;                                          // 800

namespace {

SDL_Window* g_window = nullptr;
SDL_Renderer* g_renderer = nullptr;
SDL_Texture* g_texture = nullptr;
bool g_inBackground = false;

// --- bezel controls --------------------------------------------------------

// A simulated hardware key. Which ones exist is read from the board profile, so
// an X4 Pro shows its two page keys, Power and the capacitive Home key, while an
// X4 shows its full front-button set.
enum class Glyph { ChevronUp, ChevronDown, ChevronLeft, ChevronRight, Confirm, Back, Power, Home };

// HOME_KEY is not a HalGPIO button: the capacitive Home key is reported by the
// touch controller, so it is fed through sim_input_home_key() instead.
constexpr uint8_t HOME_KEY = 0xFF;

struct Control {
  uint8_t button;  // HalGPIO::BTN_* or HOME_KEY
  Glyph glyph;
  SDL_Rect rect{};
};

Control g_controls[8];
int g_controlCount = 0;
// Which control the current press landed on, so a finger sliding off does not
// leak into a neighbour. -2 = none, -1 = the panel, >= 0 = index in g_controls.
int g_capturedControl = -2;

bool g_controlsEnabled = false;

SDL_Rect g_panelRect{};

int g_frontlightBrightness = 0;
int g_frontlightWarm = 50;

// Staging buffer between the render task and the main thread. The firmware
// renders on a FreeRTOS task, which the emulator runs as a real thread, and SDL
// draws only from the thread that made the renderer -- a present from the task
// silently never reached the window, leaving the screen an interaction behind.
// The panel conversion happens on whichever thread published the frame; the
// upload and the draw happen on the main thread.
std::mutex g_stagingMutex;
std::vector<uint8_t> g_staging;  // RGB24, TEX_W * TEX_H
bool g_stagingDirty = false;
std::thread::id g_mainThread;

bool onMainThread() { return std::this_thread::get_id() == g_mainThread; }

bool boardHasButton(uint8_t buttonIndex) {
  const auto& in = BoardConfig::ACTIVE.input;
  switch (buttonIndex) {
    case HalGPIO::BTN_BACK: return in.back >= 0;
    case HalGPIO::BTN_CONFIRM: return in.confirm >= 0;
    case HalGPIO::BTN_LEFT: return in.left >= 0;
    case HalGPIO::BTN_RIGHT: return in.right >= 0;
    case HalGPIO::BTN_UP: return in.up >= 0;
    case HalGPIO::BTN_DOWN: return in.down >= 0;
    case HalGPIO::BTN_POWER: return in.power >= 0;
    default: return false;
  }
}

void addControl(uint8_t button, Glyph glyph) {
  if (g_controlCount >= static_cast<int>(sizeof(g_controls) / sizeof(g_controls[0]))) return;
  g_controls[g_controlCount].button = button;
  g_controls[g_controlCount].glyph = glyph;
  g_controlCount++;
}

// Build the control strip from the active board profile. Order runs left to
// right in the order a thumb reaches them: page keys outermost, Home centre.
void buildControls() {
  g_controlCount = 0;
  if (boardHasButton(HalGPIO::BTN_UP)) addControl(HalGPIO::BTN_UP, Glyph::ChevronUp);
  if (boardHasButton(HalGPIO::BTN_LEFT)) addControl(HalGPIO::BTN_LEFT, Glyph::ChevronLeft);
  if (boardHasButton(HalGPIO::BTN_BACK)) addControl(HalGPIO::BTN_BACK, Glyph::Back);
  if (BoardConfig::hasHomeKey()) addControl(HOME_KEY, Glyph::Home);
  if (boardHasButton(HalGPIO::BTN_CONFIRM)) addControl(HalGPIO::BTN_CONFIRM, Glyph::Confirm);
  if (boardHasButton(HalGPIO::BTN_POWER)) addControl(HalGPIO::BTN_POWER, Glyph::Power);
  if (boardHasButton(HalGPIO::BTN_RIGHT)) addControl(HalGPIO::BTN_RIGHT, Glyph::ChevronRight);
  if (boardHasButton(HalGPIO::BTN_DOWN)) addControl(HalGPIO::BTN_DOWN, Glyph::ChevronDown);
}

// --- layout ----------------------------------------------------------------

// Recomputed on every resize and orientation change. The panel keeps its aspect
// ratio and is centred in whatever is left after the control strip and the
// platform's safe area.
void layout() {
  int outW = 0, outH = 0;
  SDL_GetRendererOutputSize(g_renderer, &outW, &outH);
  if (outW <= 0 || outH <= 0) return;

  // Safe-area insets arrive in points; the renderer works in pixels.
  int winW = 0, winH = 0;
  SDL_GetWindowSize(g_window, &winW, &winH);
  const float pxPerPt = (winW > 0) ? static_cast<float>(outW) / static_cast<float>(winW) : 1.0f;
  float saTop = 0, saBottom = 0, saLeft = 0, saRight = 0;
  sim_platform_safe_area(&saTop, &saBottom, &saLeft, &saRight);

  const int insetTop = static_cast<int>(saTop * pxPerPt);
  const int insetBottom = static_cast<int>(saBottom * pxPerPt);
  const int insetLeft = static_cast<int>(saLeft * pxPerPt);
  const int insetRight = static_cast<int>(saRight * pxPerPt);

  const int availX = insetLeft;
  const int availY = insetTop;
  const int availW = std::max(1, outW - insetLeft - insetRight);
  const int availH = std::max(1, outH - insetTop - insetBottom);

  int stripH = 0;
  if (g_controlsEnabled && g_controlCount > 0) {
    // Tall enough to hit with a thumb, but never so tall it starves the panel.
    stripH = std::min(availH / 4, std::max(72, availH / 9));
  }

  const int panelAreaH = std::max(1, availH - stripH);
  const double scale =
      std::min(static_cast<double>(availW) / TEX_W, static_cast<double>(panelAreaH) / TEX_H);
  const int pw = std::max(1, static_cast<int>(TEX_W * scale));
  const int ph = std::max(1, static_cast<int>(TEX_H * scale));
  g_panelRect = {availX + (availW - pw) / 2, availY + (panelAreaH - ph) / 2, pw, ph};

  if (stripH == 0) return;

  const int stripY = availY + availH - stripH;
  const int slotW = availW / g_controlCount;
  const int btnSize = std::min(slotW - 12, stripH - 20);
  for (int i = 0; i < g_controlCount; i++) {
    const int cx = availX + slotW * i + slotW / 2;
    const int cy = stripY + stripH / 2;
    g_controls[i].rect = {cx - btnSize / 2, cy - btnSize / 2, btnSize, btnSize};
  }
}

// --- glyph drawing ---------------------------------------------------------

void drawThickLine(int x1, int y1, int x2, int y2, int thickness) {
  const double dx = x2 - x1;
  const double dy = y2 - y1;
  const double len = std::sqrt(dx * dx + dy * dy);
  if (len < 0.5) return;
  // Offset the line along its own normal to give it width; enough passes to
  // leave no gaps at this scale.
  const double nx = -dy / len;
  const double ny = dx / len;
  for (int t = -thickness / 2; t <= thickness / 2; t++) {
    const int ox = static_cast<int>(std::lround(nx * t));
    const int oy = static_cast<int>(std::lround(ny * t));
    SDL_RenderDrawLine(g_renderer, x1 + ox, y1 + oy, x2 + ox, y2 + oy);
  }
}

void drawArc(int cx, int cy, int radius, double fromDeg, double toDeg, int thickness) {
  // Enough angular steps that neighbouring points land on adjacent pixels at
  // the outer radius, and half-pixel radial steps so the stroke has no holes:
  // a coarser sweep drew the Home ring as a dotted circle.
  const int steps = std::max(48, static_cast<int>((radius + thickness) * 8));
  const double halfThickness = thickness / 2.0;
  for (double t = -halfThickness; t <= halfThickness; t += 0.5) {
    const double r = radius + t;
    if (r <= 0) continue;
    for (int i = 0; i <= steps; i++) {
      const double a = (fromDeg + (toDeg - fromDeg) * i / steps) * M_PI / 180.0;
      SDL_RenderDrawPoint(g_renderer, cx + static_cast<int>(std::lround(std::cos(a) * r)),
                          cy + static_cast<int>(std::lround(std::sin(a) * r)));
    }
  }
}

void drawDisc(int cx, int cy, int radius) {
  for (int dy = -radius; dy <= radius; dy++) {
    const int span = static_cast<int>(std::lround(std::sqrt(static_cast<double>(radius * radius - dy * dy))));
    SDL_RenderDrawLine(g_renderer, cx - span, cy + dy, cx + span, cy + dy);
  }
}

void drawGlyph(Glyph glyph, const SDL_Rect& r) {
  const int cx = r.x + r.w / 2;
  const int cy = r.y + r.h / 2;
  const int s = std::max(4, r.w / 4);          // glyph half-extent
  const int th = std::max(2, r.w / 14);        // stroke thickness

  switch (glyph) {
    case Glyph::ChevronUp:
      drawThickLine(cx - s, cy + s / 2, cx, cy - s / 2, th);
      drawThickLine(cx, cy - s / 2, cx + s, cy + s / 2, th);
      break;
    case Glyph::ChevronDown:
      drawThickLine(cx - s, cy - s / 2, cx, cy + s / 2, th);
      drawThickLine(cx, cy + s / 2, cx + s, cy - s / 2, th);
      break;
    case Glyph::ChevronLeft:
      drawThickLine(cx + s / 2, cy - s, cx - s / 2, cy, th);
      drawThickLine(cx - s / 2, cy, cx + s / 2, cy + s, th);
      break;
    case Glyph::ChevronRight:
      drawThickLine(cx - s / 2, cy - s, cx + s / 2, cy, th);
      drawThickLine(cx + s / 2, cy, cx - s / 2, cy + s, th);
      break;
    case Glyph::Confirm:
      // Tick.
      drawThickLine(cx - s, cy, cx - s / 3, cy + s * 2 / 3, th);
      drawThickLine(cx - s / 3, cy + s * 2 / 3, cx + s, cy - s * 2 / 3, th);
      break;
    case Glyph::Back:
      // Left arrow: head plus shaft, so it reads differently from a plain
      // chevron (the X4 has both a Back key and a Left key).
      drawThickLine(cx - s, cy, cx + s, cy, th);
      drawThickLine(cx - s, cy, cx - s / 4, cy - s * 3 / 4, th);
      drawThickLine(cx - s, cy, cx - s / 4, cy + s * 3 / 4, th);
      break;
    case Glyph::Power:
      // Ring with a gap at the top, plus the stem through it.
      drawArc(cx, cy, s, -60, 240, th);
      drawThickLine(cx, cy - s - th, cx, cy - s / 3, th);
      break;
    case Glyph::Home:
      // Hollow ring, matching the capacitive key printed on the bezel.
      drawArc(cx, cy, s, 0, 360, th);
      break;
  }
}

void drawControls() {
  if (!g_controlsEnabled) return;
  for (int i = 0; i < g_controlCount; i++) {
    const Control& c = g_controls[i];
    const bool down = (c.button == HOME_KEY) ? sim_input_home_key_is_down() : sim_input_button_is_down(c.button);

    // Pressed keys fill; idle keys are outlined. Both read clearly against the
    // dark bezel without needing a font.
    if (down) {
      SDL_SetRenderDrawColor(g_renderer, 90, 90, 96, 255);
      SDL_RenderFillRect(g_renderer, &c.rect);
      SDL_SetRenderDrawColor(g_renderer, 250, 250, 250, 255);
    } else {
      SDL_SetRenderDrawColor(g_renderer, 70, 70, 76, 255);
      SDL_RenderDrawRect(g_renderer, &c.rect);
      SDL_SetRenderDrawColor(g_renderer, 175, 175, 182, 255);
    }
    drawGlyph(c.glyph, c.rect);
  }
}

// --- texture fill ----------------------------------------------------------

// Write one panel pixel into the staging image, rotating the panel's landscape
// frame into the portrait image. Kept as one place so the two fill paths and
// the touch mapping below cannot drift apart.
inline void putPanelPixel(uint8_t* pixels, int panelX, int panelY, uint8_t v) {
  const int texX = PANEL_H - 1 - panelY;
  const int texY = panelX;
  const size_t off = (static_cast<size_t>(texY) * TEX_W + static_cast<size_t>(texX)) * 3;
  pixels[off + 0] = v;
  pixels[off + 1] = v;
  pixels[off + 2] = v;
}

}  // namespace

void sim_window_upload_bw(const uint8_t* buf, bool inverted) {
  if (!buf) return;
  std::lock_guard<std::mutex> lock(g_stagingMutex);
  g_staging.resize(static_cast<size_t>(TEX_W) * TEX_H * 3);
  uint8_t* pixels = g_staging.data();

  const int WB = static_cast<int>(EInkDisplay::DISPLAY_WIDTH_BYTES);
  for (int y = 0; y < PANEL_H; y++) {
    const size_t rowBase = static_cast<size_t>(y) * WB;
    for (int byteIdx = 0; byteIdx < WB; byteIdx++) {
      const uint8_t byte = buf[rowBase + byteIdx];
      const int xBase = byteIdx * 8;
      const int count = std::min(8, PANEL_W - xBase);
      for (int b = 0; b < count; b++) {
        uint8_t v = (byte & (0x80 >> b)) ? 255 : 0;
        if (inverted) v = static_cast<uint8_t>(255 - v);
        putPanelPixel(pixels, xBase + b, y, v);
      }
    }
  }
  g_stagingDirty = true;
}

void sim_window_upload_gray(const uint8_t* bw, const uint8_t* lsb, const uint8_t* msb, bool inverted) {
  if (!bw || !lsb || !msb) return;
  std::lock_guard<std::mutex> lock(g_stagingMutex);
  g_staging.resize(static_cast<size_t>(TEX_W) * TEX_H * 3);
  uint8_t* pixels = g_staging.data();

  static constexpr uint8_t grayLut[8] = {0, 0, 170, 85, 255, 255, 255, 255};
  const int WB = static_cast<int>(EInkDisplay::DISPLAY_WIDTH_BYTES);
  for (int y = 0; y < PANEL_H; y++) {
    const size_t rowBase = static_cast<size_t>(y) * WB;
    for (int byteIdx = 0; byteIdx < WB; byteIdx++) {
      const uint8_t bwByte = bw[rowBase + byteIdx];
      const uint8_t lsbByte = lsb[rowBase + byteIdx];
      const uint8_t msbByte = msb[rowBase + byteIdx];
      const int xBase = byteIdx * 8;
      const int count = std::min(8, PANEL_W - xBase);
      for (int b = 0; b < count; b++) {
        const uint8_t mask = 0x80 >> b;
        const int lutIdx = ((bwByte & mask) ? 4 : 0) | ((msbByte & mask) ? 2 : 0) | ((lsbByte & mask) ? 1 : 0);
        uint8_t v = grayLut[lutIdx];
        if (inverted) v = static_cast<uint8_t>(255 - v);
        putPanelPixel(pixels, xBase + b, y, v);
      }
    }
  }
  g_stagingDirty = true;
}

void sim_window_set_frontlight(int brightnessPercent, int warmPercent) {
  g_frontlightBrightness = std::min(100, std::max(0, brightnessPercent));
  g_frontlightWarm = std::min(100, std::max(0, warmPercent));
  // The tint is applied at draw time, so the panel has to be redrawn for a
  // change to show. Marking the frame dirty gets that done on the main thread,
  // wherever the firmware happened to change the light from.
  {
    std::lock_guard<std::mutex> lock(g_stagingMutex);
    g_stagingDirty = true;
  }
  sim_window_present();
}

namespace {
// Colour modulation for the panel texture. Modulation can only subtract, so an
// unlit panel is left untouched and a lit one pulls the channels the light is
// short of: warm light loses blue, cool light loses red.
void applyFrontlightTint() {
  if (g_frontlightBrightness <= 0) {
    SDL_SetTextureColorMod(g_texture, 255, 255, 255);
    return;
  }
  // Full strength costs the opposing channel ~12%, which reads as a tint
  // without making the page look discoloured.
  const double strength = g_frontlightBrightness / 100.0;
  const double warmth = (g_frontlightWarm - 50) / 50.0;  // -1 cool .. +1 warm
  const int shift = static_cast<int>(std::lround(std::abs(warmth) * strength * 30.0));
  const Uint8 r = static_cast<Uint8>(warmth < 0 ? 255 - shift : 255);
  const Uint8 b = static_cast<Uint8>(warmth > 0 ? 255 - shift : 255);
  const Uint8 g = static_cast<Uint8>(255 - shift / 3);
  SDL_SetTextureColorMod(g_texture, r, g, b);
}
}  // namespace

void sim_window_service() {
  bool dirty = false;
  {
    std::lock_guard<std::mutex> lock(g_stagingMutex);
    dirty = g_stagingDirty;
  }
  if (dirty) sim_window_present();
}

void sim_window_present() {
  // A present from the render task cannot draw; the frame it staged is picked
  // up by sim_window_service() on the next main-loop pass instead.
  if (!onMainThread()) return;
  if (!g_renderer || !g_texture || g_inBackground) return;

  {
    std::lock_guard<std::mutex> lock(g_stagingMutex);
    if (g_stagingDirty && !g_staging.empty()) {
      SDL_UpdateTexture(g_texture, nullptr, g_staging.data(), TEX_W * 3);
      g_stagingDirty = false;
    }
  }

  SDL_SetRenderDrawColor(g_renderer, 24, 24, 27, 255);
  SDL_RenderClear(g_renderer);
  applyFrontlightTint();
  SDL_RenderCopy(g_renderer, g_texture, nullptr, &g_panelRect);
  drawControls();
  SDL_RenderPresent(g_renderer);
}

bool sim_window_init() {
  if (g_window) return true;
  g_mainThread = std::this_thread::get_id();
  if (SDL_Init(SDL_INIT_VIDEO) != 0) return false;

  // On iOS the panel is driven by fingers, and SDL's touch-to-mouse synthesis
  // gives one code path for both the phone and a desktop mouse. Set explicitly
  // rather than relying on the platform default.
  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
  SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");

  // The bezel keys are the only way to reach Power, the page keys, and the Home
  // key on a device with no keyboard, so they are always up on iOS. On desktop
  // they are opt-in: the keyboard already covers every key, and the window
  // stays exactly panel-sized for screenshots.
#if defined(SIM_PLATFORM_IOS)
  g_controlsEnabled = true;
#else
  g_controlsEnabled = (std::getenv("CROSSPOINT_EMU_CONTROLS") != nullptr);
#endif
  buildControls();

  Uint32 windowFlags = SDL_WINDOW_SHOWN | SDL_WINDOW_ALLOW_HIGHDPI;
  int windowW = TEX_W;
  int windowH = TEX_H;
#if defined(SIM_PLATFORM_IOS)
  // Fill the screen; the layout centres the panel in whatever it gets.
  windowFlags |= SDL_WINDOW_FULLSCREEN | SDL_WINDOW_BORDERLESS;
  SDL_DisplayMode mode;
  if (SDL_GetCurrentDisplayMode(0, &mode) == 0) {
    windowW = mode.w;
    windowH = mode.h;
  }
#else
  windowFlags |= SDL_WINDOW_RESIZABLE;
  if (g_controlsEnabled) windowH += TEX_H / 8;
#endif

  g_window = SDL_CreateWindow("Crosspoint Emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, windowW,
                              windowH, windowFlags);
  if (!g_window) return false;

  // PRESENTVSYNC: without it, SDL_RenderPresent() can swap mid-scanout, showing
  // a torn frame -- a slice of the previous content at the top of the window
  // while the rest already shows the new one. Purely an emulator presentation
  // artifact (real e-ink transfers the buffer atomically over SPI).
  g_renderer = SDL_CreateRenderer(g_window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
  if (!g_renderer) return false;

  g_texture = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_STREAMING, TEX_W, TEX_H);
  if (!g_texture) return false;

  layout();
  return true;
}

void sim_window_shutdown() {
  if (g_texture) {
    SDL_DestroyTexture(g_texture);
    g_texture = nullptr;
  }
  if (g_renderer) {
    SDL_DestroyRenderer(g_renderer);
    g_renderer = nullptr;
  }
  if (g_window) {
    SDL_DestroyWindow(g_window);
    g_window = nullptr;
  }
  SDL_Quit();
}

namespace {

// Mouse coordinates are in window points; the renderer works in pixels, which
// differ on any high-DPI screen (every iPhone, and Retina Macs).
void windowToOutput(int mx, int my, int& ox, int& oy) {
  int winW = 0, winH = 0, outW = 0, outH = 0;
  SDL_GetWindowSize(g_window, &winW, &winH);
  SDL_GetRendererOutputSize(g_renderer, &outW, &outH);
  ox = (winW > 0) ? mx * outW / winW : mx;
  oy = (winH > 0) ? my * outH / winH : my;
}

bool outputToPanel(int ox, int oy, int& panelX, int& panelY) {
  if (g_panelRect.w <= 0 || g_panelRect.h <= 0) return false;
  if (ox < g_panelRect.x || ox >= g_panelRect.x + g_panelRect.w) return false;
  if (oy < g_panelRect.y || oy >= g_panelRect.y + g_panelRect.h) return false;
  const int texX = (ox - g_panelRect.x) * TEX_W / g_panelRect.w;
  const int texY = (oy - g_panelRect.y) * TEX_H / g_panelRect.h;
  // Inverse of putPanelPixel().
  panelX = std::min(PANEL_W - 1, std::max(0, texY));
  panelY = std::min(PANEL_H - 1, std::max(0, PANEL_H - 1 - texX));
  return true;
}

int controlAt(int ox, int oy) {
  if (!g_controlsEnabled) return -1;
  for (int i = 0; i < g_controlCount; i++) {
    const SDL_Rect& r = g_controls[i].rect;
    if (ox >= r.x && ox < r.x + r.w && oy >= r.y && oy < r.y + r.h) return i;
  }
  return -1;
}

void pressControl(int index, bool down) {
  if (index < 0 || index >= g_controlCount) return;
  if (g_controls[index].button == HOME_KEY) {
    sim_input_home_key(down);
  } else {
    sim_input_set_button(g_controls[index].button, down);
  }
}

void releaseCaptured() {
  if (g_capturedControl == -1) {
    sim_input_touch_up();
  } else if (g_capturedControl >= 0) {
    pressControl(g_capturedControl, false);
  }
  g_capturedControl = -2;
}

}  // namespace

bool sim_window_pump() {
  bool needsPresent = false;
  SDL_Event e;

  // Backgrounded on iOS: block instead of spinning the main loop at full tilt.
  // A background app burning CPU is one iOS terminates; the wait also parks the
  // firmware loop, which has nothing to do with no screen to draw to.
  if (g_inBackground) {
    SDL_WaitEventTimeout(nullptr, 200);
  }

  while (SDL_PollEvent(&e)) {
    switch (e.type) {
      case SDL_QUIT:
        return false;

      case SDL_WINDOWEVENT:
        if (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e.window.event == SDL_WINDOWEVENT_RESIZED ||
            e.window.event == SDL_WINDOWEVENT_EXPOSED) {
          layout();
          needsPresent = true;
        }
        break;

      // iOS lifecycle. Drawing after the app is backgrounded is a termination
      // offence on iOS, so presentation stops until it is frontmost again.
      case SDL_APP_WILLENTERBACKGROUND:
        g_inBackground = true;
        releaseCaptured();
        break;
      case SDL_APP_DIDENTERFOREGROUND:
        g_inBackground = false;
        layout();
        needsPresent = true;
        break;

      case SDL_KEYDOWN:
        // Buttons are read through SDL_GetKeyboardState, so consuming key
        // events here does not interfere with them.
        if (e.key.repeat == 0) {
          // Cmd+S (Ctrl+S on Linux) saves a screenshot; H taps the Home key.
          if (e.key.keysym.sym == SDLK_s && (e.key.keysym.mod & (KMOD_GUI | KMOD_CTRL))) {
            sim_window_save_screenshot();
          } else if (e.key.keysym.sym == SDLK_h) {
            sim_input_home_key(true);
          }
        }
        break;
      case SDL_KEYUP:
        if (e.key.keysym.sym == SDLK_h) sim_input_home_key(false);
        break;

      case SDL_MOUSEBUTTONDOWN: {
        if (e.button.button != SDL_BUTTON_LEFT) break;
        int ox = 0, oy = 0;
        windowToOutput(e.button.x, e.button.y, ox, oy);
        const int ctrl = controlAt(ox, oy);
        if (ctrl >= 0) {
          g_capturedControl = ctrl;
          pressControl(ctrl, true);
          needsPresent = true;
          break;
        }
        int px = 0, py = 0;
        if (outputToPanel(ox, oy, px, py)) {
          g_capturedControl = -1;
          sim_input_touch_down(px, py);
        }
        break;
      }

      case SDL_MOUSEMOTION: {
        if (g_capturedControl != -1) break;  // only a panel contact tracks motion
        int ox = 0, oy = 0;
        windowToOutput(e.motion.x, e.motion.y, ox, oy);
        int px = 0, py = 0;
        if (outputToPanel(ox, oy, px, py)) {
          sim_input_touch_move(px, py);
        }
        break;
      }

      case SDL_MOUSEBUTTONUP:
        if (e.button.button != SDL_BUTTON_LEFT) break;
        if (g_capturedControl >= 0) needsPresent = true;
        releaseCaptured();
        break;

      default:
        break;
    }
  }

  // Press feedback on the bezel keys is the emulator's own chrome, not
  // something the firmware redraws, so repaint here when it changes.
  if (needsPresent) sim_window_present();
  return true;
}

// Cmd+S (Ctrl+S on Linux) writes the panel to screenshots/ at its exact
// resolution -- unlike an OS window grab, which picks up the bezel, window
// chrome, and Retina scaling.
//
// BMP because that is all SDL2 writes without SDL_image. miniz is vendored but
// compiled with MINIZ_NO_DEFLATE_APIS, so its PNG writer is not built, and
// turning deflate on would grow the firmware for an emulator convenience.
// Convert afterwards, e.g.
//   sips -s format png screenshots/*.bmp --out docs/images/screenshots/
void sim_window_save_screenshot() {
  if (!g_renderer || !g_texture) return;

  // Render the panel alone, at 1:1, into an offscreen target: reading back the
  // window would capture the bezel and whatever scaling the window is at.
  SDL_Texture* target = SDL_CreateTexture(g_renderer, SDL_PIXELFORMAT_RGB24, SDL_TEXTUREACCESS_TARGET, TEX_W, TEX_H);
  if (!target) {
    printf("[SHOT] target alloc failed: %s\n", SDL_GetError());
    return;
  }
  SDL_Texture* previousTarget = SDL_GetRenderTarget(g_renderer);
  SDL_SetRenderTarget(g_renderer, target);
  SDL_RenderCopy(g_renderer, g_texture, nullptr, nullptr);

  SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, TEX_W, TEX_H, 24, SDL_PIXELFORMAT_RGB24);
  if (!surface) {
    printf("[SHOT] surface alloc failed: %s\n", SDL_GetError());
    SDL_SetRenderTarget(g_renderer, previousTarget);
    SDL_DestroyTexture(target);
    return;
  }
  const int rc = SDL_RenderReadPixels(g_renderer, nullptr, SDL_PIXELFORMAT_RGB24, surface->pixels, surface->pitch);
  SDL_SetRenderTarget(g_renderer, previousTarget);
  SDL_DestroyTexture(target);
  if (rc != 0) {
    printf("[SHOT] read failed: %s\n", SDL_GetError());
    SDL_FreeSurface(surface);
    return;
  }

  mkdir("screenshots", 0755);  // fails harmlessly when it already exists
  char path[128];
  const std::time_t now = std::time(nullptr);
  std::tm tm{};
  localtime_r(&now, &tm);
  std::strftime(path, sizeof(path), "screenshots/crosspoint-%Y%m%d-%H%M%S.bmp", &tm);

  if (SDL_SaveBMP(surface, path) == 0) {
    printf("[SHOT] %s (%dx%d)\n", path, TEX_W, TEX_H);
  } else {
    printf("[SHOT] save failed: %s\n", SDL_GetError());
  }
  fflush(stdout);
  SDL_FreeSurface(surface);

  // The read left the window showing the offscreen render; put the panel back.
  sim_window_present();
}
