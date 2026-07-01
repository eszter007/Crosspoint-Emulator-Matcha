// Emulator image decoders for in-book images (JPEG/PNG).
//
// Decodes via stb_image, downscales with area-averaging, Atkinson-dithers to
// 2-bit grayscale, writes the same pixel-cache format ImageBlock::renderFromCache
// reads (uint16 w, uint16 h, then 2bpp rows, 4px/byte, MSB first), and renders
// the current pass directly to the framebuffer via DirectPixelWriter.

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>

#include <HalStorage.h>
#include <HardwareSerial.h>

#include "BitmapHelpers.h"
#include "Epub/converters/DirectPixelWriter.h"
#include "Epub/converters/JpegToFramebufferConverter.h"
#include "Epub/converters/PngToFramebufferConverter.h"
#include "stb_image.h"

namespace {

bool readFileToVector(const std::string& path, std::vector<uint8_t>& out) {
  HalFile f;
  if (!Storage.openFileForRead("IMG", path, f)) return false;
  const int sz = f.size();
  if (sz <= 0) return false;
  out.resize(sz);
  const int n = f.read(out.data(), sz);
  f.close();
  return n == sz;
}

bool getDims(const std::string& path, ImageDimensions& out) {
  std::vector<uint8_t> data;
  if (!readFileToVector(path, data)) return false;
  int w = 0, h = 0, comp = 0;
  if (!stbi_info_from_memory(data.data(), static_cast<int>(data.size()), &w, &h, &comp)) return false;
  out.width = static_cast<int16_t>(w);
  out.height = static_cast<int16_t>(h);
  return true;
}

bool decode(const std::string& path, GfxRenderer& renderer, const RenderConfig& config) {
  std::vector<uint8_t> data;
  if (!readFileToVector(path, data)) {
    Serial.printf("[IMG] read failed: %s\n", path.c_str());
    return false;
  }

  int srcW = 0, srcH = 0, comp = 0;
  unsigned char* pixels =
      stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &srcW, &srcH, &comp, 1);
  if (!pixels) {
    Serial.printf("[IMG] stb_image failed (%s): %s\n", path.c_str(), stbi_failure_reason());
    return false;
  }

  // Output dimensions: exact if requested, otherwise fit within max box.
  int outW = config.maxWidth;
  int outH = config.maxHeight;
  if (!config.useExactDimensions && srcW > 0 && srcH > 0) {
    const float scaleW = static_cast<float>(config.maxWidth) / srcW;
    const float scaleH = static_cast<float>(config.maxHeight) / srcH;
    const float scale = std::min({scaleW, scaleH, 1.0f});
    outW = std::max(1, static_cast<int>(srcW * scale));
    outH = std::max(1, static_cast<int>(srcH * scale));
  }
  if (outW < 1) outW = 1;
  if (outH < 1) outH = 1;

  // Downscale (area average) to grayscale, then Atkinson-dither to 2-bit.
  const int bytesPerRow = (outW + 3) / 4;  // 2bpp, 4px/byte
  std::vector<uint8_t> packed(static_cast<size_t>(bytesPerRow) * outH, 0);

  const uint32_t scaleX_fp = (static_cast<uint32_t>(srcW) << 16) / outW;
  const uint32_t scaleY_fp = (static_cast<uint32_t>(srcH) << 16) / outH;

  AtkinsonDitherer ditherer(outW);
  for (int y = 0; y < outH; y++) {
    const int srcYStart = (static_cast<uint32_t>(y) * scaleY_fp) >> 16;
    const int srcYEnd =
        std::min(static_cast<int>((static_cast<uint32_t>(y + 1) * scaleY_fp) >> 16), srcH);
    const int srcYCount = std::max(1, srcYEnd - srcYStart);
    uint8_t* row = packed.data() + static_cast<size_t>(y) * bytesPerRow;

    for (int x = 0; x < outW; x++) {
      const int srcXStart = (static_cast<uint32_t>(x) * scaleX_fp) >> 16;
      const int srcXEnd =
          std::min(static_cast<int>((static_cast<uint32_t>(x + 1) * scaleX_fp) >> 16), srcW);
      const int srcXCount = std::max(1, srcXEnd - srcXStart);

      int sum = 0;
      for (int sy = srcYStart; sy < srcYStart + srcYCount && sy < srcH; sy++)
        for (int sx = srcXStart; sx < srcXStart + srcXCount && sx < srcW; sx++)
          sum += pixels[sy * srcW + sx];
      const uint8_t gray = static_cast<uint8_t>(sum / (srcYCount * srcXCount));

      const uint8_t v = ditherer.processPixel(gray, x);  // 0=black .. 3=white
      row[x >> 2] |= (v & 0x03) << (6 - (x & 3) * 2);
    }
    ditherer.nextRow();
  }
  stbi_image_free(pixels);

  // Write pixel cache for subsequent strip passes.
  if (!config.cachePath.empty()) {
    HalFile cf;
    if (Storage.openFileForWrite("IMG", config.cachePath, cf)) {
      const uint16_t w16 = static_cast<uint16_t>(outW);
      const uint16_t h16 = static_cast<uint16_t>(outH);
      cf.write(&w16, 2);
      cf.write(&h16, 2);
      cf.write(packed.data(), packed.size());
      cf.close();
    }
  }

  // Render this pass directly to the framebuffer. DirectPixelWriter does no
  // bounds checking on its own (by design -- see its header comment), so a
  // destY/destX past the logical screen must be skipped here: an out-of-range
  // input maps, via the orientation rotation, to a physical column past the
  // row's byte width, and the resulting byte index spills into an adjacent
  // row's bytes instead of being dropped -- corrupting unrelated content.
  // The real firmware's JPEGDEC-based decoder already clamps for this; this
  // sim-only stub was missing the equivalent guard.
  const int screenW = renderer.getScreenWidth();
  const int screenH = renderer.getScreenHeight();
  DirectPixelWriter pw;
  pw.init(renderer);
  for (int row = 0; row < outH; row++) {
    const int destY = config.y + row;
    if (destY < 0 || destY >= screenH) continue;
    const uint8_t* rowBuf = packed.data() + static_cast<size_t>(row) * bytesPerRow;
    pw.beginRow(destY);
    int colStart, colEnd;
    pw.bandColRange(config.x, outW, colStart, colEnd);
    for (int col = colStart; col < colEnd; col++) {
      const int destX = config.x + col;
      if (destX < 0 || destX >= screenW) continue;
      const uint8_t v = (rowBuf[col >> 2] >> (6 - (col & 3) * 2)) & 0x03;
      pw.writePixel(destX, v);
    }
  }

  return true;
}

}  // namespace

bool JpegToFramebufferConverter::getDimensionsStatic(const std::string& path, ImageDimensions& out) {
  return getDims(path, out);
}

bool JpegToFramebufferConverter::decodeToFramebuffer(const std::string& path, GfxRenderer& renderer,
                                                     const RenderConfig& config) {
  return decode(path, renderer, config);
}

bool JpegToFramebufferConverter::supportsFormat(const std::string& extension) {
  std::string ext = extension;
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return ext == ".jpg" || ext == ".jpeg";
}

bool PngToFramebufferConverter::getDimensionsStatic(const std::string& path, ImageDimensions& out) {
  return getDims(path, out);
}

bool PngToFramebufferConverter::decodeToFramebuffer(const std::string& path, GfxRenderer& renderer,
                                                    const RenderConfig& config) {
  return decode(path, renderer, config);
}

bool PngToFramebufferConverter::supportsFormat(const std::string& extension) {
  std::string ext = extension;
  std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
  return ext == ".png";
}
