#include "EInkDisplay.h"
#include "HalDisplay.h"
#include "sim_display.h"
#include "sim_spi_bus.h"
#include "sim_window.h"

#include <cstring>

namespace {
bool g_hasBw = false;
bool g_hasGrayLsb = false;
bool g_hasGrayMsb = false;
bool g_inverted = false;
uint8_t g_bwBuffer[EInkDisplay::BUFFER_SIZE];
uint8_t g_grayLsbBuffer[EInkDisplay::BUFFER_SIZE];
uint8_t g_grayMsbBuffer[EInkDisplay::BUFFER_SIZE];
}  // namespace

HalDisplay display;

EInkDisplay::EInkDisplay(int8_t, int8_t, int8_t, int8_t, int8_t, int8_t) : frameBuffer(frameBuffer0), isScreenOn(false) {}

bool sim_display_init(void) { return sim_window_init(); }
void sim_display_shutdown(void) { sim_window_shutdown(); }
bool sim_display_pump_events(void) { return sim_window_pump(); }

void EInkDisplay::begin() {
  if (!sim_window_init()) return;
  frameBuffer = frameBuffer0;
  memset(frameBuffer0, 0xFF, EInkDisplay::BUFFER_SIZE);
  isScreenOn = true;
}

void EInkDisplay::clearScreen(uint8_t color) const { memset(frameBuffer, color, EInkDisplay::BUFFER_SIZE); }

void EInkDisplay::drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool) const {
  SpiBusGuard guard;
  if (!imageData || !frameBuffer) return;
  const uint16_t rowBytes = (w + 7) / 8;
  for (uint16_t row = 0; row < h; row++) {
    const uint16_t dstY = y + row;
    if (dstY >= EInkDisplay::DISPLAY_HEIGHT) break;
    for (uint16_t col = 0; col < w; col += 8) {
      const uint8_t byte = imageData[row * rowBytes + col / 8];
      for (int b = 0; b < 8 && col + b < w; b++) {
        const uint16_t dstX = x + col + b;
        if (dstX >= EInkDisplay::DISPLAY_WIDTH) break;
        // Icon format: 1 = white (transparent), 0 = black (foreground).
        // Only draw black pixels; leave white pixels as-is (transparent).
        if (!(byte & (0x80 >> b))) {
          const size_t idx = dstY * EInkDisplay::DISPLAY_WIDTH_BYTES + dstX / 8;
          frameBuffer[idx] &= ~(0x80 >> (dstX & 7));
        }
      }
    }
  }
}

void EInkDisplay::setFramebuffer(const uint8_t*) const {}
void EInkDisplay::copyGrayscaleBuffers(const uint8_t* lsb, const uint8_t* msb) {
  if (!lsb || !msb) return;
  memcpy(g_grayLsbBuffer, lsb, EInkDisplay::BUFFER_SIZE);
  memcpy(g_grayMsbBuffer, msb, EInkDisplay::BUFFER_SIZE);
  g_hasGrayLsb = true;
  g_hasGrayMsb = true;
}
void EInkDisplay::copyGrayscaleLsbBuffers(const uint8_t* lsb) {
  if (!lsb) return;
  memcpy(g_grayLsbBuffer, lsb, EInkDisplay::BUFFER_SIZE);
  g_hasGrayLsb = true;
}
void EInkDisplay::copyGrayscaleMsbBuffers(const uint8_t* msb) {
  if (!msb) return;
  memcpy(g_grayMsbBuffer, msb, EInkDisplay::BUFFER_SIZE);
  g_hasGrayMsb = true;
}
void EInkDisplay::cleanupGrayscaleBuffers(const uint8_t*) {
  g_hasGrayLsb = false;
  g_hasGrayMsb = false;
}

void EInkDisplay::displayBuffer(RefreshMode, bool) {
  SpiBusGuard guard;
  if (!frameBuffer) return;
  memcpy(g_bwBuffer, frameBuffer, EInkDisplay::BUFFER_SIZE);
  g_hasBw = true;
  g_hasGrayLsb = false;
  g_hasGrayMsb = false;
  sim_window_upload_bw(frameBuffer, g_inverted);
  sim_window_present();
}

void EInkDisplay::displayWindow(uint16_t, uint16_t, uint16_t, uint16_t) { displayBuffer(FAST_REFRESH); }
void EInkDisplay::displayGrayBuffer(bool) {
  SpiBusGuard guard;
  if (!g_hasBw || !g_hasGrayLsb || !g_hasGrayMsb) return;
  sim_window_upload_gray(g_bwBuffer, g_grayLsbBuffer, g_grayMsbBuffer, g_inverted);
  sim_window_present();
}
void EInkDisplay::refreshDisplay(RefreshMode mode, bool) { displayBuffer(mode); }
void EInkDisplay::displayGrayscaleBase(RefreshMode fallback, bool turnOffScreen) { displayBuffer(fallback, turnOffScreen); }
void EInkDisplay::grayscaleRevert() {}
void EInkDisplay::setCustomLUT(bool, const unsigned char*) {}
void EInkDisplay::deepSleep() { isScreenOn = false; }
void EInkDisplay::saveFrameBufferAsPBM(const char*) {}
// Real-hardware waveform preconditioning before a grayscale pass; no
// comparable panel timing concern in the sim.
void EInkDisplay::preconditionGrayscale() {}
void EInkDisplay::preconditionGrayscale(uint16_t, uint16_t, uint16_t, uint16_t) {}

