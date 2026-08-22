cmake_minimum_required(VERSION 3.16)
project(crosspoint-emulator CXX C)

set(CMAKE_CXX_STANDARD 20)

if(NOT DEFINED CROSSPOINT_ROOT)
  set(CROSSPOINT_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../Crosspoint")
endif()
# if(EXISTS ...) is only well-defined for absolute paths; a relative
# CROSSPOINT_ROOT (e.g. -DCROSSPOINT_ROOT=./crosspoint-reader) resolves
# against the process's working directory, not CMAKE_CURRENT_SOURCE_DIR,
# silently breaking every EXISTS check below when configured from build/.
get_filename_component(CROSSPOINT_ROOT "${CROSSPOINT_ROOT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

if(NOT DEFINED FREEINK_SDK_ROOT)
  set(FREEINK_SDK_ROOT "${CROSSPOINT_ROOT}/freeink-sdk")
endif()
get_filename_component(FREEINK_SDK_ROOT "${FREEINK_SDK_ROOT}" ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

# --- Which device this emulator build simulates --------------------------------
# The FreeInk SDK composes a build from -DFREEINK_DEVICE_<NAME> flags and exposes
# the selected board as BoardConfig::ACTIVE, which the firmware reads for screen
# size, bezel insets, UI scale, touch, frontlight and button layout. The emulator
# uses the real SDK header, so picking a device here gives the simulated device
# the same profile the firmware sees on hardware.
#
# One device per build: the SDK rejects mixing MCU families in one binary, and
# X4 (ESP32-C3) and X4 Pro (ESP32-S3) are different families.
set(CROSSPOINT_DEVICE "x4" CACHE STRING "Device to simulate: x4 or x4pro")
set_property(CACHE CROSSPOINT_DEVICE PROPERTY STRINGS x4 x4pro)

if(CROSSPOINT_DEVICE STREQUAL "x4")
  set(CROSSPOINT_DEVICE_DEFINE FREEINK_DEVICE_X4=1)
elseif(CROSSPOINT_DEVICE STREQUAL "x4pro")
  set(CROSSPOINT_DEVICE_DEFINE FREEINK_DEVICE_X4PRO=1)
else()
  message(FATAL_ERROR "CROSSPOINT_DEVICE must be x4 or x4pro (got '${CROSSPOINT_DEVICE}')")
endif()
message(STATUS "Simulating device: ${CROSSPOINT_DEVICE}")

# --- iOS -----------------------------------------------------------------------
# Configure with the iOS toolchain to build the app instead of a desktop binary:
#   cmake -G Xcode -DCMAKE_SYSTEM_NAME=iOS -DCROSSPOINT_DEVICE=x4pro \
#         -DCROSSPOINT_ROOT=... -DSDL2_SOURCE_DIR=/path/to/SDL2 ..
# See docs/ios.md.
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
  set(CROSSPOINT_IOS TRUE)
endif()

if(DEFINED SDL2_ROOT)
  set(CMAKE_PREFIX_PATH "${SDL2_ROOT};${CMAKE_PREFIX_PATH}")
endif()

# iOS has no system SDL2 to find, so it is built from source as a subproject.
# SDL2_SOURCE_DIR points at an SDL2 (not SDL3) checkout; SDL2's own CMake build
# produces the static library and the iOS main shim the app links against.
set(CROSSPOINT_SDL2_FROM_SOURCE FALSE)
if(DEFINED SDL2_SOURCE_DIR)
  if(NOT EXISTS "${SDL2_SOURCE_DIR}/CMakeLists.txt")
    message(FATAL_ERROR "SDL2_SOURCE_DIR does not look like an SDL2 checkout: ${SDL2_SOURCE_DIR}")
  endif()
  set(SDL_SHARED OFF CACHE BOOL "" FORCE)
  set(SDL_STATIC ON CACHE BOOL "" FORCE)
  set(SDL_TEST OFF CACHE BOOL "" FORCE)
  add_subdirectory("${SDL2_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/sdl2-build" EXCLUDE_FROM_ALL)
  set(CROSSPOINT_SDL2_FROM_SOURCE TRUE)
  set(SDL2_FOUND TRUE)
elseif(CROSSPOINT_IOS)
  message(FATAL_ERROR "iOS builds need -DSDL2_SOURCE_DIR=/path/to/SDL2 (an SDL2 source checkout).")
endif()

set(SDL2_USE_PKGCONFIG OFF)
if(NOT CROSSPOINT_SDL2_FROM_SOURCE)
find_package(SDL2 QUIET)
if(NOT SDL2_FOUND)
  find_package(PkgConfig QUIET)
  if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 sdl2)
    if(SDL2_FOUND)
      set(SDL2_USE_PKGCONFIG ON)
    endif()
  endif()
endif()
endif()
if(NOT SDL2_FOUND)
  message(FATAL_ERROR "SDL2 not found. Need SDL2 (not SDL3), or -DSDL2_SOURCE_DIR=/path/to/SDL2.")
endif()

if(EXISTS "${CROSSPOINT_ROOT}/scripts/build_html.py")
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "PYTHONPATH=" python3 "${CROSSPOINT_ROOT}/scripts/build_html.py"
    WORKING_DIRECTORY "${CROSSPOINT_ROOT}"
    OUTPUT_QUIET
  )
endif()

set(SIM_SOURCES
  sim/src/main_sim.cpp
  sim/src/sim_display.cpp
  sim/src/sim_window.cpp
  sim/src/sim_gpio.cpp
  sim/src/sim_storage.cpp
  sim/src/sim_spi_bus.cpp
  sim/src/arduino_stub.cpp
  sim/src/esp_stub.cpp
  sim/src/battery_stub.cpp
  sim/src/frontlight_stub.cpp
  sim/src/xteink_detect_stub.cpp
  sim/src/wifi_stub.cpp
  sim/src/freertos_stub.cpp
  sim/src/mdns_stub.cpp
  sim/src/ota_updater_stub.cpp
  sim/src/http_downloader_stub.cpp
  sim/src/image_to_bmp.cpp
  sim/src/crosspoint_web_server_stub.cpp
  sim/src/image_decoder_stubs.cpp
  sim/src/jpeg_to_bmp_converter_stub.cpp
  sim/src/fork_compat_stubs.cpp
  sim/src/excluded_activity_stubs.cpp
  sim/src/translation_activity_sim.cpp
)

if(CROSSPOINT_IOS)
  list(APPEND SIM_SOURCES sim/ios/sim_ios_platform.m)
else()
  list(APPEND SIM_SOURCES sim/src/sim_platform_desktop.cpp)
endif()

file(GLOB_RECURSE CROSSPOINT_SRC "${CROSSPOINT_ROOT}/src/*.cpp")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/network/OtaUpdater\\.cpp$")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/network/HttpDownloader\\.cpp$")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/network/CrossPointWebServer\\.cpp$")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/network/WebDAVHandler\\.cpp$")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/JsonSettingsIO\\.cpp$")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/EpubReaderTranslationActivity\\.cpp$")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/WifiSelectionActivity\\.cpp$")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/FontDownloadActivity\\.cpp$")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/ClockSyncActivity\\.cpp$")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/KOReaderSyncActivity\\.cpp$")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/KOReaderAuthActivity\\.cpp$")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/OtaUpdateActivity\\.cpp$")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/OpdsBookBrowserActivity\\.cpp$")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/CalibreConnectActivity\\.cpp$")
list(FILTER CROSSPOINT_SRC EXCLUDE REGEX ".*/CrossPointWebServerActivity\\.cpp$")

