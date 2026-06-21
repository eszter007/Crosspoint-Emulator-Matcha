#include "JpegToBmpConverter.h"

#include <HardwareSerial.h>
#include <SdFat.h>

#include <cstdint>
#include <cstring>
#include <vector>

#include "BitmapHelpers.h"
#include "stb_image.h"

static constexpr int TARGET_MAX_WIDTH = 480;
static constexpr int TARGET_MAX_HEIGHT = 800;

static inline void write16(Print& out, uint16_t value) {
  out.write(value & 0xFF);
  out.write((value >> 8) & 0xFF);
}

static inline void write32(Print& out, uint32_t value) {
  out.write(value & 0xFF);
  out.write((value >> 8) & 0xFF);
  out.write((value >> 16) & 0xFF);
  out.write((value >> 24) & 0xFF);
}

static inline void write32Signed(Print& out, int32_t value) {
  out.write(value & 0xFF);
  out.write((value >> 8) & 0xFF);
  out.write((value >> 16) & 0xFF);
  out.write((value >> 24) & 0xFF);
}

static void writeBmpHeader1bit(Print& bmpOut, int width, int height) {
  const int bytesPerRow = (width + 31) / 32 * 4;
  const int imageSize = bytesPerRow * height;
  const uint32_t fileSize = 62 + imageSize;
  bmpOut.write('B'); bmpOut.write('M');
  write32(bmpOut, fileSize); write32(bmpOut, 0); write32(bmpOut, 62);
  write32(bmpOut, 40);
  write32Signed(bmpOut, width); write32Signed(bmpOut, -height);
  write16(bmpOut, 1); write16(bmpOut, 1);
  write32(bmpOut, 0); write32(bmpOut, imageSize);
  write32(bmpOut, 2835); write32(bmpOut, 2835);
  write32(bmpOut, 2); write32(bmpOut, 2);
  uint8_t palette[8] = {0x00,0x00,0x00,0x00, 0xFF,0xFF,0xFF,0x00};
  for (uint8_t b : palette) bmpOut.write(b);
}

static void writeBmpHeader2bit(Print& bmpOut, int width, int height) {
  const int bytesPerRow = (width * 2 + 31) / 32 * 4;
  const int imageSize = bytesPerRow * height;
  const uint32_t fileSize = 70 + imageSize;
  bmpOut.write('B'); bmpOut.write('M');
  write32(bmpOut, fileSize); write32(bmpOut, 0); write32(bmpOut, 70);
  write32(bmpOut, 40);
  write32Signed(bmpOut, width); write32Signed(bmpOut, -height);
  write16(bmpOut, 1); write16(bmpOut, 2);
  write32(bmpOut, 0); write32(bmpOut, imageSize);
  write32(bmpOut, 2835); write32(bmpOut, 2835);
  write32(bmpOut, 4); write32(bmpOut, 4);
  uint8_t palette[16] = {
    0x00,0x00,0x00,0x00, 0x55,0x55,0x55,0x00,
    0xAA,0xAA,0xAA,0x00, 0xFF,0xFF,0xFF,0x00
  };
  for (uint8_t b : palette) bmpOut.write(b);
}

