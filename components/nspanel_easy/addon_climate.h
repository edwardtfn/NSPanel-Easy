// addon_climate.h

#pragma once

#ifdef NSPANEL_EASY_ADDON_CLIMATE_BASE

#include <cstdint>
#include <string>
#include "all_icons.h"
#include "nextion_constants.h"
#include "page_climate.h"  // For the HVAC mode buttons, the climate modes and the presets

#ifdef NSPANEL_EASY_SUBSCRIBE
#include "hw_temperature.h"  // For the Home Assistant temperature reference
#endif                       // NSPANEL_EASY_SUBSCRIBE

namespace esphome::nspanel_easy {

// Global var for the friendly name of the embedded climate entity
extern std::string addon_climate_friendly_name;

extern bool is_addon_climate_visible;

// =============================================================================
// Climate Enumerations
// =============================================================================

/**
 * @enum ClimateAction
 * @brief Current action of the climate device.
 *
 * Represents what the climate device is currently doing.
 * Values match ESPHome's climate component definitions.
 */
enum ClimateAction : uint8_t {
  CLIMATE_ACTION_OFF = 0,      ///< Climate device is off (inactive or no power)
  CLIMATE_ACTION_COOLING = 2,  ///< Climate device is actively cooling
  CLIMATE_ACTION_HEATING = 3,  ///< Climate device is actively heating
  CLIMATE_ACTION_IDLE = 4,     ///< Climate device is idle (monitoring but no action needed)
  CLIMATE_ACTION_DRYING = 5,   ///< Climate device is drying
  CLIMATE_ACTION_FAN = 6       ///< Climate device is in fan only mode
};

/**
 * @enum ClimateChipVisibility
 * @brief Controls when the climate chip is shown on the home screen.
 *
 * Sent by the blueprint on boot via `action_component_val` with `page: "mem"`
 * and stored in the `climate_chip_visibility` global.
 */
enum ClimateChipVisibility : uint8_t {
  CLIMATE_CHIP_ACTIVE_ONLY = 0,  ///< Show chip only when actively heating/cooling/drying/fan
  CLIMATE_CHIP_ALWAYS = 1,       ///< Always show chip when a climate entity is configured
  CLIMATE_CHIP_NEVER = 2         ///< Never show the chip, regardless of climate state
};

// =============================================================================
// Climate Icon Lookup Tables
// =============================================================================

/**
 * @brief Lookup table for the inactive states (off and idle) with different modes.
 *
 * Array indices correspond to ClimateMode enum values (0-6):
 * - [0] CLIMATE_MODE_OFF
 * - [1] CLIMATE_MODE_HEAT_COOL
 * - [2] CLIMATE_MODE_COOL
 * - [3] CLIMATE_MODE_HEAT
 * - [4] CLIMATE_MODE_FAN_ONLY
 * - [5] CLIMATE_MODE_DRY
 * - [6] CLIMATE_MODE_AUTO
 *
 * When the climate device is off or idle, the icon reflects the selected mode
 * but uses grey color to indicate inactive state. CLIMATE_MODE_OFF has no icon.
 */
constexpr IconData climate_off_mode_icons[] = {
    {Icons::MDI_NONE, Colors::RGB565_BLACK},          // CLIMATE_MODE_OFF (0)
    {Icons::MDI_AUTORENEW, Colors::RGB565_GRAY},      // CLIMATE_MODE_HEAT_COOL (1)
    {Icons::MDI_SNOWFLAKE, Colors::RGB565_GRAY},      // CLIMATE_MODE_COOL (2)
    {Icons::MDI_FIRE, Colors::RGB565_GRAY},           // CLIMATE_MODE_HEAT (3)
    {Icons::MDI_FAN, Colors::RGB565_GRAY},            // CLIMATE_MODE_FAN_ONLY (4)
    {Icons::MDI_WATER_PERCENT, Colors::RGB565_GRAY},  // CLIMATE_MODE_DRY (5)
    {Icons::MDI_CALENDAR_SYNC, Colors::RGB565_GRAY}   // CLIMATE_MODE_AUTO (6)
};

/**
 * @brief Lookup table for active climate action states.
 *
 * Array indices correspond to ClimateAction enum values:
 * - [0-1] Unused (reserved)
 * - [2] CLIMATE_ACTION_COOLING
 * - [3] CLIMATE_ACTION_HEATING
 * - [4] CLIMATE_ACTION_IDLE (unused: idle shows the mode from climate_off_mode_icons)
 * - [5] CLIMATE_ACTION_DRYING
 * - [6] CLIMATE_ACTION_FAN
 *
 * When the climate device is actively operating, the icon and color
 * reflect the current action being performed.
 *
 * @note Indices 0-1 are unused to maintain alignment with the
 *       ClimateAction enum values. Index 4 (idle) is kept for the same reason.
 */
constexpr IconData climate_action_icons[] = {
    {Icons::MDI_NONE, Colors::RGB565_BLACK},            // Unused index 0
    {Icons::MDI_NONE, Colors::RGB565_BLACK},            // Unused index 1
    {Icons::MDI_SNOWFLAKE, Colors::RGB565_BLUE},        // CLIMATE_ACTION_COOLING (2)
    {Icons::MDI_FIRE, Colors::RGB565_DEEP_ORANGE},      // CLIMATE_ACTION_HEATING (3)
    {Icons::MDI_THERMOMETER, Colors::RGB565_GRAY},      // CLIMATE_ACTION_IDLE (4), unused
    {Icons::MDI_WATER_PERCENT, Colors::RGB565_ORANGE},  // CLIMATE_ACTION_DRYING (5)
    {Icons::MDI_FAN, Colors::RGB565_CYAN}               // CLIMATE_ACTION_FAN (6)
};

#ifdef NSPANEL_EASY_SUBSCRIBE

// =============================================================================
// Thermostat Temperature Source
// =============================================================================

/**
 * @brief Resolve the temperature the embedded thermostat regulates on.
 *
 * The Home Assistant temperature reference is used only when its binding asks for it
 * (see temp_reference_drives_thermostat()), while it is usable and recent, and while it
 * is within TEMP_CALIBRATION_MAX_OFFSET_C of the panel's sensor; a larger difference
 * usually means it reports in a different unit. Otherwise the panel's sensor is used,
 * including its (auto-)calibrated correction.
 *
 * @param embedded Temperature of the panel's own sensor, in °C.
 * @param max_age_ms Longest time without a change of the reference before it is ignored.
 * @return The reference in °C when it can be trusted, otherwise @p embedded.
 */
float thermostat_temp_resolve(float embedded, uint32_t max_age_ms);

#endif  // NSPANEL_EASY_SUBSCRIBE

#ifdef NSPANEL_EASY_ADDON_CLIMATE_PID

// =============================================================================
// PID Slow PWM
// =============================================================================

/**
 * @enum PidPwmChannel
 * @brief What a PWM cycle of the PID add-ons drives.
 *
 * Latched together with the level at the start of each cycle, so the including package
 * knows which relay to drive (dual) or which operating mode the cycle belongs to (single).
 */
enum PidPwmChannel : uint8_t {
  PID_PWM_CHANNEL_NONE = 0,  ///< No demand: the relays stay off for the whole cycle
  PID_PWM_CHANNEL_HEAT = 1,  ///< The cycle heats
  PID_PWM_CHANNEL_COOL = 2,  ///< The cycle cools
};

/**
 * @brief Relay on-time of one PWM cycle, respecting the minimum on and off times.
 *
 * An on-time shorter than @p min_on_ms becomes 0 (relay off for the whole cycle), and an off-time
 * shorter than @p min_off_ms becomes 0 (relay on for the whole cycle). If both minimums together
 * exceed the period, the minimum on-time is checked first.
 *
 * @param level Output share (0..1); values outside this range are clamped, NaN counts as 0.
 * @param period_ms Cycle length in milliseconds.
 * @param min_on_ms Shortest on-time in milliseconds.
 * @param min_off_ms Shortest off-time in milliseconds.
 * @return On-time in milliseconds, between 0 and @p period_ms.
 */
uint32_t pid_pwm_on_time(float level, uint32_t period_ms, uint32_t min_on_ms, uint32_t min_off_ms);

/**
 * @class PidPwmCycle
 * @brief Slow PWM of the PID add-ons, with the level latched once per cycle.
 *
 * ESPHome's slow_pwm compares the level against the elapsed time continuously, so a level change
 * in the middle of a cycle can cut a pulse short or switch the relay on a second time. Here a new
 * level only takes effect at the start of the next cycle, and its on-time is limited by
 * pid_pwm_on_time(), so each pulse and each pause lasts at least the minimum times.
 *
 * A new cycle starts before the end of the current one when:
 * - force_level() was called (explicit commands such as room off, mode change or boost), or
 * - the current cycle has no on-time, the relay has been off for at least the minimum off-time,
 *   and the pending level results in an on-time, so a new demand does not wait a full period.
 *
 * At the end of a cycle, a new cycle that would switch a relay on waits until that relay has been
 * off for the minimum off-time, which can be longer than the period. Both the shared output and,
 * for the dual package, the relay of each channel are checked.
 */
class PidPwmCycle {
 public:
  /**
   * @brief Sets the level for the next cycle.
   * @param level Output share (0..1).
   * @param channel What the level drives.
   */
  void set_level(float level, PidPwmChannel channel);