file(GLOB CROSSPOINT_LIB_CPP
  "${CROSSPOINT_ROOT}/lib/GfxRenderer/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Epub/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Epub/Epub/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Epub/Epub/*/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Epub/Epub/*/*/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Txt/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Xtc/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Xtc/Xtc/*.cpp"
  "${CROSSPOINT_ROOT}/lib/EpdFont/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Utf8/*.cpp"
  "${CROSSPOINT_ROOT}/lib/FsHelpers/*.cpp"
  "${CROSSPOINT_ROOT}/lib/ZipFile/*.cpp"
  "${CROSSPOINT_ROOT}/lib/InflateReader/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Logging/*.cpp"
  "${CROSSPOINT_ROOT}/lib/I18n/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Dict/*.cpp"
  "${CROSSPOINT_ROOT}/lib/MiniBidi/*.cpp"
  "${CROSSPOINT_ROOT}/lib/MiniBidi/*.c"
  "${CROSSPOINT_ROOT}/lib/miniz/*.c"
  "${CROSSPOINT_ROOT}/lib/expat/*.c"
  "${CROSSPOINT_ROOT}/lib/picojpeg/*.c"
  "${CROSSPOINT_ROOT}/lib/uzlib/src/*.c"
  "${CROSSPOINT_ROOT}/lib/KOReaderSync/*.cpp"
  "${CROSSPOINT_ROOT}/lib/OpdsParser/*.cpp"
  "${CROSSPOINT_ROOT}/lib/MangaPanel/*.cpp"
  "${CROSSPOINT_ROOT}/lib/BmpToBmpConverter/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Memory/*.cpp"
  "${CROSSPOINT_ROOT}/lib/Serialization/*.cpp"
  "${CROSSPOINT_ROOT}/lib/miniz/src/*.cpp"
  "${CROSSPOINT_ROOT}/lib/miniz/src/*.c"
  "${CROSSPOINT_ROOT}/lib/OpdsParser/*/*.cpp"
  "${FREEINK_SDK_ROOT}/libs/ui/FreeInkUI/src/*.cpp"
)
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX ".*/hal/.*")
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX ".*/JpegToFramebufferConverter\\.cpp$")
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX ".*/PngToFramebufferConverter\\.cpp$")
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX ".*/KOReaderJsonIO\\.cpp$")
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX ".*/KOReaderSyncClient\\.cpp$")
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX ".*/Serialization/ObfuscationUtils\\.cpp$")

