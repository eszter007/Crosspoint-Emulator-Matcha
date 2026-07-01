// Native (desktop emulator) implementation of EpubReaderTranslationActivity.
//
// The real firmware implementation (crosspoint-reader/src/.../EpubReaderTranslationActivity.cpp)
// uses ESP32 WiFi + esp_http_client + ArduinoJson and runs on the Xteink device.
// That file is excluded from the emulator build via CMake; THIS file is compiled
// instead so the feature can be tested on the desktop using libcurl.
//
// The desktop already has internet, so WiFi selection is skipped. The API key is
// read from /gemini.key on the simulated SD card, exactly like the device.

#include "activities/reader/EpubReaderTranslationActivity.h"

#include <curl/curl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

#include <GfxRenderer.h>
#include <HalStorage.h>
#include <I18n.h>

#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace {

constexpr const char* API_KEY_PATH = "/system/gemini.key";
constexpr const char* GEMINI_MODEL = "gemini-2.5-flash";

// Single-instance helper state (only one translation activity exists at a time).
bool s_spinnerRendered = false;
bool s_callStarted = false;
std::string s_apiKey;

size_t curlWriteCb(char* ptr, size_t size, size_t nmemb, void* userdata) {
  auto* s = static_cast<std::string*>(userdata);
  s->append(ptr, size * nmemb);
  return size * nmemb;
}

std::string jsonEscape(const std::string& in) {
  std::string out;
  out.reserve(in.size() + 16);
  for (unsigned char c : in) {
    switch (c) {
      case '"': out += "\\\""; break;
      case '\\': out += "\\\\"; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default:
        if (c < 0x20) {
          char buf[8];
          snprintf(buf, sizeof(buf), "\\u%04x", c);
          out += buf;
        } else {
          out += static_cast<char>(c);
        }
    }
  }
  return out;
}

// Extract the first JSON string value following a "text": key, decoding escapes.
bool extractGeminiText(const std::string& resp, std::string& out) {
  size_t p = resp.find("\"text\":");
  if (p == std::string::npos) return false;
  p += 7;
  while (p < resp.size() && (resp[p] == ' ' || resp[p] == '\t' || resp[p] == '\n' || resp[p] == '\r')) p++;
  if (p >= resp.size() || resp[p] != '"') return false;
  p++;
  std::string s;
  while (p < resp.size()) {
    char c = resp[p++];
    if (c == '\\') {
      if (p >= resp.size()) break;
      char e = resp[p++];
      switch (e) {
        case 'n': s += '\n'; break;
        case 't': s += '\t'; break;
        case 'r': s += '\r'; break;
        case '"': s += '"'; break;
        case '\\': s += '\\'; break;
        case '/': s += '/'; break;
        case 'u': {
          if (p + 4 <= resp.size()) {
            long cp = strtol(resp.substr(p, 4).c_str(), nullptr, 16);
            p += 4;
            if (cp < 0x80) {
              s += static_cast<char>(cp);
            } else if (cp < 0x800) {
              s += static_cast<char>(0xC0 | (cp >> 6));
              s += static_cast<char>(0x80 | (cp & 0x3F));
            } else {
              s += static_cast<char>(0xE0 | (cp >> 12));
              s += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
              s += static_cast<char>(0x80 | (cp & 0x3F));
            }
          }
          break;
        }
        default: s += e;
      }
    } else if (c == '"') {
      out = s;
      return true;
    } else {
      s += c;
    }
  }
  return false;
}

}  // namespace

EpubReaderTranslationActivity::EpubReaderTranslationActivity(GfxRenderer& renderer, MappedInputManager& mappedInput,
                                                             std::string sourceText, std::string preTranslatedText)
    : Activity("Translation", renderer, mappedInput), sourceText(std::move(sourceText)) {
  if (!preTranslatedText.empty()) {
    translatedText = std::move(preTranslatedText);
    hasPreTranslation = true;
    state = SHOWING_RESULT;
  }
}

void EpubReaderTranslationActivity::onEnter() {
  Activity::onEnter();

  if (hasPreTranslation) {
    requestUpdate();
    return;
  }

  s_spinnerRendered = false;
  s_callStarted = false;
  s_apiKey.clear();

  // Desktop already has internet — skip WiFi selection, go straight to the key.
  if (!readApiKey(s_apiKey)) {
    errorMessage = tr(STR_TRANSLATION_NO_API_KEY);
    state = ERROR;
    requestUpdate();
    return;
  }

  state = TRANSLATING;
  requestUpdate();
}

void EpubReaderTranslationActivity::onExit() { Activity::onExit(); }

bool EpubReaderTranslationActivity::readApiKey(std::string& keyOut) {
  // Read via HalFile with an explicit length — the sim's readFile() helper is not
  // binary-safe (it builds a String from an un-terminated buffer, appending garbage).
  HalFile file;
  if (!Storage.openFileForRead("XLAT", API_KEY_PATH, file)) return false;
  char buf[256];
  int n = file.read(buf, sizeof(buf) - 1);
  file.close();
  if (n <= 0) return false;

  std::string key(buf, static_cast<size_t>(n));
  // Trim all leading/trailing whitespace and control characters.
  auto isTrim = [](char c) { return c == '\n' || c == '\r' || c == ' ' || c == '\t' || c == '\0'; };
  while (!key.empty() && isTrim(key.back())) key.pop_back();
  size_t start = 0;
  while (start < key.size() && isTrim(key[start])) start++;
  key = key.substr(start);

  if (key.empty()) return false;
  keyOut = std::move(key);
  return true;
}

