#pragma once

#include <cstdint>

#define FREEINK_LOG_TRANSPORT_ROM_PRINTF 0
#define FREEINK_LOG_TRANSPORT_SERIAL 1
#define FREEINK_LOG_TRANSPORT FREEINK_LOG_TRANSPORT_SERIAL

// Host-side stand-in for the SDK's BoardConfig. The real header
// (freeink-sdk/libs/hardware/BoardConfig/include/BoardConfig.h) pulls in
// Arduino.h, driver/gpio.h and esp_rom_sys.h and carries every board's pin
// map, so the emulator mirrors just the surface the firmware reads on the
// host build. Keep field defaults identical to the SDK's -- layout code is
// tuned against them.
namespace BoardConfig {

// Bezel-covered edge columns of the panel's native portrait frame; the
// firmware keeps content out of them. Defaults match the SDK (X4-tuned).
struct ViewableInsets {
  uint8_t top = 9;
  uint8_t right = 3;
  uint8_t bottom = 3;
  uint8_t left = 3;
};

struct BoardProfile {
  const char* name = "Emulator";
  ViewableInsets viewableInsets = {};
};

inline BoardProfile ACTIVE = {};

// Board identity. The emulator is not any real board, so every is<Board>()
// predicate is false and firmware falls through to its generic path.
inline bool isX4Pro() { return false; }
inline bool isPaperMono() { return false; }

inline bool hasTouch() { return false; }
// No capacitive home key on the emulated board, matching hasTouch() above.
inline bool hasHomeKey() { return false; }
inline void holdPowerRails() {}

}  // namespace BoardConfig
