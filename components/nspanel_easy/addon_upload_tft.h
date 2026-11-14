// addon_upload_tft.h - Complete TFT component state management

#pragma once

#ifdef NSPANEL_EASY_ADDON_UPLOAD_TFT

#include <cstdint>

namespace esphome::nspanel_easy {

// TFT upload state variables (previously YAML globals)
extern uint8_t tft_upload_attempt;
extern bool tft_upload_manual_request;
extern uint32_t tft_upload_first_time_synced_ms;
extern bool tft_upload_result;

/// Automatic uploads allowed per firmware version when the TFT version only differs from the firmware version
static constexpr uint8_t TFT_AUTO_UPLOAD_MAX_ATTEMPTS = 3;

/**
 * @brief Check whether the TFT may still be updated automatically for a version difference.
 *
 * The automatic update replaces a TFT whose version differs from the firmware version. A TFT that never
 * matches (e.g. a custom TFT, or a custom URL serving another version) would then be uploaded on every boot,
 * so the uploads are counted per firmware version, in flash, and stop after a successful upload or after
 * TFT_AUTO_UPLOAD_MAX_ATTEMPTS attempts. A firmware update starts a new count.
 *
 * Not used when the TFT is below the minimum version required by the firmware: that update is always required.
 *
 * @param firmware_version The running firmware version.
 * @return true if another automatic upload is allowed for this firmware version.
 */
bool tft_auto_upload_allowed(const char *firmware_version);

/**
 * @brief Register a TFT upload for the given firmware version, in flash.
 *
 * Flushed to flash immediately, as a reboot follows every upload.
 *
 * @param firmware_version The running firmware version.
 * @param completed        true for a successful upload (automatic or manual), which ends the automatic
 *                         updates for this firmware version; false for the start of an automatic upload,
 *                         which counts one attempt.
 */
void tft_auto_upload_register(const char *firmware_version, bool completed);

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_ADDON_UPLOAD_TFT
