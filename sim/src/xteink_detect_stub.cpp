#include <XteinkDetect.h>

// The emulator's board is fixed at configure time (CROSSPOINT_DEVICE), so there
// is nothing to probe. Returning false leaves BoardConfig::ACTIVE's default
// controller in place -- the same outcome as a real probe that does not
// fingerprint an UltraChip part.

namespace freeink {
bool applyXteinkDisplayController() { return false; }
}  // namespace freeink
