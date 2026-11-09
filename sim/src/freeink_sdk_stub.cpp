// SDK helpers the emulator has no hardware for.

namespace freeink {
// Selects the panel controller variant for a batch of Xteink units at runtime.
// The sim always renders through its own SDL display, so there is nothing to
// switch and this is a no-op.
void applyXteinkDisplayController() {}
}  // namespace freeink