set(ARDUINO_JSON_ROOT "${CROSSPOINT_ROOT}/.pio/libdeps/default/ArduinoJson")
if(EXISTS "${ARDUINO_JSON_ROOT}/src/ArduinoJson.h")
  file(GLOB ARDUINO_JSON_SOURCES "${ARDUINO_JSON_ROOT}/src/*.cpp")
  list(APPEND CROSSPOINT_LIB_CPP ${ARDUINO_JSON_SOURCES})
endif()

set(IOS_RESOURCES)
if(CROSSPOINT_IOS)
  set(IOS_RESOURCES "${CMAKE_CURRENT_SOURCE_DIR}/sim/ios/LaunchScreen.storyboard")
endif()

add_executable(crosspoint_emulator MACOSX_BUNDLE
  ${SIM_SOURCES}
  ${CROSSPOINT_SRC}
  ${CROSSPOINT_LIB_CPP}
  ${IOS_RESOURCES}
)

# MACOSX_BUNDLE only means anything on Apple platforms; elsewhere it is ignored
# and the target stays a plain executable.
if(CROSSPOINT_IOS)
  set_source_files_properties(${IOS_RESOURCES} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
  set(MACOSX_BUNDLE_GUI_IDENTIFIER "org.crosspoint.emulator" CACHE STRING "iOS bundle identifier")
  set(MACOSX_BUNDLE_BUNDLE_NAME "Crosspoint")
  set(MACOSX_BUNDLE_SHORT_VERSION_STRING "0.16.0")
  set(MACOSX_BUNDLE_BUNDLE_VERSION "1")
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/sim/ios/Info.plist.in" "${CMAKE_BINARY_DIR}/Info.plist" @ONLY)
  set_target_properties(crosspoint_emulator PROPERTIES
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_BINARY_DIR}/Info.plist"
    XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "1,2"          # iPhone and iPad
    XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "13.0"     # UIScene safe-area API
    XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
    XCODE_ATTRIBUTE_CLANG_ENABLE_OBJC_ARC "YES"
  )
endif()

file(GLOB LIB_DIRS "${CROSSPOINT_ROOT}/lib/*/")
file(GLOB FREEINK_INCLUDE_DIRS "${FREEINK_SDK_ROOT}/libs/*/*/include")
set(LIB_INCLUDES "${CROSSPOINT_ROOT}/src" "${CROSSPOINT_ROOT}/lib")
foreach(LIB_DIR ${LIB_DIRS})
  list(APPEND LIB_INCLUDES "${LIB_DIR}")
