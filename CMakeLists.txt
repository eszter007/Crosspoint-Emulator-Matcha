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

if(DEFINED SDL2_ROOT)
  set(CMAKE_PREFIX_PATH "${SDL2_ROOT};${CMAKE_PREFIX_PATH}")
endif()

set(SDL2_USE_PKGCONFIG OFF)
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
if(NOT SDL2_FOUND)
  message(FATAL_ERROR "SDL2 not found. Need SDL2 (not SDL3).")
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
  sim/src/sim_gpio.cpp
  sim/src/sim_storage.cpp
  sim/src/sim_spi_bus.cpp
  sim/src/arduino_stub.cpp
  sim/src/esp_stub.cpp
  sim/src/battery_stub.cpp
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
  sim/src/json_settings_stub.cpp
  sim/src/excluded_activity_stubs.cpp
  sim/src/translation_activity_sim.cpp
)

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
  "${CROSSPOINT_ROOT}/lib/OpdsParser/*/*.cpp"
)
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX ".*/hal/.*")
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX ".*/JpegToFramebufferConverter\\.cpp$")
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX ".*/PngToFramebufferConverter\\.cpp$")
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX ".*/KOReaderJsonIO\\.cpp$")
list(FILTER CROSSPOINT_LIB_CPP EXCLUDE REGEX ".*/KOReaderSyncClient\\.cpp$")

set(ARDUINO_JSON_ROOT "${CROSSPOINT_ROOT}/.pio/libdeps/default/ArduinoJson")
if(EXISTS "${ARDUINO_JSON_ROOT}/src/ArduinoJson.h")
  file(GLOB ARDUINO_JSON_SOURCES "${ARDUINO_JSON_ROOT}/src/*.cpp")
  list(APPEND CROSSPOINT_LIB_CPP ${ARDUINO_JSON_SOURCES})
endif()

add_executable(crosspoint_emulator
  ${SIM_SOURCES}
  ${CROSSPOINT_SRC}
  ${CROSSPOINT_LIB_CPP}
)

file(GLOB LIB_DIRS "${CROSSPOINT_ROOT}/lib/*/")
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
)

if(EXISTS "${ARDUINO_JSON_ROOT}/src")
  target_include_directories(crosspoint_emulator PRIVATE ${ARDUINO_JSON_ROOT}/src)
endif()

if(SDL2_USE_PKGCONFIG)
  target_include_directories(crosspoint_emulator PRIVATE ${SDL2_INCLUDE_DIRS})
  target_link_libraries(crosspoint_emulator PRIVATE ${SDL2_LIBRARIES})
  target_link_directories(crosspoint_emulator PRIVATE ${SDL2_LIBRARY_DIRS})
else()
  target_link_libraries(crosspoint_emulator PRIVATE SDL2::SDL2)
endif()

# libcurl — used by the emulator's native translation activity (desktop HTTPS)
find_package(CURL REQUIRED)
target_link_libraries(crosspoint_emulator PRIVATE CURL::libcurl)

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
)


target_compile_options(crosspoint_emulator PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-include;cassert>)