HalDisplay::HalDisplay() : einkDisplay(0, 0, 0, 0, 0, 0) {}
HalDisplay::~HalDisplay() = default;

void HalDisplay::begin(bool) { einkDisplay.begin(); }
void HalDisplay::clearScreen(uint8_t color) const { einkDisplay.clearScreen(color); }
void HalDisplay::drawImage(const uint8_t* d, uint16_t x, uint16_t y, uint16_t w, uint16_t h, bool pm) const {
  einkDisplay.drawImage(d, x, y, w, h, pm);
}
void HalDisplay::drawImageTransparent(const uint8_t* d, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                                      bool pm) const {
  einkDisplay.drawImage(d, x, y, w, h, pm);
}
void HalDisplay::displayBuffer(RefreshMode mode, bool turnOffScreen) {
  einkDisplay.displayBuffer(static_cast<EInkDisplay::RefreshMode>(mode), turnOffScreen);
}
void HalDisplay::displayBufferAsync(RefreshMode mode) { displayBuffer(mode); }
void HalDisplay::waitRefreshComplete() {}
bool HalDisplay::supportsAsyncRefresh() const { return false; }
void HalDisplay::displayGrayscaleBase(RefreshMode fallback, bool turnOffScreen) {
  einkDisplay.displayGrayscaleBase(static_cast<EInkDisplay::RefreshMode>(fallback), turnOffScreen);
}
void HalDisplay::refreshDisplay(RefreshMode mode, bool turnOff) {
  einkDisplay.refreshDisplay(static_cast<EInkDisplay::RefreshMode>(mode), turnOff);
}

// Output polarity: the framebuffer stays in normal polarity and the inversion
// is applied on the way to the panel, matching the hardware driver. A change
// only shows on the next refresh, as it does on the device.
void HalDisplay::setInverted(bool inverted) { g_inverted = inverted; }
bool HalDisplay::toggleInverted() {
  g_inverted = !g_inverted;
  return g_inverted;
}
bool HalDisplay::isInverted() const { return g_inverted; }

void HalDisplay::deepSleep() { einkDisplay.deepSleep(); }
void HalDisplay::preconditionGrayscale() { einkDisplay.preconditionGrayscale(); }
void HalDisplay::preconditionGrayscale(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
  einkDisplay.preconditionGrayscale(x, y, w, h);
}
uint8_t* HalDisplay::getFrameBuffer() const { return einkDisplay.getFrameBuffer(); }
uint8_t* HalDisplay::lendFrameBufferStorage(uint32_t* sizeOut) {
  if (sizeOut) *sizeOut = BUFFER_SIZE;
  return einkDisplay.getFrameBuffer();
}
void HalDisplay::returnFrameBufferStorage() { memset(einkDisplay.getFrameBuffer(), 0xFF, BUFFER_SIZE); }
void HalDisplay::copyGrayscaleBuffers(const uint8_t* l, const uint8_t* m) { einkDisplay.copyGrayscaleBuffers(l, m); }
void HalDisplay::copyGrayscaleLsbBuffers(const uint8_t* l) { einkDisplay.copyGrayscaleLsbBuffers(l); }
void HalDisplay::copyGrayscaleMsbBuffers(const uint8_t* m) { einkDisplay.copyGrayscaleMsbBuffers(m); }
void HalDisplay::cleanupGrayscaleBuffers(const uint8_t* b) { einkDisplay.cleanupGrayscaleBuffers(b); }
void HalDisplay::displayGrayBuffer(bool turnOffScreen) { einkDisplay.displayGrayBuffer(turnOffScreen); }
void HalDisplay::writeGrayscalePlaneStrip(bool, const uint8_t*, uint16_t, uint16_t) {}
bool HalDisplay::supportsStripGrayscale() const { return false; }
bool HalDisplay::combinesGrayscaleBase() const { return false; }
uint16_t HalDisplay::getDisplayWidth() const { return DISPLAY_WIDTH; }
uint16_t HalDisplay::getDisplayHeight() const { return DISPLAY_HEIGHT; }
uint16_t HalDisplay::getDisplayWidthBytes() const { return DISPLAY_WIDTH_BYTES; }
uint32_t HalDisplay::getBufferSize() const { return BUFFER_SIZE; }
