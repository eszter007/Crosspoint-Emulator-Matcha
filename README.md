# Crosspoint Emulator — Japanese Reading Companion

A desktop emulator for the [Crosspoint](https://github.com/crosspoint-reader/crosspoint-reader) e-reader firmware (Xteink X4), built for **Japanese language learners**. Read Japanese EPUBs with built-in dictionary lookup, verb deinflection, grammar references, and page translation — all running natively on your Mac or PC without the physical device.

This is a fork of the Crosspoint firmware with extensive Japanese language support added on top of the base e-reader functionality.

---

## Features

### Japanese Reading Support

- **Vertical text (tategaki)** — Full vertical Japanese layout with proper punctuation positioning (brackets, dashes, ellipsis), small kana placement, and font-adaptive spacing. Works with UDDigiKyokasho, Noto Serif, and Noto Sans.
- **Furigana (ruby text)** — Reading aids rendered above (horizontal) or beside (vertical) kanji, with anti-overlap logic so dense furigana doesn't collide.
- **Bold, italic, and emphasis marks** — Sesame dots (﹅) and other text formatting preserved in both reading modes.

### Dictionary & Word Lookup

- **Built-in JMdict dictionary** — Look up any word on the current page. Available in both vertical and horizontal reading modes via the reader menu.
- **Multiple readings** — Kanji with multiple dictionary entries (e.g. 家 → いえ, うち, や) show all readings sorted by frequency.
- **Verb deinflection** — Conjugated forms resolve to their dictionary base: 読まれた→読む, 食べさせられて→食べる, 出された→出す. Covers passive, causative, te-form, masu-stem, and compound auxiliaries.
- **Grammar dictionary** — Integrated grammar reference (from "Dictionary of Japanese Grammar") surfaces grammar patterns alongside vocabulary definitions.
- **Name dictionary (JMnedict)** — Japanese names are recognized and grouped with honorifics (根岸さん, 和樹くん shown as one unit, not split).
- **Smart word boundaries** — Pre-scans the page to find dictionary-matchable positions. Skips particles, digits, and punctuation. Handles compound words (間取り図) and bound suffixes (設計士).
- **Scrollable definitions** — Long entries scroll with Left/Right buttons. Entry navigation with Up/Down.

### Page Translation

- **Gemini AI translation** — Translate the current page from Japanese to English via Google's Gemini 2.5 Flash API. Shows a scrollable English translation overlay.
- **Works in the emulator** — Desktop version uses libcurl (no WiFi needed). Device version uses ESP32 WiFi + HTTP client.

### Image Handling

- **Dedicated image pages** — Images in vertical mode render on their own page at full size.
- **Aspect-aware rotation** — Landscape images on a portrait screen (or vice versa) auto-rotate so the reader tilts the device naturally.
- **Status bar respected** — Rotated images inset correctly so they don't overlap the status bar.
- **No blank pages** — Consecutive images no longer produce empty pages between them.

### E-Reader Features (from Crosspoint base)

- EPUB, TXT, and XTC format support
- Library grid with cover thumbnails and reading progress
- Multiple font families and sizes
- Bookmarks and progress tracking
- Theme support (Classic, Lyra)
- Multiple display orientations

---

## Quick Start

### Requirements

- **macOS** or **Linux** (Windows: see [Windows Setup](#setup-on-windows) below)
- C++17 compiler, CMake 3.16+, SDL2, Python 3, Git
- libcurl (included on macOS; `libcurl4-openssl-dev` on Linux)

### 1. Clone

```bash
git clone https://github.com/eszter007/Crosspoint-Emulator.git
cd Crosspoint-Emulator
git clone https://github.com/eszter007/crosspoint-reader-JP.git crosspoint-reader
cd crosspoint-reader && git checkout claude/vertical-japanese-text-pyosmu && cd ..
```

### 2. Install dependencies (macOS)

```bash
xcode-select --install        # C++ compiler
brew install cmake sdl2        # Build tools
```

### 3. Generate i18n strings

```bash
python3 crosspoint-reader/scripts/gen_i18n.py \
  crosspoint-reader/lib/I18n/translations \
  crosspoint-reader/lib/I18n/
```

### 4. Build

```bash
mkdir -p build && cd build
cmake ..
cmake --build .
cd ..
```

### 5. Set up the SD card directory

```bash
mkdir -p sdcard
# Add a Japanese EPUB:
cp /path/to/japanese-book.epub sdcard/
```

### 6. Set up dictionaries

Convert and install the dictionaries the word lookup feature needs:

```bash
# Download Jitendex (Yomitan format) from https://github.com/stephenmk/Jitendex
# Download JMnedict from https://github.com/JMdictProject
# Download Grammar dictionary (Yomitan format)

# Convert dictionaries:
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

To use the "Translate Page" feature:

1. Get a Gemini API key from [Google AI Studio](https://aistudio.google.com/apikey)
2. Create the key file:
   ```bash
   echo -n "AIzaSyYOUR_KEY_HERE" > sdcard/gemini.key
   ```

### 8. Set up Japanese fonts (optional)

For the best vertical text experience, place `.cpfont` font files in `sdcard/.fonts/`. The emulator works with the built-in fonts, but SD card fonts (UDDigiKyokasho, Noto Serif JP) give better results for Japanese text.

See `tools/build-sd-fonts.py` and `tools/sd-fonts.yaml` for font conversion.

### 9. Run

```bash
./build/crosspoint_emulator
```

### Keyboard Controls

| Key | Action |
|-----|--------|
| Arrow Keys | Navigate / Page turn |
| Enter | Confirm / Open menu |
| Backspace / Escape | Back |
| P | Power |

### Using Word Lookup

1. Open a Japanese book
2. Press **Enter** to open the reader menu
3. Select **Word Lookup**
4. **Up/Down** — navigate between matched words
5. **Left/Right** — scroll within a long definition
6. **Back** — return to reading

### Using Translation

1. Open a Japanese book
2. Press **Enter** → select **Translate Page**
3. Wait for "Translating..." to complete
4. **Up/Down** — scroll the translation
5. **Back** — return to reading

---

## Setup on macOS (detailed)

1. **Xcode Command Line Tools**: `xcode-select --install`
2. **Homebrew**: `/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"`
3. **CMake + SDL2**: `brew install cmake sdl2`
4. **Python 3**: `python3 --version` (pre-installed on macOS; `brew install python3` if missing)
5. **Clone and build** — follow [Quick Start](#quick-start) above

## Setup on Windows

1. Install **Visual Studio Build Tools** with "Desktop development with C++"
2. Install **CMake** from cmake.org (add to PATH)
3. Install **SDL2** development package — extract to e.g. `C:\SDL2`
4. Install **Python 3** from python.org (add to PATH)
5. Clone repos and build:
   ```cmd
   git clone https://github.com/eszter007/Crosspoint-Emulator.git
   cd Crosspoint-Emulator
   git clone https://github.com/eszter007/crosspoint-reader-JP.git crosspoint-reader
   cd crosspoint-reader && git checkout claude/vertical-japanese-text-pyosmu && cd ..
   python3 crosspoint-reader/scripts/gen_i18n.py crosspoint-reader/lib/I18n/translations crosspoint-reader/lib/I18n/
   mkdir build && cd build
   cmake .. -DSDL2_ROOT=C:\SDL2
   cmake --build . --config Release
   ```

---

## Architecture

The emulator runs the **same firmware code** as the Xteink X4 device, replacing hardware-specific components with desktop equivalents:

| Component | Device | Emulator |
|-----------|--------|----------|
| Display | 800x480 E-Ink | SDL2 window |
| Storage | SD card | `./sdcard/` directory |
| Input | Physical buttons | Keyboard |
| Images | On-device JPEG/PNG decode | stb_image |
| Network | ESP32 WiFi + HTTP | libcurl (translation only) |
| Dictionary | SD card binary index | Same (via simulated SD) |

### Key directories

```
Crosspoint-Emulator/
  CMakeLists.txt          # Build configuration
  sim/src/                # Emulator HAL (display, storage, input, stubs)
  sim/include/            # Emulator HAL headers
  sdcard/                 # Simulated SD card root
    dict/                 # Dictionary files (jmdict, jmnedict, grammar)
    gemini.key            # Gemini API key (optional)
    .crosspoint/          # Cached EPUB layouts (auto-generated)
  crosspoint-reader/      # Firmware source (submodule or clone)
    lib/Dict/             # Dictionary lookup, deinflection
    lib/Epub/             # EPUB parsing, vertical text layout
    lib/GfxRenderer/      # Rendering engine
    src/activities/reader/ # Reader UI (word lookup, translation, menu)
```

---

## Troubleshooting

**Build fails with missing headers**: Run `python3 crosspoint-reader/scripts/gen_i18n.py ...` first (generates I18n headers). If `ArduinoJson.h` errors appear, those activities are excluded from the emulator build — do a clean rebuild: `rm -rf build && mkdir build && cd build && cmake .. && cmake --build .`

**No books appear**: Ensure `.epub` files are directly in `sdcard/` (not nested). Check: `ls sdcard/*.epub`

**Word Lookup not in menu**: Dictionary files must be in `sdcard/dict/`. Check: `ls sdcard/dict/jmdict.idx`

**Translation shows "No API key"**: Create `sdcard/gemini.key` with your Gemini API key (raw key string, no quotes).

**Translation shows HTTP 429**: Rate limit on Google's free tier. Wait a minute and retry.

**Vertical text cache stale after code changes**: Delete `sdcard/.crosspoint/` and restart the emulator.

**SDL2 not found**: `brew install sdl2` (macOS) or `apt install libsdl2-dev` (Linux).

---

## Contributing

1. Fork and clone
2. Create a feature branch
3. Test the emulator builds and runs
4. Open a pull request with a description of changes

Built on top of the [Crosspoint](https://github.com/crosspoint-reader/crosspoint-reader) e-reader firmware project.