static bool jpegToBmpInternal(HalFile& jpegFile, Print& bmpOut,
                               int targetWidth, int targetHeight,
                               bool oneBit, bool crop) {
  // Read entire JPEG into memory
  const int fileSize = jpegFile.size();
  if (fileSize <= 0) return false;
  std::vector<uint8_t> buf(fileSize);
  const int bytesRead = jpegFile.read(buf.data(), fileSize);
  if (bytesRead <= 0) return false;

  int srcW = 0, srcH = 0, channels = 0;
  unsigned char* pixels = stbi_load_from_memory(buf.data(), fileSize,
                                                 &srcW, &srcH, &channels, 1);
  if (!pixels) {
    Serial.printf("[JPG] stb_image failed: %s\n", stbi_failure_reason());
    return false;
  }

  int outW = srcW, outH = srcH;
  if (targetWidth > 0 && targetHeight > 0 &&
      (srcW > targetWidth || srcH > targetHeight)) {
    float scaleW = static_cast<float>(targetWidth) / srcW;
    float scaleH = static_cast<float>(targetHeight) / srcH;
    float scale = crop ? std::max(scaleW, scaleH) : std::min(scaleW, scaleH);
    outW = std::max(1, static_cast<int>(srcW * scale));
    outH = std::max(1, static_cast<int>(srcH * scale));
  }

  int bytesPerRow;
  if (oneBit) {
    writeBmpHeader1bit(bmpOut, outW, outH);
    bytesPerRow = (outW + 31) / 32 * 4;
  } else {
    writeBmpHeader2bit(bmpOut, outW, outH);
    bytesPerRow = (outW * 2 + 31) / 32 * 4;
  }

  std::vector<uint8_t> rowBuf(bytesPerRow, 0);
  Atkinson1BitDitherer ditherer1(oneBit ? outW : 1);
  AtkinsonDitherer ditherer2(oneBit ? 1 : outW);

  const uint32_t scaleX_fp = (static_cast<uint32_t>(srcW) << 16) / outW;
  const uint32_t scaleY_fp = (static_cast<uint32_t>(srcH) << 16) / outH;

  for (int outY = 0; outY < outH; outY++) {
    memset(rowBuf.data(), 0, bytesPerRow);
    const int srcYStart = (static_cast<uint32_t>(outY) * scaleY_fp) >> 16;
    const int srcYEnd = std::min(
        static_cast<int>((static_cast<uint32_t>(outY + 1) * scaleY_fp) >> 16), srcH);
    const int srcYCount = std::max(1, srcYEnd - srcYStart);

    for (int outX = 0; outX < outW; outX++) {
      const int srcXStart = (static_cast<uint32_t>(outX) * scaleX_fp) >> 16;
      const int srcXEnd = std::min(
          static_cast<int>((static_cast<uint32_t>(outX + 1) * scaleX_fp) >> 16), srcW);
      const int srcXCount = std::max(1, srcXEnd - srcXStart);

      int sum = 0;
      for (int sy = srcYStart; sy < srcYStart + srcYCount && sy < srcH; sy++)
        for (int sx = srcXStart; sx < srcXStart + srcXCount && sx < srcW; sx++)
          sum += pixels[sy * srcW + sx];
      const uint8_t gray = static_cast<uint8_t>(sum / (srcYCount * srcXCount));

      if (oneBit) {
        const uint8_t bit = ditherer1.processPixel(gray, outX);
        rowBuf[outX / 8] |= (bit << (7 - (outX % 8)));
      } else {
        const uint8_t adjusted = static_cast<uint8_t>(adjustPixel(gray));
        const uint8_t twoBit = ditherer2.processPixel(adjusted, outX);
        rowBuf[(outX * 2) / 8] |= (twoBit << (6 - ((outX * 2) % 8)));
      }
    }

    if (oneBit) ditherer1.nextRow();
    else ditherer2.nextRow();
    bmpOut.write(rowBuf.data(), bytesPerRow);
  }

  stbi_image_free(pixels);
  return true;
}

bool JpegToBmpConverter::jpegFileToBmpStream(HalFile& jpegFile, Print& bmpOut, bool crop) {
  return jpegToBmpInternal(jpegFile, bmpOut, TARGET_MAX_WIDTH, TARGET_MAX_HEIGHT, false, crop);
}

bool JpegToBmpConverter::jpegFileToBmpStreamWithSize(HalFile& jpegFile, Print& bmpOut,
                                                      int targetWidth, int targetHeight) {
  return jpegToBmpInternal(jpegFile, bmpOut, targetWidth, targetHeight, false, true);
}

bool JpegToBmpConverter::jpegFileTo1BitBmpStreamWithSize(HalFile& jpegFile, Print& bmpOut,
                                                          int targetWidth, int targetHeight) {
  return jpegToBmpInternal(jpegFile, bmpOut, targetWidth, targetHeight, true, true);
}

bool JpegToBmpConverter::jpegFileToBmpStreamInternal(HalFile& jpegFile, Print& bmpOut,
                                                      int targetWidth, int targetHeight,
                                                      bool oneBit, bool crop) {
  return jpegToBmpInternal(jpegFile, bmpOut, targetWidth, targetHeight, oneBit, crop);
}
