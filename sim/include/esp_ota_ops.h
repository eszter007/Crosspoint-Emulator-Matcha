#pragma once

#include "esp_partition.h"

inline const esp_partition_t* esp_ota_get_next_update_partition(const void*) {
  return esp_partition_find_first(ESP_PARTITION_TYPE_APP, ESP_PARTITION_SUBTYPE_APP_OTA_0, nullptr);
}