bool EpubReaderTranslationActivity::callGeminiApi(const std::string& apiKey) {
  std::string url = "https://generativelanguage.googleapis.com/v1beta/models/";
  url += GEMINI_MODEL;
  url += ":generateContent?key=";
  url += apiKey;

  std::string prompt =
      "Translate the following Japanese text to English. "
      "Return only the translation, no commentary.\n\n" +
      sourceText;

  std::string body = "{\"contents\":[{\"parts\":[{\"text\":\"";
  body += jsonEscape(prompt);
  body += "\"}]}],\"generationConfig\":{\"temperature\":0.3,\"maxOutputTokens\":2048}}";

  CURL* curl = curl_easy_init();
  if (!curl) {
    errorMessage = tr(STR_TRANSLATION_FAILED);
    return false;
  }

  std::string resp;
  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");

  curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCb);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &resp);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);

  CURLcode rc = curl_easy_perform(curl);
  long httpCode = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);

  if (rc != CURLE_OK) {
    errorMessage = std::string("Network error: ") + curl_easy_strerror(rc);
    return false;
  }
  if (httpCode != 200) {
    // Surface Google's error message if present (e.g. quota details for 429).
    std::string apiMsg;
    size_t mp = resp.find("\"message\":");
    if (mp != std::string::npos) {
      std::string tail = resp.substr(mp + 10);  // skip past "message":
      extractGeminiText("\"text\":" + tail, apiMsg);  // reuse the JSON-string decoder
    }
    errorMessage = "HTTP " + std::to_string(httpCode);
    if (httpCode == 429) errorMessage += " (rate limit / quota exceeded)";
    if (!apiMsg.empty()) errorMessage += ": " + apiMsg;
    return false;
  }
  if (!extractGeminiText(resp, translatedText)) {
    errorMessage = tr(STR_TRANSLATION_FAILED);
    return false;
  }
  return true;
}

void EpubReaderTranslationActivity::onWifiComplete(bool) {}

void EpubReaderTranslationActivity::loop() {
  if (mappedInput.wasReleased(MappedInputManager::Button::Back)) {
    ActivityResult result;
    result.isCancelled = true;
    setResult(std::move(result));
    finish();
    return;
  }

  // Perform the (blocking) network call only after the spinner has been shown.
  if (state == TRANSLATING && s_spinnerRendered && !s_callStarted) {
    s_callStarted = true;
    if (callGeminiApi(s_apiKey)) {
      state = SHOWING_RESULT;
    } else {
      state = ERROR;
    }
    requestUpdate();
    return;
  }

  if (state == SHOWING_RESULT) {
    buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Down}, [this] {
      if (scrollOffset < maxScrollOffset) {
        scrollOffset++;
        requestUpdate();
      }
    });
    buttonNavigator.onPressAndContinuous({MappedInputManager::Button::Up}, [this] {
      if (scrollOffset > 0) {
        scrollOffset--;
        requestUpdate();
      }
    });
  }
}

void EpubReaderTranslationActivity::render(RenderLock&&) {
  renderer.clearScreen();

  auto& theme = UITheme::getInstance();
  auto metrics = theme.getMetrics();
  Rect screen = theme.getScreenSafeArea(renderer, true, false);

  GUI.drawHeader(renderer, Rect{screen.x, screen.y + metrics.topPadding, screen.width, metrics.headerHeight},
                 tr(STR_TRANSLATE_PAGE));

  const int contentTop = screen.y + metrics.topPadding + metrics.headerHeight + metrics.verticalSpacing;
  const int footerHeight = renderer.getLineHeight(SMALL_FONT_ID) + metrics.verticalSpacing;
  const int contentBottom = screen.y + screen.height - footerHeight;
  const int maxWidth = screen.width - metrics.contentSidePadding * 2;
  const int textX = screen.x + metrics.contentSidePadding;

  if (state == TRANSLATING) {
    UITheme::drawCenteredText(renderer, screen, UI_12_FONT_ID, screen.y + screen.height / 2, tr(STR_TRANSLATING), true);
  } else if (state == ERROR) {
    const int fontId = UI_12_FONT_ID;
    const int lineHeight = renderer.getLineHeight(fontId);
    auto lines = renderer.wrappedText(fontId, errorMessage.c_str(), maxWidth, 16);
    int y = screen.y + screen.height / 2 - (static_cast<int>(lines.size()) * lineHeight) / 2;
    for (const auto& line : lines) {
      const int w = renderer.getTextWidth(fontId, line.c_str());
      renderer.drawText(fontId, screen.x + (screen.width - w) / 2, y, line.c_str(), true);
      y += lineHeight;
    }
    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  } else if (state == SHOWING_RESULT) {
    const int fontId = UI_12_FONT_ID;
    const int lineHeight = renderer.getLineHeight(fontId);

    auto lines = renderer.wrappedText(fontId, translatedText.c_str(), maxWidth, 64);

    maxScrollOffset = static_cast<int>(lines.size()) - (contentBottom - contentTop) / lineHeight;
    if (maxScrollOffset < 0) maxScrollOffset = 0;
    if (scrollOffset > maxScrollOffset) scrollOffset = maxScrollOffset;

    int y = contentTop;
    for (int i = scrollOffset; i < static_cast<int>(lines.size()) && y + lineHeight <= contentBottom; i++) {
      renderer.drawText(fontId, textX, y, lines[i].c_str(), true);
      y += lineHeight;
    }

    if (maxScrollOffset > 0) {
      std::string scrollInfo = std::to_string(scrollOffset + 1) + "/" + std::to_string(maxScrollOffset + 1);
      renderer.drawText(SMALL_FONT_ID, screen.x + screen.width - metrics.contentSidePadding - 40, contentBottom + 2,
                        scrollInfo.c_str(), true);
    }

    const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", tr(STR_DIR_UP), tr(STR_DIR_DOWN));
    GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  }

  renderer.displayBuffer();

  if (state == TRANSLATING) s_spinnerRendered = true;
}
