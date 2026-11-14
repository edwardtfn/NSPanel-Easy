// addon_upload_tft.cpp - TFT component state definitions

#ifdef NSPANEL_EASY_ADDON_UPLOAD_TFT

#include "addon_upload_tft.h"

#include <cinttypes>

#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

namespace esphome::nspanel_easy {

static const char *const TAG = "nspanel.addon.upload_tft";

// TFT upload state variables (previously YAML globals)
uint8_t tft_upload_attempt = 0;
bool tft_upload_manual_request = false;
uint32_t tft_upload_first_time_synced_ms = 0;
bool tft_upload_result = false;

/// Automatic uploads registered for one firmware version, persisted in flash
struct TftAutoUploadRecord {
  uint32_t firmware_hash;  ///< fnv1_hash of the firmware version the uploads belong to
  uint8_t attempts;        ///< Uploads started; TFT_AUTO_UPLOAD_MAX_ATTEMPTS once one succeeded
};

/**
 * @brief Preference handle for the persisted automatic upload record.
 *
 * @return Preference object for the record.
 */
static ESPPreferenceObject tft_auto_upload_pref() {
  return global_preferences->make_preference<TftAutoUploadRecord>(fnv1_hash("nspanel_easy_tft_auto_upload"));
}

/**
 * @brief Uploads registered for a firmware version.
 *
 * @param firmware_hash fnv1_hash of the firmware version.
 * @return The registered attempts, or 0 when nothing is stored for this firmware version.
 */
static uint8_t tft_auto_upload_attempts(uint32_t firmware_hash) {
  TftAutoUploadRecord record{};
  if (!tft_auto_upload_pref().load(&record) || record.firmware_hash != firmware_hash) {
    return 0;  // Nothing stored, or stored for another firmware version
  }
  return record.attempts;
}

bool tft_auto_upload_allowed(const char *firmware_version) {
  return tft_auto_upload_attempts(fnv1_hash(firmware_version)) < TFT_AUTO_UPLOAD_MAX_ATTEMPTS;
}

void tft_auto_upload_register(const char *firmware_version, bool completed) {
  TftAutoUploadRecord record{};
  record.firmware_hash = fnv1_hash(firmware_version);
  const uint8_t attempts = tft_auto_upload_attempts(record.firmware_hash);
  if (completed || attempts >= TFT_AUTO_UPLOAD_MAX_ATTEMPTS) {
    record.attempts = TFT_AUTO_UPLOAD_MAX_ATTEMPTS;
  } else {
    record.attempts = static_cast<uint8_t>(attempts + 1);
  }  // if (completed || attempts >= TFT_AUTO_UPLOAD_MAX_ATTEMPTS)

  // A reboot follows every upload, so the record is written to flash now
  if (!tft_auto_upload_pref().save(&record) || !global_preferences->sync()) {
    ESP_LOGW(TAG, "TFT upload record not stored");
    return;
  }
  ESP_LOGD(TAG, "TFT upload registered for firmware %s: %" PRIu8 "/%" PRIu8, firmware_version, record.attempts,
           TFT_AUTO_UPLOAD_MAX_ATTEMPTS);
}

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_ADDON_UPLOAD_TFT
