#pragma once

#include "EInkDisplay.h"

class HalDisplay {
 public:
  HalDisplay();
  ~HalDisplay();

  enum RefreshMode {
    FULL_REFRESH,
    HALF_REFRESH,
    FAST_REFRESH,
  };

  void begin(bool seamless = false);

  static constexpr uint16_t DISPLAY_WIDTH = EInkDisplay::DISPLAY_WIDTH;
  static constexpr uint16_t DISPLAY_HEIGHT = EInkDisplay::DISPLAY_HEIGHT;
  static constexpr uint16_t DISPLAY_WIDTH_BYTES = EInkDisplay::DISPLAY_WIDTH_BYTES;
  static constexpr uint32_t BUFFER_SIZE = EInkDisplay::BUFFER_SIZE;

  void clearScreen(uint8_t color = 0xFF) const;
  void drawImage(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                 bool fromProgmem = false) const;
  void drawImageTransparent(const uint8_t* imageData, uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                            bool fromProgmem = false) const;

  void displayBuffer(RefreshMode mode = FAST_REFRESH, bool turnOffScreen = false);
  void displayBufferAsync(RefreshMode mode = FAST_REFRESH);
  void waitRefreshComplete();
  bool supportsAsyncRefresh() const;
  void displayGrayscaleBase(RefreshMode fallback = HALF_REFRESH, bool turnOffScreen = false);
  void refreshDisplay(RefreshMode mode = FAST_REFRESH, bool turnOffScreen = false);

  // Output polarity (night mode). As on hardware the framebuffer stays in
  // normal polarity; the inversion is applied on the way to the panel, which
  // here means on the way into the SDL texture.
  void setInverted(bool inverted);
  bool toggleInverted();
  bool isInverted() const;

  void deepSleep();

  // Real-hardware waveform preconditioning before a grayscale pass; no-op in sim.
  void preconditionGrayscale();
  void preconditionGrayscale(uint16_t x, uint16_t y, uint16_t w, uint16_t h);

  uint8_t* getFrameBuffer() const;
  uint8_t* lendFrameBufferStorage(uint32_t* sizeOut);
  void returnFrameBufferStorage();

  void copyGrayscaleBuffers(const uint8_t* lsbBuffer, const uint8_t* msbBuffer);
  void copyGrayscaleLsbBuffers(const uint8_t* lsbBuffer);
  void copyGrayscaleMsbBuffers(const uint8_t* msbBuffer);
  void cleanupGrayscaleBuffers(const uint8_t* bwBuffer);
  void displayGrayBuffer(bool turnOffScreen = false);

  void writeGrayscalePlaneStrip(bool lsbPlane, const uint8_t* rows, uint16_t yStart, uint16_t numRows);
  bool supportsStripGrayscale() const;
  // True when the panel drives the B/W base and the grayscale planes in one
  // waveform (Paper Mono). The emulated panels do not.
  bool combinesGrayscaleBase() const;

  uint16_t getDisplayWidth() const;
  uint16_t getDisplayHeight() const;
  uint16_t getDisplayWidthBytes() const;
  uint32_t getBufferSize() const;

 private:
  EInkDisplay einkDisplay;
};

extern HalDisplay display;
