# Crosspoint Emulator — Japanese Reading Fork

A fork of [jonmooreai/Crosspoint-Emulator](https://github.com/jonmooreai/Crosspoint-Emulator).

The emulator runs the [Crosspoint](https://github.com/crosspoint-reader/crosspoint-reader) e-reader firmware on your Mac or PC using an SDL2 window, directory-backed SD card, and keyboard input. This fork targets the https://github.com/eszter007/crosspoint-reader-JP, a Japanese-enabled Crosspoint firmware fork rather than mainline Crosspoint.

---

## What this fork adds

The upstream emulator ([jonmooreai/Crosspoint-Emulator](https://github.com/jonmooreai/Crosspoint-Emulator)) already provides a complete desktop emulator with a library grid, thumbnail prewarm, UX polish (button press feedback, centralized constants), and detailed build documentation. This fork does not change any of that. It improves support for rendering the Japanese language learning fork Matcha Reader:
https://github.com/eszter007/matcha-reader

---

## Quick start

### Requirements

- macOS or Linux (Windows: see upstream README)
- C++17 compiler, CMake 3.16+, SDL2 2.x, Python 3, Git
- libcurl (pre-installed on macOS; `libcurl4-openssl-dev` on Linux)

### 1. Clone

```sh
git clone https://github.com/eszter007/Crosspoint-Emulator.git
cd Crosspoint-Emulator
git clone https://github.com/eszter007/crosspoint-reader-JP.git crosspoint-reader
cd crosspoint-reader && git checkout claude/vertical-japanese-text-pyosmu && cd ..
```

> **Note:** This fork clones the firmware as `crosspoint-reader/` inside the emulator directory rather than as a sibling `../Crosspoint`. The `CMakeLists.txt` has been updated accordingly.

### 2. Install dependencies (macOS)

```sh
xcode-select --install
brew install cmake sdl2
```

### 3. Generate i18n strings

```sh
python3 crosspoint-reader/scripts/gen_i18n.py \
  crosspoint-reader/lib/I18n/translations \
  crosspoint-reader/lib/I18n/
```

### 4. Build

```sh
mkdir -p build && cd build
cmake ..
cmake --build .
cd ..
```

### 5. Set up the SD card directory

```sh
mkdir -p sdcard
cp /path/to/japanese-book.epub sdcard/
```

### 6. Set up dictionaries

Convert and install the dictionary indexes the word lookup feature needs:

```sh
# Download Jitendex (Yomitan format): https://github.com/stephenmk/Jitendex
# Download JMnedict: https://github.com/JMdictProject
# Download a grammar dictionary in Yomitan format

python3 tools/dict_convert/convert_jmdict.py \
  --input /path/to/jitendex-yomitan.zip \
  --output-dir sdcard/dict/

python3 tools/dict_convert/convert_jmdict.py \
  --input /path/to/JMnedict.zip \
  --output-dir sdcard/dict/ \
  --names

python3 tools/dict_convert/convert_jmdict.py \
  --input /path/to/grammar-dict.zip \
  --output-dir sdcard/dict/ \
  --grammar
```

This produces `jmdict.idx`/`.dat`, `jmnedict.idx`/`.dat`, and `grammar.idx`/`.dat` in `sdcard/dict/`.

### 7. Set up translation (optional)

```sh
echo -n "YOUR_GEMINI_KEY_HERE" > sdcard/gemini.key
```

Get a key from [Google AI Studio](https://aistudio.google.com/apikey).

### 8. Japanese fonts (optional)

Place `.cpfont` files in `sdcard/.fonts/`. The emulator runs with built-in fonts, but SD card fonts (UDDigiKyokasho, Noto Serif JP) give better vertical text results. See `tools/build-sd-fonts.py` and `tools/sd-fonts.yaml`.

### 9. Run

```sh
./build/crosspoint_emulator
```

---

## Using the Japanese features

### Word lookup

1. Open a Japanese EPUB
2. Press **Enter** to open the reader menu
3. Select **Word Lookup**
4. **Left/Right** — move between matched words on the page
5. **Up/Downt** — scroll within a long definition entry
6. **Back** — return to reading

### Page translation

1. Open a Japanese EPUB
2. Press **Enter** → select **Translate Page**
3. Wait for the translation to complete
4. **Up/Down** — scroll the translation overlay
5. **Back** — return to reading

---

## Keyboard shortcuts

| Keys | Action |
|------|--------|
| Arrow keys | Up / Down / Left / Right (physical buttons) |
| Enter / Numpad Enter | Confirm |
| Backspace / Escape | Back |
| P | Power |
| P + Down (hold) | **Take a screenshot** — saves a BMP to `sdcard/screenshots/` and flashes a border on-screen to confirm the capture. Filename includes the book title, chapter, page, and progress percentage when a book is open. |

### Waking from sleep

Sleep mode has no real low-power state on desktop — the simulated "deep sleep" call is a no-op, so once the sleep screen is showing, press **Enter** to wake up. This resumes the last open book (if sleep was entered from the reader) or returns to Home, mirroring the same logic a real device uses after waking from an actual reboot. On real hardware, wake is triggered by holding the physical Power button instead.

---

## Architecture

The emulator runs the firmware source unchanged, replacing hardware components with desktop equivalents:

| Component  | Device                    | Emulator                        |
|------------|---------------------------|---------------------------------|
| Display    | 800×480 e-ink             | SDL2 window                     |
| Storage    | SD card                   | `./sdcard/` directory           |
| Input      | Physical buttons          | Keyboard                        |
| Images     | On-device JPEG/PNG decode | stb_image                       |
| Network    | ESP32 WiFi + HTTP         | libcurl (translation only)      |
| Dictionary | SD card binary index      | Same (via simulated SD)         |

### Key directories

```
Crosspoint-Emulator/
  CMakeLists.txt                   # Build configuration (modified from upstream)
  sim/src/                         # Emulator HAL — unchanged from upstream
  sim/include/                     # Emulator HAL headers — unchanged from upstream
  sdcard/                          # Simulated SD card root
    dict/                          # Dictionary indexes (jmdict, jmnedict, grammar)
    gemini.key                     # Gemini API key (optional)
    .crosspoint/                   # Cached EPUB layouts (auto-generated)
  crosspoint-reader/               # Japanese firmware fork (cloned here, not as sibling)
    lib/Dict/                      # Dictionary lookup, deinflection engine
    lib/Epub/                      # EPUB parsing + vertical text layout engine
    lib/GfxRenderer/               # Rendering engine
    src/activities/reader/         # Reader UI (word lookup, translation, menu)
```

---

## Troubleshooting

**Build fails with missing i18n headers** — Run the `gen_i18n.py` step before building (step 3 above). If `ArduinoJson.h` errors appear, those activities are excluded from the emulator build; do a clean rebuild: `rm -rf build && mkdir build && cd build && cmake .. && cmake --build .`

**No books appear** — Check that `.epub` files are directly in `sdcard/`, not nested in a subdirectory. Verify with `ls sdcard/*.epub`.

**Word Lookup not in menu** — Dictionary index files must exist in `sdcard/dict/`. Check with `ls sdcard/dict/jmdict.idx`.

**Translation shows "No API key"** — Create `sdcard/gemini.key` containing your Gemini key as a raw string with no quotes or newline.

**Translation returns HTTP 429** — Rate limit on Google's free tier. Wait a minute and retry.

**Vertical text layout looks stale after a code change** — Delete `sdcard/.crosspoint/` and restart.

**Terminal is noisy with `[DBG]`/`[INF]` log lines** — Debug logging (`ENABLE_SERIAL_LOG`, `LOG_LEVEL=2`) is on by default in this fork's `CMakeLists.txt`, useful for diagnosing emulator-specific issues. Lower `LOG_LEVEL` to `1` (errors + info) or `0` (errors only) there if you want quieter output.

**SDL2 not found** — `brew install sdl2` (macOS) or `apt install libsdl2-dev` (Linux).

**File Transfer / WiFi Setup / KOReader Sync / OPDS / Calibre Connect / Firmware Update show "Not available in the emulator"** — Expected. These activities need real WiFi/HTTP hardware and are excluded from the emulator build (see `sim/src/excluded_activity_stubs.cpp`); the stub screen exists so entering them doesn't silently freeze — press **Back** to return home. They work normally on real hardware.

For general build and emulator issues not specific to the Japanese features, see the [upstream README](https://github.com/jonmooreai/Crosspoint-Emulator).

---

## Contributing

1. Fork and clone
2. Create a feature branch
3. Test that the emulator builds and runs
4. Open a pull request describing the change

Built on top of [jonmooreai/Crosspoint-Emulator](https://github.com/jonmooreai/Crosspoint-Emulator), which is in turn built on the [Crosspoint](https://github.com/crosspoint-reader/crosspoint-reader) e-reader firmware.
