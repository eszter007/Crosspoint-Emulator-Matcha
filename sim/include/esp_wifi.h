#pragma once

#define WIFI_MODE_STA 1
#define WIFI_MODE_AP 2

inline int esp_wifi_stop() { return 0; }
inline int esp_wifi_start() { return 0; }
inline int esp_wifi_set_mode(int) { return 0; }
inline int esp_wifi_set_ps(int) { return 0; }
