// addon_climate.cpp

#ifdef NSPANEL_EASY_ADDON_CLIMATE_BASE

#include "addon_climate.h"

#ifdef NSPANEL_EASY_SUBSCRIBE
#include <cmath>

#include "esphome/core/log.h"
#endif  // NSPANEL_EASY_SUBSCRIBE

namespace esphome::nspanel_easy {

// Global var for the friendly name of the embedded climate entity
std::string addon_climate_friendly_name = "Thermostat";
bool is_addon_climate_visible = false;

#ifdef NSPANEL_EASY_SUBSCRIBE

static const char *const TAG = "nspanel.addon.climate";

/// @brief Which temperature the thermostat regulates on, so each switch is logged once.
enum class ThermostatTempSource : uint8_t {
  EMBEDDED,              ///< Panel's sensor: the reference is not meant to drive the thermostat
  EMBEDDED_UNAVAILABLE,  ///< Panel's sensor: reference unusable, stale or unchanged for too long
  EMBEDDED_IMPLAUSIBLE,  ///< Panel's sensor: reference too far from it
  REFERENCE,             ///< Home Assistant temperature reference
};
static ThermostatTempSource thermostat_temp_source = ThermostatTempSource::EMBEDDED;

float thermostat_temp_resolve(float embedded, uint32_t max_age_ms) {
  const optional<float> reference = temp_reference_get(max_age_ms);
  ThermostatTempSource source = ThermostatTempSource::REFERENCE;
  if (!temp_reference_drives_thermostat()) {
    source = ThermostatTempSource::EMBEDDED;
  } else if (!reference.has_value()) {
    source = ThermostatTempSource::EMBEDDED_UNAVAILABLE;
  } else if (std::isfinite(embedded) && std::fabs(*reference - embedded) > TEMP_CALIBRATION_MAX_OFFSET_C) {
    source = ThermostatTempSource::EMBEDDED_IMPLAUSIBLE;
  }

  if (source != thermostat_temp_source) {
    thermostat_temp_source = source;
    if (source == ThermostatTempSource::REFERENCE) {
      ESP_LOGI(TAG, "Thermostat temperature from the Home Assistant reference");
    } else if (source == ThermostatTempSource::EMBEDDED_UNAVAILABLE) {
      ESP_LOGI(TAG, "Thermostat temperature from the embedded sensor: reference unavailable or unchanged for too long");
    } else if (source == ThermostatTempSource::EMBEDDED_IMPLAUSIBLE) {
      ESP_LOGW(TAG,
               "Thermostat temperature from the embedded sensor: reference %.1f °C is too far from %.1f °C. "
               "Check the reference's unit",
               *reference, embedded);
    } else if (source == ThermostatTempSource::EMBEDDED) {
      ESP_LOGI(TAG, "Thermostat temperature from the embedded sensor");
    }
  }
  return source == ThermostatTempSource::REFERENCE ? *reference : embedded;
}

#endif  // NSPANEL_EASY_SUBSCRIBE

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_ADDON_CLIMATE_BASE
