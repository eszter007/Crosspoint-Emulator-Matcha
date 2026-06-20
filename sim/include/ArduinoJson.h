#pragma once

// Minimal ArduinoJson stub for emulator build
// Real ArduinoJson is provided by PlatformIO if web server screens are needed

namespace ArduinoJson {

class JsonDocument {
 public:
  JsonDocument() {}
  ~JsonDocument() {}
  bool containsKey(const char* key) const { return false; }
  bool isNull() const { return true; }
  bool operator[](const char* key) { return false; }
};

}  // namespace ArduinoJson

// Top-level aliases
using DynamicJsonDocument = ArduinoJson::JsonDocument;

