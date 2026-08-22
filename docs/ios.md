# Running the emulator on an iPhone

The emulator builds as an iOS app: the same firmware sources, the same board
profile, driven by your finger instead of a keyboard. It is the natural home for
the X4 Pro target, whose real input is a touch panel and a capacitive Home key.

Everything below needs a Mac with Xcode. There is no way to build or run an iOS
app from Linux or Windows.

## What you need

- macOS with Xcode 15+ and the iOS SDK
- CMake 3.16+
- An **SDL2** source checkout (not SDL3, and not a Homebrew install — iOS needs
  SDL compiled for the device):

  ```sh
  git clone --branch SDL2 https://github.com/libsdl-org/SDL.git ~/src/SDL2
  ```

- The firmware, cloned into the emulator directory as `crosspoint-reader/`, with
  its i18n strings generated — see the main README's Quick start.
- An Apple developer account. A free one is enough to run on your own device;
  the app is not distributable without a paid one.

## Configure and build

```sh
mkdir -p build-ios && cd build-ios
cmake -G Xcode \
      -DCMAKE_SYSTEM_NAME=iOS \
      -DCROSSPOINT_DEVICE=x4pro \
      -DCROSSPOINT_ROOT=../crosspoint-reader \
      -DSDL2_SOURCE_DIR=$HOME/src/SDL2 \
      ..
open crosspoint-emulator.xcodeproj
```

In Xcode, select your device, set a signing team on the `crosspoint_emulator`
target (Signing & Capabilities — the generated project has no team baked in),
and Run.

To simulate the button-driven X4 instead, configure with
`-DCROSSPOINT_DEVICE=x4`. One device per build; see the README.

## Getting books onto it

The app's Documents directory *is* the SD card. `UIFileSharingEnabled` and
`LSSupportsOpeningDocumentsInPlace` are set, so it shows up in the Files app
under **On My iPhone → Crosspoint**, and in Finder's Files tab when the phone is
plugged in. Drop `.epub` files straight in there.

The firmware's own `.crosspoint/` cache is created alongside them, exactly as it
is on a card.

## How it is laid out

The panel is 800x480, read out into a 480x800 portrait image — the way the
device is held. It is scaled to fit the screen with its aspect ratio intact and
centred above a control strip carrying the board's physical keys: for the X4 Pro
the two page keys, Power, and the capacitive Home key.

Those keys are drawn by the emulator, not the firmware. They exist because a
phone has no keyboard and the panel itself is touch input; the strip is what the
desktop build's keyboard shortcuts stand in for. It is sized from the safe-area
insets, so nothing sits under the home indicator.

The app is portrait-locked. Rotating the phone would letterbox the panel into a
sliver without simulating anything — the firmware's own orientation setting
rotates the panel instead, exactly as it does on hardware.

## Differences from the device

- **Touch fidelity.** Gestures are classified against the SDK's own thresholds
  (28 px tap slop, 60 px swipe, 500 ms long press), so what registers here
  registers on hardware. The one deliberate difference: hardware only notices a
  lift on its next I2C poll, so its contact durations carry ~120 ms of
  hold-over. The emulator adds that to the measured duration rather than to the
  event, keeping the swipe time window honest without the input feeling laggy.
- **Multi-touch** is not simulated. Nothing in the firmware's HAL exposes it.
- **The frontlight** is a tint over the panel. Brightness and warmth do
  something visible, but a backlit LCD cannot reproduce light added to a
  reflective panel.
- **No translation.** The translation activity's HTTP client is libcurl, which
  iOS has no linkable copy of, so that one screen reports the missing client.
  Everything else in the activity still runs.
- **Wall-clock speed.** E-ink refresh timing is not simulated: pages appear
  instantly instead of taking the panel's 1-2 s.

## If the build fails

- **`iOS builds need -DSDL2_SOURCE_DIR=...`** — the path must point at an SDL2
  checkout's top level (the directory with its `CMakeLists.txt`).
- **Signing errors on Run** — set a team on the target. CMake cannot generate
  one, and Xcode will not run an unsigned app on a device.
- **Code changes not picked up** — re-run `cmake` after adding files; the Xcode
  generator writes the project once from the source lists.
