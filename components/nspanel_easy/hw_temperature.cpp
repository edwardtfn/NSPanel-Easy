// hw_temperature.cpp

#if defined(NSPANEL_EASY_HW_TEMPERATURE) && defined(NSPANEL_EASY_SUBSCRIBE)

#include "hw_temperature.h"

#include <cmath>
#include <cstring>

#include "esphome/core/hal.h"      // For millis
#include "esphome/core/helpers.h"  // For parse_number and fahrenheit_to_celsius
#include "esphome/core/log.h"

#include "base.h"  // For api_stale

namespace esphome::nspanel_easy {

static const char *const TAG = "nspanel.hw.temperature";

std::function<void()> temp_reference_changed;

/// @brief Whether the reference currently has a usable value; false while Home Assistant is stale.
static bool reference_usable = false;
/// @brief Whether the reference entity itself is available. Only a climate reference reports it,
/// through its "availability" binding, since its attribute is not re-sent when the entity drops it.
static bool reference_available = true;
/// @brief Last value received from the reference, in °C; kept while unusable to detect real changes.
static float reference_c = NAN;
/// @brief millis() when the reference value last changed.
static uint32_t reference_changed_ms = 0;
/// @brief Set once temp_reference_aged_out() has reported the reference too old; cleared on a change.
/// Keeps it too old after millis() - reference_changed_ms wraps around, about 49.7 days later.
static bool reference_aged_reported = false;
/// @brief Whether the binding targets the "thermostat" component.
static bool reference_drives_thermostat = false;
/// @brief Smoothed correction, in °C; NAN until the first sample.
static float correction_smoothed_c = NAN;

/// @brief Why calibration is paused, so each reason is logged once when it starts.
enum class CalibrationPause : uint8_t {
  NONE,          ///< Calibrating
  NO_REFERENCE,  ///< Reference unusable or unavailable, or Home Assistant stale
  TOO_OLD,       ///< Reference unchanged for longer than the maximum age
  OUT_OF_RANGE,  ///< Implied offset beyond TEMP_CALIBRATION_MAX_OFFSET_C
};
static CalibrationPause pause_reason = CalibrationPause::NO_REFERENCE;

/// @brief Whether the reference has gone unchanged for longer than @p max_age_ms.
static bool reference_too_old(uint32_t max_age_ms) { return millis() - reference_changed_ms > max_age_ms; }

void temp_reference_render(const SubBinding &binding, const SubRuntime &rt, const char *state, bool /*visible*/) {
  if (strcmp(binding.component, "availability") == 0) {
    // rt.state rather than state: for a climate binding without attribute, state carries hvac_action
    // while it holds a value, and Home Assistant does not clear it when the entity becomes unavailable.
    const bool available = !sub_state_in(rt.state, SUB_UNUSABLE_STATES);
    if (available != reference_available) {
      reference_available = available;
      ESP_LOGD(TAG, "Calibration reference %s %s", binding.entity, available ? "available" : "unavailable");
      if (reference_drives_thermostat && temp_reference_changed) {
        temp_reference_changed();
      }
    }
    return;
  }

  // Only the value binding carries the mode; the availability binding must not reset it
  reference_drives_thermostat = (strcmp(binding.component, "thermostat") == 0);

  // A stale value cannot be trusted, so it is handled like an unusable one. A non-finite
  // number ("nan", "inf") parses successfully but would poison the correction.
  const optional<float> value = api_stale ? optional<float>{} : parse_number<float>(state);
  const bool usable = value.has_value() && std::isfinite(*value);
  bool changed = false;
  if (usable) {
#if NSPANEL_EASY_HW_TEMPERATURE_IS_FAHRENHEIT
    const float celsius = fahrenheit_to_celsius(*value);
#else   // NSPANEL_EASY_HW_TEMPERATURE_IS_FAHRENHEIT
    const float celsius = *value;
#endif  // NSPANEL_EASY_HW_TEMPERATURE_IS_FAHRENHEIT

    // Only a real change refreshes the age: the same value is re-applied on page
    // changes and re-sent by Home Assistant on reconnection.
    if (celsius != reference_c) {
      reference_c = celsius;
      reference_changed_ms = millis();
      reference_aged_reported = false;
      changed = true;
      ESP_LOGV(TAG, "Temperature reference %s: %.2f °C", binding.entity, celsius);
    }
  }

  changed = changed || (usable != reference_usable);
  reference_usable = usable;
  if (changed && reference_drives_thermostat && temp_reference_changed) {
    temp_reference_changed();
  }
}

bool temp_reference_drives_thermostat() { return reference_drives_thermostat; }

optional<float> temp_reference_get(uint32_t max_age_ms) {
  if (!reference_usable || !reference_available || reference_aged_reported || reference_too_old(max_age_ms)) {
    return {};
  }
  return reference_c;
}

bool temp_reference_aged_out(uint32_t max_age_ms) {
  if (!reference_usable || !reference_available || reference_aged_reported || !reference_too_old(max_age_ms)) {
    return false;
  }
  reference_aged_reported = true;
  return true;
}

optional<float> temp_calibration_sample(float raw_c, float correction_c, uint32_t max_age_ms, float alpha) {
  CalibrationPause reason = CalibrationPause::NONE;
  float offset_c = NAN;
  if (!reference_usable || !reference_available || !std::isfinite(raw_c)) {
    reason = CalibrationPause::NO_REFERENCE;
  } else if (reference_too_old(max_age_ms)) {
    reason = CalibrationPause::TOO_OLD;
  } else {
    offset_c = reference_c - raw_c;
    if (!std::isfinite(offset_c) || std::fabs(offset_c) > TEMP_CALIBRATION_MAX_OFFSET_C) {
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
