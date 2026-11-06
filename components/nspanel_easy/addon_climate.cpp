// addon_climate.cpp

#ifdef NSPANEL_EASY_ADDON_CLIMATE_BASE

#include "addon_climate.h"

#if defined(NSPANEL_EASY_SUBSCRIBE) || defined(NSPANEL_EASY_ADDON_CLIMATE_PID)
#include <cmath>

#include "esphome/core/log.h"
#endif  // NSPANEL_EASY_SUBSCRIBE || NSPANEL_EASY_ADDON_CLIMATE_PID

#ifdef NSPANEL_EASY_ADDON_CLIMATE_PID
#include <cinttypes>  // For PRIu32
#endif                // NSPANEL_EASY_ADDON_CLIMATE_PID

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

#ifdef NSPANEL_EASY_ADDON_CLIMATE_PID

static const char *const TAG_PID = "nspanel.addon.climate.pid";

PidPwmCycle pid_pwm_cycle;

uint32_t pid_pwm_on_time(float level, uint32_t period_ms, uint32_t min_on_ms, uint32_t min_off_ms) {
  if (std::isnan(level) || level <= 0.0f)
    return 0;
  if (level >= 1.0f)
    return period_ms;
  const uint32_t on_ms = static_cast<uint32_t>(level * static_cast<float>(period_ms));
  if (on_ms < min_on_ms)
    return 0;  // Pulse too short: skip it
  if ((period_ms - on_ms) < min_off_ms)
    return period_ms;  // Pause too short: stay on
  return on_ms;
}

void PidPwmCycle::set_level(float level, PidPwmChannel channel) {
  this->pending_level_ = level;
  this->pending_channel_ = channel;
}

void PidPwmCycle::force_level(float level, PidPwmChannel channel) {
  this->set_level(level, channel);
  this->restart_ = true;
}

void PidPwmCycle::start_cycle_(uint32_t now_ms, uint32_t period_ms, uint32_t min_on_ms, uint32_t min_off_ms) {
  this->cycle_start_ms_ = now_ms;
  this->started_ = true;
  this->restart_ = false;
  this->on_ms_ = pid_pwm_on_time(this->pending_level_, period_ms, min_on_ms, min_off_ms);
  this->channel_ = (this->on_ms_ > 0) ? this->pending_channel_ : PID_PWM_CHANNEL_NONE;
  ESP_LOGD(TAG_PID, "PWM cycle: level %.0f %%, on for %" PRIu32 " of %" PRIu32 " s", this->pending_level_ * 100.0f,
           this->on_ms_ / 1000, period_ms / 1000);
}

bool PidPwmCycle::update(uint32_t now_ms, uint32_t period_ms, uint32_t min_on_ms, uint32_t min_off_ms) {
  const PidPwmChannel last_channel = this->channel_;
  const bool last_on = this->on_;

  if (!this->started_ || this->restart_) {
    this->start_cycle_(now_ms, period_ms, min_on_ms, min_off_ms);
  } else if ((now_ms - this->cycle_start_ms_) >= period_ms) {
    this->start_cycle_(now_ms, period_ms, min_on_ms, min_off_ms);
  } else if (this->on_ms_ == 0 && !this->on_ && (!this->was_on_ || (now_ms - this->off_since_ms_) >= min_off_ms) &&
             pid_pwm_on_time(this->pending_level_, period_ms, min_on_ms, min_off_ms) > 0) {
    // Idle cycle and a new demand: start right away instead of waiting for the end of the cycle
    this->start_cycle_(now_ms, period_ms, min_on_ms, min_off_ms);
  }  // if (!this->started_ || this->restart_)

  const bool on = (this->on_ms_ > 0) && ((now_ms - this->cycle_start_ms_) < this->on_ms_);
  if (this->on_ && !on)
    this->off_since_ms_ = now_ms;
  if (on)
    this->was_on_ = true;
  this->on_ = on;
  return (on != last_on) || (this->channel_ != last_channel);
}

#endif  // NSPANEL_EASY_ADDON_CLIMATE_PID

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_ADDON_CLIMATE_BASE
