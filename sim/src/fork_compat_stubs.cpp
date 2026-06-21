#include <HalClock.h>
#include <HalPowerManager.h>
#include <HalSystem.h>
#include <HalTiltSensor.h>
#include <Logging.h>
#include <ObfuscationUtils.h>
#include <PngToBmpConverter.h>

#include <cstdint>
#include <cstdarg>
#include <cstdio>

HalPowerManager powerManager;
HalClock halClock;
HalTiltSensor halTiltSensor;
MySerialImpl MySerialImpl::instance;

size_t MySerialImpl::printf(const char* format, ...) {
  va_list args;
  va_start(args, format);
  char buffer[512];
  const int n = std::vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  if (n <= 0) {
    return 0;
  }
  const size_t len = static_cast<size_t>(n < static_cast<int>(sizeof(buffer)) ? n : static_cast<int>(sizeof(buffer) - 1));
  return logSerial.write(reinterpret_cast<const uint8_t*>(buffer), len);
}

size_t MySerialImpl::write(uint8_t b) { return logSerial.write(b); }

size_t MySerialImpl::write(const uint8_t* buffer, size_t size) { return logSerial.write(buffer, size); }

void MySerialImpl::flush() { logSerial.flush(); }

void HalPowerManager::begin() {}

void HalPowerManager::setPowerSaving(bool enabled) { isLowPower = enabled; }

void HalPowerManager::startDeepSleep(HalGPIO&) const {}

uint16_t HalPowerManager::getBatteryPercentage() const { return 100; }

HalPowerManager::Lock::Lock() : valid(true) {}

HalPowerManager::Lock::~Lock() = default;

void HalClock::begin() {
  _available = true;
  _hasCachedTime = false;
  _lastPollMs = 0;
}

bool HalClock::getTime(uint8_t& hour, uint8_t& minute) const {
  if (!_available) {
    return false;
  }
  const unsigned long nowMs = millis();
  const uint32_t totalMinutes = static_cast<uint32_t>(nowMs / 60000UL) % (24U * 60U);
  hour = static_cast<uint8_t>(totalMinutes / 60U);
  minute = static_cast<uint8_t>(totalMinutes % 60U);
  _cachedHour = hour;
  _cachedMinute = minute;
  _hasCachedTime = true;
  _lastPollMs = nowMs;
  return true;
}

bool HalClock::formatTime(char* buf, size_t bufSize, uint8_t utcOffsetQuarterHoursBiased, bool use12Hour) const {
  if (!buf || bufSize == 0) {
    return false;
  }

  uint8_t hour = 0;
  uint8_t minute = 0;
  if (!getTime(hour, minute)) {
    buf[0] = '\0';
    return false;
  }

  const int offsetQuarterHours = static_cast<int>(utcOffsetQuarterHoursBiased) - 48;
  const int offsetMinutes = offsetQuarterHours * 15;
  int localTotalMinutes = static_cast<int>(hour) * 60 + static_cast<int>(minute) + offsetMinutes;
  localTotalMinutes %= (24 * 60);
  if (localTotalMinutes < 0) {
    localTotalMinutes += 24 * 60;
  }
  hour = static_cast<uint8_t>(localTotalMinutes / 60);
  minute = static_cast<uint8_t>(localTotalMinutes % 60);

  if (use12Hour) {
    const bool isPm = hour >= 12;
    uint8_t displayHour = hour % 12;
    if (displayHour == 0) {
      displayHour = 12;
    }
    std::snprintf(buf, bufSize, "%u:%02u %s", static_cast<unsigned>(displayHour), static_cast<unsigned>(minute),
                  isPm ? "PM" : "AM");
  } else {
    std::snprintf(buf, bufSize, "%02u:%02u", static_cast<unsigned>(hour), static_cast<unsigned>(minute));
  }

  return true;
}

bool HalClock::syncFromNTP() { return _available; }

bool HalClock::writeTimeToRTC(uint8_t, uint8_t, uint8_t) { return true; }

void HalTiltSensor::begin() { _available = false; }

bool HalTiltSensor::wake() {
  _isAwake = true;
  _wakeMs = millis();
  return true;
}

bool HalTiltSensor::deepSleep() {
  _isAwake = false;
  return true;
}

void HalTiltSensor::update(const uint8_t, const uint8_t, const bool) {}

bool HalTiltSensor::wasTiltedForward() {
  const bool event = _tiltForwardEvent;
  _tiltForwardEvent = false;
  return event;
}

bool HalTiltSensor::wasTiltedBack() {
  const bool event = _tiltBackEvent;
  _tiltBackEvent = false;
  return event;
}

bool HalTiltSensor::hadActivity() {
  const bool had = _hadActivity;
  _hadActivity = false;
  return had;
}

void HalTiltSensor::clearPendingEvents() {
  _tiltForwardEvent = false;
  _tiltBackEvent = false;
  _hadActivity = false;
}

namespace HalSystem {
void begin() {}

void checkPanic() {}

void clearPanic() {}

std::string getPanicInfo(bool) { return {}; }

bool isRebootFromPanic() { return false; }
}  // namespace HalSystem

bool PngToBmpConverter::pngFileToBmpStreamInternal(HalFile&, Print&, int, int, bool, bool) { return false; }

bool PngToBmpConverter::pngFileToBmpStream(HalFile& pngFile, Print& bmpOut, bool crop) {
  return pngFileToBmpStreamInternal(pngFile, bmpOut, -1, -1, false, crop);
}

bool PngToBmpConverter::pngFileToBmpStreamWithSize(HalFile& pngFile, Print& bmpOut, int targetMaxWidth,
                                                   int targetMaxHeight) {
  return pngFileToBmpStreamInternal(pngFile, bmpOut, targetMaxWidth, targetMaxHeight, false, true);
}

bool PngToBmpConverter::pngFileTo1BitBmpStreamWithSize(HalFile& pngFile, Print& bmpOut, int targetMaxWidth,
                                                       int targetMaxHeight) {
  return pngFileToBmpStreamInternal(pngFile, bmpOut, targetMaxWidth, targetMaxHeight, true, true);
}

extern "C" uint32_t uzlib_adler32(const void*, unsigned int, uint32_t prev_sum) { return prev_sum; }

extern "C" uint32_t uzlib_crc32(const void*, unsigned int, uint32_t crc) { return crc; }

namespace obfuscation {
String obfuscateToBase64(const std::string& plaintext) { return String(plaintext.c_str()); }

std::string deobfuscateFromBase64(const char* encoded, bool* ok) {
  if (ok) {
    *ok = true;
  }
  return encoded ? std::string(encoded) : std::string();
}
}  // namespace obfuscation
