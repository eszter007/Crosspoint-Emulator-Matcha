#include <ArduinoJson.h>
#include <HalStorage.h>
#include <JsonSettingsIO.h>
#include <Logging.h>

#include "BookmarkEntry.h"

namespace JsonSettingsIO {

bool saveSettings(const CrossPointSettings&, const char*) { return true; }
bool loadSettings(CrossPointSettings&, const char*, bool*) { return false; }

bool saveState(const CrossPointState&, const char*) { return true; }
bool loadState(CrossPointState&, const char*) { return false; }

bool saveWifi(const WifiCredentialStore&, const char*) { return true; }
bool loadWifi(WifiCredentialStore&, const char*, bool*) { return false; }

bool saveRecentBooks(const RecentBooksStore&, const char*) { return true; }
bool loadRecentBooks(RecentBooksStore&, const char*) { return false; }

bool saveOpds(const OpdsServerStore&, const char*) { return true; }
bool loadOpds(OpdsServerStore&, const char*, bool*) { return false; }

// Real implementations (mirrors src/JsonSettingsIO.cpp) — bookmarks don't touch
// ObfuscationUtils (ESP32 hardware MAC only), so this is safe to build for the sim.
bool saveBookmarks(const std::vector<BookmarkEntry>& bookmarks, const char* path) {
  JsonDocument doc;
  JsonArray arr = doc["bookmarks"].to<JsonArray>();
  for (const auto& bookmark : bookmarks) {
    JsonObject obj = arr.add<JsonObject>();
    obj["xpath"] = bookmark.xpath;
    obj["percentage"] = bookmark.percentage;
    obj["summary"] = bookmark.summary;
    obj["si"] = bookmark.computedSpineIndex;
    obj["pc"] = bookmark.computedChapterPageCount;
    obj["pp"] = bookmark.computedChapterProgress;
  }

  String json;
  serializeJson(doc, json);
  return Storage.writeFile(path, json);
}

bool loadBookmarks(std::vector<BookmarkEntry>& bookmarks, const char* json) {
  JsonDocument doc;
  auto error = deserializeJson(doc, json);
  if (error) {
    LOG_ERR("BKM", "JSON parse error: %s", error.c_str());
    return false;
  }

  JsonArray arr = doc["bookmarks"].as<JsonArray>();
  bookmarks.clear();
  bookmarks.reserve(arr.size());
  for (JsonObject obj : arr) {
    bookmarks.emplace_back();
    auto& bookmark = bookmarks.back();
    bookmark.xpath = obj["xpath"] | std::string("");
    bookmark.percentage = obj["percentage"] | static_cast<float>(0);
    bookmark.summary = obj["summary"] | std::string("");
    bookmark.computedSpineIndex = obj["si"] | static_cast<uint16_t>(0);
    bookmark.computedChapterPageCount = obj["pc"] | static_cast<uint16_t>(0);
    bookmark.computedChapterProgress = obj["pp"] | static_cast<uint16_t>(0);
  }
  return true;
}

}  // namespace JsonSettingsIO
