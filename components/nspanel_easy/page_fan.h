// page_fan.h

#pragma once

#ifdef NSPANEL_EASY_PAGE_FAN

#include <string>
#include <vector>

#include "entity_snapshot.h"

/**
 * @file page_fan.h
 * @brief Fan settings page, rendered from the Blueprint's entity snapshot (issue #636).
 */

namespace esphome::nspanel_easy {

/// @brief Mode buttons on the fan page (mode_bt1 to mode_bt5), for speeds or preset modes.
static constexpr int FAN_MODE_BUTTONS = 5;

/// @brief What a fan lets the page adjust, as the Blueprint worked it out.
struct FanFeatures {
  int percentage{0};               ///< Speed in percent; 0 while the fan is off
  int steps{0};                    ///< Speeds: 100 / percentage_step, rounded; 0 without speed steps
  bool speed{false};               ///< SET_SPEED, with speed steps
  bool speed_buttons{false};       ///< Up to 3 speeds: buttons (off + speeds), as Home Assistant's dialog
  bool speed_slider{false};        ///< More than 3 speeds: percentage slider
  bool oscillate{false};           ///< OSCILLATE
  bool oscillating{false};         ///< The fan is oscillating
  bool preset_modes{false};        ///< PRESET_MODE, with at least one preset mode
  bool preset_buttons{false};      ///< No speed control and up to 5 modes: modes as buttons
  std::string preset_mode;         ///< Current preset mode, trimmed; empty when none
  std::vector<std::string> modes;  ///< Preset modes
};

/**
 * @brief What a fan supports and its current values.
 *
 * @param entity The fan, from the snapshot.
 * @return The features and values.
 */
FanFeatures fan_features(const SnapshotEntity &entity);

/**
 * @brief Speed button shown as active: the speed for the percentage, with the same
 *        rounding as Home Assistant's fanPercentageToSpeed; 0 (off) while the fan is off.
 *
 * @param fan Features and values of the fan.
 * @param on Whether the fan is on.
 * @return 0 for off, 1 to steps for the speeds.
 */
int fan_active_speed(const FanFeatures &fan, bool on);

/**
 * @brief Slider position for the percentage: percentage / 100 x steps, rounded half to even
 *        as the Blueprint did.
 *
 * @param fan Features and values of the fan.
 * @return 0 to steps.
 */
int fan_slider_value(const FanFeatures &fan);

/**
 * @brief Icon of a speed button.
 *
 * @param speed 0 for off, 1 to steps for the speeds.
 * @param steps Speeds of the fan.
 * @return mdi:fan-off, mdi:fan for a single speed, else mdi:fan-speed-<speed>.
 */
const char *fan_speed_icon(int speed, int steps);

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_FAN
