#pragma once

#define FREEINK_LOG_TRANSPORT_ROM_PRINTF 0
#define FREEINK_LOG_TRANSPORT_SERIAL 1
#define FREEINK_LOG_TRANSPORT FREEINK_LOG_TRANSPORT_SERIAL

namespace BoardConfig {
inline bool hasTouch() { return false; }
inline void holdPowerRails() {}
}  // namespace BoardConfig
