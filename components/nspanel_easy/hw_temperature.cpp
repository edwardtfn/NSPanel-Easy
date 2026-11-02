// hw_temperature.cpp

#if defined(NSPANEL_EASY_HW_TEMPERATURE) && defined(NSPANEL_EASY_SUBSCRIBE)

#include "hw_temperature.h"

#include <cmath>

#include "esphome/core/hal.h"      // For millis
#include "esphome/core/helpers.h"  // For parse_number and fahrenheit_to_celsius
#include "esphome/core/log.h"

#include "base.h"  // For api_stale

namespace esphome::nspanel_easy {

static const char *const TAG = "nspanel.hw.temperature";

/// @brief Whether the reference currently has a usable, fresh value.
static bool reference_usable = false;
/// @brief Last value received from the reference, in °C; kept while unusable to detect real changes.
static float reference_c = NAN;
/// @brief millis() when the reference value last changed.
static uint32_t reference_changed_ms = 0;
/// @brief Smoothed correction, in °C; NAN until the first sample.
static float correction_smoothed_c = NAN;

/// @brief Why calibration is paused, so each reason is logged once when it starts.
enum class CalibrationPause : uint8_t {
  NONE,          ///< Calibrating
  NO_REFERENCE,  ///< Reference unusable or Home Assistant stale
  TOO_OLD,       ///< Reference unchanged for longer than the maximum age
  OUT_OF_RANGE,  ///< Implied offset beyond TEMP_CALIBRATION_MAX_OFFSET_C
};
static CalibrationPause pause_reason = CalibrationPause::NO_REFERENCE;

void temp_reference_render(const SubBinding &binding, const SubRuntime & /*rt*/, const char *state, bool /*visible*/) {
  // A stale value cannot be trusted, so it is handled like an unusable one.
  const optional<float> value = api_stale ? optional<float>{} : parse_number<float>(state);
  if (!value.has_value()) {
    reference_usable = false;
    return;
  }

#if NSPANEL_EASY_HW_TEMPERATURE_IS_FAHRENHEIT
  const float celsius = fahrenheit_to_celsius(*value);
#else
  const float celsius = *value;
#endif  // NSPANEL_EASY_HW_TEMPERATURE_IS_FAHRENHEIT

  // Only a real change refreshes the age: the same value is re-applied on page
  // changes and re-sent by Home Assistant on reconnection.
  if (celsius != reference_c) {
    reference_c = celsius;
    reference_changed_ms = millis();
    ESP_LOGV(TAG, "Calibration reference %s: %.2f °C", binding.entity, celsius);
  }
  reference_usable = true;
}

optional<float> temp_calibration_sample(float raw_c, float correction_c, uint32_t max_age_ms, float alpha) {
  CalibrationPause reason = CalibrationPause::NONE;
  float offset_c = NAN;
  if (!reference_usable || std::isnan(raw_c)) {
    reason = CalibrationPause::NO_REFERENCE;
  } else if (millis() - reference_changed_ms > max_age_ms) {
    reason = CalibrationPause::TOO_OLD;
  } else {
    offset_c = reference_c - raw_c;
    if (std::fabs(offset_c) > TEMP_CALIBRATION_MAX_OFFSET_C) {
      reason = CalibrationPause::OUT_OF_RANGE;
    }
  }

  if (reason != pause_reason) {
    pause_reason = reason;
    if (reason == CalibrationPause::NONE) {
      ESP_LOGI(TAG, "Temperature calibration running");
    } else if (reason == CalibrationPause::NO_REFERENCE) {
      ESP_LOGI(TAG, "Temperature calibration paused: no usable reference");
    } else if (reason == CalibrationPause::TOO_OLD) {
      ESP_LOGI(TAG, "Temperature calibration paused: reference unchanged for too long");
    } else if (reason == CalibrationPause::OUT_OF_RANGE) {
      ESP_LOGW(TAG, "Temperature calibration paused: offset of %.1f °C is out of range. Check the reference's unit",
               offset_c);
    }
  }
  if (reason != CalibrationPause::NONE) {
    return {};
  }

  if (std::isnan(correction_smoothed_c)) {
    correction_smoothed_c = correction_c;
  }
  correction_smoothed_c += alpha * (offset_c - correction_smoothed_c);
  ESP_LOGV(TAG, "Calibration sample: offset %.2f °C, correction %.2f °C", offset_c, correction_smoothed_c);
  return correction_smoothed_c;
}

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_HW_TEMPERATURE && NSPANEL_EASY_SUBSCRIBE
