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

#include <KOReaderJsonIO.h>
#include <I18n.h>
#include "components/UITheme.h"
#include "fontIds.h"

namespace KOReaderJsonIO {
bool save(const KOReaderCredentialStore&, const char*) { return true; }
bool load(KOReaderCredentialStore&, const char*, bool*) { return false; }
}

// Stubs for activities excluded from emulator (ArduinoJson/WiFi/HTTP deps)

void ClockSyncActivity::onEnter() { Activity::onEnter(); }
void ClockSyncActivity::onExit() { Activity::onExit(); }
void ClockSyncActivity::loop() {}
void ClockSyncActivity::render(RenderLock&&) {}
void ClockSyncActivity::runSync() {}

void CrossPointWebServerActivity::loop() {}
void CrossPointWebServerActivity::render(RenderLock&&) {}
void CrossPointWebServerActivity::onEnter() { Activity::onEnter(); }
void CrossPointWebServerActivity::onExit() { Activity::onExit(); }

void KOReaderAuthActivity::loop() {}
void KOReaderAuthActivity::render(RenderLock&&) {}
void KOReaderAuthActivity::onWifiSelectionComplete(bool) {}
void KOReaderAuthActivity::performAuthentication() {}
void KOReaderAuthActivity::onEnter() { Activity::onEnter(); }
void KOReaderAuthActivity::onExit() { Activity::onExit(); }

void KOReaderSyncActivity::onEnter() { Activity::onEnter(); }
void KOReaderSyncActivity::onExit() { Activity::onExit(); }
void KOReaderSyncActivity::loop() {}
void KOReaderSyncActivity::render(RenderLock&&) {}

void OpdsBookBrowserActivity::loop() {}
void OpdsBookBrowserActivity::render(RenderLock&&) {}
void OpdsBookBrowserActivity::onEnter() { Activity::onEnter(); }
void OpdsBookBrowserActivity::onExit() { Activity::onExit(); }

void OtaUpdateActivity::onEnter() { Activity::onEnter(); }
void OtaUpdateActivity::onExit() { Activity::onExit(); }
void OtaUpdateActivity::loop() {}
void OtaUpdateActivity::render(RenderLock&&) {}

void WifiSelectionActivity::onEnter() { Activity::onEnter(); }
void WifiSelectionActivity::onExit() { Activity::onExit(); }
void WifiSelectionActivity::loop() {}
void WifiSelectionActivity::render(RenderLock&&) {}

// EpubReaderTranslationActivity is implemented natively in translation_activity_sim.cpp


void CalibreConnectActivity::loop() {}
void CalibreConnectActivity::render(RenderLock&&) {}
void CalibreConnectActivity::onEnter() { Activity::onEnter(); }
void CalibreConnectActivity::onExit() { Activity::onExit(); }

static SdCardFontRegistry dummyRegistry;
FontDownloadActivity::FontDownloadActivity(GfxRenderer& r, MappedInputManager& m)
    : Activity("FontDownload", r, m), fontInstaller_(dummyRegistry) {}
void FontDownloadActivity::onEnter() { Activity::onEnter(); }
void FontDownloadActivity::onExit() { Activity::onExit(); }
void FontDownloadActivity::loop() {}
void FontDownloadActivity::render(RenderLock&&) {}