endforeach()

target_include_directories(crosspoint_emulator PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/sim/include
  ${LIB_INCLUDES}
  ${CROSSPOINT_ROOT}/lib/Epub/Epub
  ${CROSSPOINT_ROOT}/lib/Epub/Epub/parsers
  ${CROSSPOINT_ROOT}/lib/Epub/Epub/hyphenation
  ${CROSSPOINT_ROOT}/lib/uzlib/src
  ${CROSSPOINT_ROOT}/lib/miniz/src
  ${FREEINK_INCLUDE_DIRS}
)

if(EXISTS "${ARDUINO_JSON_ROOT}/src")
  target_include_directories(crosspoint_emulator PRIVATE ${ARDUINO_JSON_ROOT}/src)
endif()

if(CROSSPOINT_SDL2_FROM_SOURCE)
  # SDL2main supplies the iOS entry point that stands up UIApplication before
  # calling SDL_main (main_sim.cpp's main(), renamed by SDL_main.h).
  target_link_libraries(crosspoint_emulator PRIVATE SDL2::SDL2main SDL2::SDL2-static)
elseif(SDL2_USE_PKGCONFIG)
  target_include_directories(crosspoint_emulator PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(crosspoint_emulator PRIVATE ${SDL2_LIBRARIES})
  target_link_directories(crosspoint_emulator PRIVATE ${SDL2_LIBRARY_DIRS})
else()
  target_link_libraries(crosspoint_emulator PRIVATE SDL2::SDL2)
endif()

if(CROSSPOINT_IOS)
  target_link_libraries(crosspoint_emulator PRIVATE "-framework UIKit" "-framework Foundation")
endif()

# libcurl — the emulator's native translation activity calls Gemini over HTTPS
# with it. Optional: iOS ships no linkable libcurl, and the activity reports the
# missing client rather than the build failing (see translation_activity_sim.cpp).
if(CROSSPOINT_IOS)
  set(CROSSPOINT_ENABLE_CURL_DEFAULT OFF)
else()
  set(CROSSPOINT_ENABLE_CURL_DEFAULT ON)
endif()
option(CROSSPOINT_ENABLE_CURL "Build the translation activity's HTTP client" ${CROSSPOINT_ENABLE_CURL_DEFAULT})

if(CROSSPOINT_ENABLE_CURL)
  find_package(CURL REQUIRED)
  target_link_libraries(crosspoint_emulator PRIVATE CURL::libcurl)
  target_compile_definitions(crosspoint_emulator PRIVATE CROSSPOINT_EMU_HAVE_CURL=1)
else()
  target_compile_definitions(crosspoint_emulator PRIVATE CROSSPOINT_EMU_HAVE_CURL=0)
endif()

target_compile_definitions(crosspoint_emulator PRIVATE
  CROSSPOINT_EMULATED=1
  PROGMEM=
  EINK_DISPLAY_SINGLE_BUFFER_MODE=1
  CROSSPOINT_VERSION="0.16.0-emu"
  MINIZ_NO_ZLIB_COMPATIBLE_NAMES=1
  DISABLE_FS_H_WARNING=1
  XML_GE=0
  XML_CONTEXT_BYTES=1024
  USE_UTF8_LONG_NAMES=1
  ENABLE_SERIAL_LOG=1
  LOG_LEVEL=2
  ${CROSSPOINT_DEVICE_DEFINE}
  # Log transport: the emulator writes through Serial (-> stdout), not the ROM
  # printf the C3 uses before USB CDC enumerates. These were defined by the
  # emulator's own BoardConfig shim before it was replaced by the SDK header.
  FREEINK_LOG_TRANSPORT_ROM_PRINTF=0
  FREEINK_LOG_TRANSPORT_SERIAL=1
  FREEINK_LOG_TRANSPORT=1
)

if(CROSSPOINT_IOS)
  target_compile_definitions(crosspoint_emulator PRIVATE SIM_PLATFORM_IOS=1)
endif()


target_compile_options(crosspoint_emulator PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-include;cassert>)