  /**
   * @brief Sets the level and starts a new cycle with it on the next update().
   *
   * The minimum times still limit the on-time of the new cycle, but the current pulse or pause
   * ends right away, regardless of how long it has lasted.
   *
   * @param level Output share (0..1).
   * @param channel What the level drives.
   */
  void force_level(float level, PidPwmChannel channel);

  /**
   * @brief Advances the cycle; call it periodically (e.g. every second).
   * @param now_ms Current time from millis().
   * @param period_ms Cycle length in milliseconds.
   * @param min_on_ms Shortest on-time in milliseconds.
   * @param min_off_ms Shortest off-time in milliseconds.
   * @return True when the relay state or the latched channel changed.
   */
  bool update(uint32_t now_ms, uint32_t period_ms, uint32_t min_on_ms, uint32_t min_off_ms);

  /// @brief Whether the relay of the latched channel should be on now.
  bool is_on() const { return this->on_; }

  /// @brief Channel latched for the current cycle; PID_PWM_CHANNEL_NONE when the cycle has no on-time.
  PidPwmChannel channel() const { return this->channel_; }

 protected:
  /// @brief Latches the pending level and channel and starts a new cycle at @p now_ms.
  void start_cycle_(uint32_t now_ms, uint32_t period_ms, uint32_t min_on_ms, uint32_t min_off_ms);

