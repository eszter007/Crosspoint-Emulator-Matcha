#include "activities/settings/ClockSyncActivity.h"
#include "activities/network/CrossPointWebServerActivity.h"
#include "activities/settings/KOReaderAuthActivity.h"
#include "activities/reader/KOReaderSyncActivity.h"
#include "activities/browser/OpdsBookBrowserActivity.h"
#include "activities/settings/OtaUpdateActivity.h"
#include "activities/network/WifiSelectionActivity.h"
#include "activities/reader/EpubReaderTranslationActivity.h"
#include "activities/settings/FontDownloadActivity.h"
#include "activities/network/CalibreConnectActivity.h"

#include <GfxRenderer.h>
#include <KOReaderJsonIO.h>
#include <I18n.h>
#include "MappedInputManager.h"
#include "components/UITheme.h"
#include "fontIds.h"

namespace KOReaderJsonIO {
bool save(const KOReaderCredentialStore&, const char*) { return true; }
bool load(KOReaderCredentialStore&, const char*, bool*) { return false; }
}

// Stubs for activities excluded from emulator (ArduinoJson/WiFi/HTTP deps).
//
// Each stub's render() shows a plain "not available" message and loop()
// accepts Back to exit -- without these, onEnter() logs "Entering activity"
// and returns, then loop()/render() are empty forever: the screen freezes
// on whatever was last drawn with no way out, since Back is never checked.
// This text only ever appears in the emulator (never on real hardware,
// where these activities work normally), so it's a plain string rather
// than an i18n key.
namespace {
void renderNotAvailableInEmulator(GfxRenderer& renderer, MappedInputManager& mappedInput, const char* title) {
  renderer.clearScreen();
  const auto& metrics = UITheme::getInstance().getMetrics();
  const auto pageWidth = renderer.getScreenWidth();
  const auto pageHeight = renderer.getScreenHeight();
  GUI.drawHeader(renderer, Rect{0, metrics.topPadding, pageWidth, metrics.headerHeight}, title);

  const auto lineHeight = renderer.getLineHeight(UI_10_FONT_ID);
  const auto top = (pageHeight - lineHeight * 2) / 2;
  renderer.drawCenteredText(UI_10_FONT_ID, top, "Not available in the emulator");
  renderer.drawCenteredText(SMALL_FONT_ID, top + lineHeight + 6, "(requires WiFi/network hardware)");

  const auto labels = mappedInput.mapLabels(tr(STR_BACK), "", "", "");
  GUI.drawButtonHints(renderer, labels.btn1, labels.btn2, labels.btn3, labels.btn4);
  renderer.displayBuffer();
}

bool backPressedToExit(Activity& activity, MappedInputManager& mappedInput) {
  if (mappedInput.wasPressed(MappedInputManager::Button::Back)) {
    activity.onGoHome();
    return true;
  }
  return false;
}
}  // namespace

void ClockSyncActivity::onEnter() { Activity::onEnter(); requestUpdate(); }
void ClockSyncActivity::onExit() { Activity::onExit(); }
void ClockSyncActivity::loop() { backPressedToExit(*this, mappedInput); }
void ClockSyncActivity::render(RenderLock&&) { renderNotAvailableInEmulator(renderer, mappedInput, "Clock Sync"); }
void ClockSyncActivity::runSync() {}

void CrossPointWebServerActivity::loop() { backPressedToExit(*this, mappedInput); }
void CrossPointWebServerActivity::render(RenderLock&&) {
  renderNotAvailableInEmulator(renderer, mappedInput, tr(STR_FILE_TRANSFER));
}
void CrossPointWebServerActivity::onEnter() { Activity::onEnter(); requestUpdate(); }
void CrossPointWebServerActivity::onExit() { Activity::onExit(); }

void KOReaderAuthActivity::loop() { backPressedToExit(*this, mappedInput); }
void KOReaderAuthActivity::render(RenderLock&&) {
  renderNotAvailableInEmulator(renderer, mappedInput, "KOReader Sync");
}
void KOReaderAuthActivity::onWifiSelectionComplete(bool) {}
void KOReaderAuthActivity::performAuthentication() {}
void KOReaderAuthActivity::onEnter() { Activity::onEnter(); requestUpdate(); }
void KOReaderAuthActivity::onExit() { Activity::onExit(); }

void KOReaderSyncActivity::onEnter() { Activity::onEnter(); requestUpdate(); }
void KOReaderSyncActivity::onExit() { Activity::onExit(); }
void KOReaderSyncActivity::loop() { backPressedToExit(*this, mappedInput); }
void KOReaderSyncActivity::render(RenderLock&&) {
  renderNotAvailableInEmulator(renderer, mappedInput, "KOReader Sync");
}

void OpdsBookBrowserActivity::loop() { backPressedToExit(*this, mappedInput); }
void OpdsBookBrowserActivity::render(RenderLock&&) {
  renderNotAvailableInEmulator(renderer, mappedInput, "OPDS Browser");
}
void OpdsBookBrowserActivity::onEnter() { Activity::onEnter(); requestUpdate(); }
void OpdsBookBrowserActivity::onExit() { Activity::onExit(); }

void OtaUpdateActivity::onEnter() { Activity::onEnter(); requestUpdate(); }
void OtaUpdateActivity::onExit() { Activity::onExit(); }
void OtaUpdateActivity::loop() { backPressedToExit(*this, mappedInput); }
void OtaUpdateActivity::render(RenderLock&&) {
  renderNotAvailableInEmulator(renderer, mappedInput, "Firmware Update");
}

void WifiSelectionActivity::onEnter() { Activity::onEnter(); requestUpdate(); }
void WifiSelectionActivity::onExit() { Activity::onExit(); }
void WifiSelectionActivity::loop() { backPressedToExit(*this, mappedInput); }
void WifiSelectionActivity::render(RenderLock&&) {
  renderNotAvailableInEmulator(renderer, mappedInput, "WiFi Setup");
}

// EpubReaderTranslationActivity is implemented natively in translation_activity_sim.cpp


void CalibreConnectActivity::loop() { backPressedToExit(*this, mappedInput); }
void CalibreConnectActivity::render(RenderLock&&) {
  renderNotAvailableInEmulator(renderer, mappedInput, "Calibre Connect");
}
void CalibreConnectActivity::onEnter() { Activity::onEnter(); requestUpdate(); }
void CalibreConnectActivity::onExit() { Activity::onExit(); }

static SdCardFontRegistry dummyRegistry;
FontDownloadActivity::FontDownloadActivity(GfxRenderer& r, MappedInputManager& m)
    : Activity("FontDownload", r, m), fontInstaller_(dummyRegistry) {}
void FontDownloadActivity::onEnter() { Activity::onEnter(); requestUpdate(); }
void FontDownloadActivity::onExit() { Activity::onExit(); }
void FontDownloadActivity::loop() { backPressedToExit(*this, mappedInput); }
void FontDownloadActivity::render(RenderLock&&) {
  renderNotAvailableInEmulator(renderer, mappedInput, "Font Download");
}
