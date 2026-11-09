#pragma once

// Sim stub for the PNGdec library. SleepActivity includes PNGdec.h only for
// the pixel-type constants it compares the IHDR colour-type byte against when
// validating a sleep-overlay PNG; the actual decode goes through
// PngToFramebufferConverter, which the emulator implements over stb_image in
// sim/src/image_decoder_stubs.cpp. Values mirror PNGdec's own enum (and the
// PNG spec's colour-type codes).
enum {
  PNG_PIXEL_GRAYSCALE = 0,
  PNG_PIXEL_TRUECOLOR = 2,
  PNG_PIXEL_INDEXED = 3,
  PNG_PIXEL_GRAY_ALPHA = 4,
  PNG_PIXEL_TRUECOLOR_ALPHA = 6,
};