  /// @brief Whether a new cycle now would switch a relay on before it has been off for @p min_off_ms.
  bool switches_on_early_(uint32_t now_ms, uint32_t period_ms, uint32_t min_on_ms, uint32_t min_off_ms) const;

  float pending_level_{0.0f};                            ///< Level for the next cycle
  PidPwmChannel pending_channel_{PID_PWM_CHANNEL_NONE};  ///< Channel for the next cycle
  PidPwmChannel channel_{PID_PWM_CHANNEL_NONE};          ///< Channel of the current cycle
  uint32_t cycle_start_ms_{0};                           ///< Start of the current cycle
  uint32_t on_ms_{0};                                    ///< On-time of the current cycle
  uint32_t off_since_ms_{0};                             ///< When the relay last switched off
  bool on_{false};                                       ///< Relay state
  bool started_{false};                                  ///< A cycle has been started since boot
  bool restart_{false};                                  ///< Start a new cycle on the next update()
  bool was_on_{false};                                   ///< The relay has been on since boot
  uint32_t channel_off_since_ms_[3]{};                   ///< When each channel's relay last switched off
  bool channel_was_on_[3]{};                             ///< Each channel's relay has been on since boot
};

/// @brief The PWM cycle shared by both PID controllers (only one PID add-on can be installed).
extern PidPwmCycle pid_pwm_cycle;

#endif  // NSPANEL_EASY_ADDON_CLIMATE_PID

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_ADDON_CLIMATE_BASE
