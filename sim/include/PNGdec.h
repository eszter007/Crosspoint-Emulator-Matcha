#pragma once

// Stub for bitbank2/PNGdec. The emulator decodes PNGs with stb_image (see
// sim/src/image_decoder_stubs.cpp), so none of the library is linked -- but
// SleepActivity validates a sleep-overlay PNG's IHDR by hand and names the
// colour types through PNGdec's enum. These are the PNG spec's colour-type
// values, which is what PNGdec's enum carries.

enum {
  PNG_PIXEL_GRAYSCALE = 0,
  PNG_PIXEL_TRUECOLOR = 2,
  PNG_PIXEL_INDEXED = 3,
  PNG_PIXEL_GRAY_ALPHA = 4,
  PNG_PIXEL_TRUECOLOR_ALPHA = 6,
};
