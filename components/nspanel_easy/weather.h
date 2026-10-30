// weather.h

#pragma once

#ifdef NSPANEL_EASY_USE_WEATHER

#include <cstdint>
#include <cstring>

/**
 * @file weather.h
 * @brief Weather condition to Nextion picture ID mapping.
 *
 * Provides the lookup table and helper functions to resolve a Home Assistant
 * weather condition string to the correct Nextion picture ID, taking into
 * account device generation (legacy / new), active theme (light / dark), and
 * sun elevation (above / below horizon).
 *
 * Condition strings are normalised before lookup: hyphens are replaced with
 * underscores, so "clear-night" and "clear_night" resolve to the same entry.
 * Unrecognised conditions fall back to index 0 (the sentinel / fallback entry).
 *
 * Sun elevation is tracked via @ref SunInfo. When valid coordinates have not
 * yet been received from the blueprint, @ref SunInfo::is_up is derived from
 * the local time using a simple 06:00-18:00 proxy. Once coordinates are
 * available, the ESPHome @c sun component drives the state via its
 * @c on_sunrise and @c on_sunset triggers.
 */

namespace esphome::nspanel_easy {

// =============================================================================
// Sun elevation state
// =============================================================================

/**
 * @brief Tracks sun elevation state and coordinate availability.
 *
 * When @p coords_valid is @c false, @p is_up is derived from the local time as
 * a rough proxy (06:00-18:00 = above horizon). Once coordinates have been
 * received and applied to the ESPHome @c sun component, elevation is computed
 * properly and the @c on_sunrise / @c on_sunset triggers drive the updates.
 *
 * @p coords_valid is never persisted on its own: it is recomputed from the
 * restored coordinates every time they are applied, so the two cannot disagree.
 */
struct SunInfo {
  bool is_up;         ///< true when the sun is above the horizon
  bool coords_valid;  ///< true once valid coordinates have been applied to the sun component
};

extern SunInfo sun_info;  ///< Global sun elevation state for the weather engine

// =============================================================================
// Data structures
// =============================================================================

/**
 * @brief Picture IDs for one condition in one theme variant.
 *
 * Sun-unaware conditions set @p sun_down equal to @p sun_up.
 * The caller selects between them using @ref SunInfo::is_up or
 * @c sun_component->is_above_horizon().
 */
struct WeatherPicVariant {
  uint16_t sun_up;    ///< Picture ID when the sun is above the horizon
  uint16_t sun_down;  ///< Picture ID when the sun is below the horizon
};

/**
 * @brief All picture variants for a single weather condition.
 *
 * Two device generations x two themes, each with a sun-up / sun-down split.
 * A picture ID of 0 indicates the slot is not yet assigned.
 */
struct WeatherPics {
  WeatherPicVariant legacy_light;  ///< Legacy model, light theme
  WeatherPicVariant legacy_dark;   ///< Legacy model, dark theme
  WeatherPicVariant new_light;     ///< New model, light theme
  WeatherPicVariant new_dark;      ///< New model, dark theme
};

/**
 * @brief Associates a normalised condition string with its picture set.
 *
 * Index 0 is always the sentinel / fallback entry (null key).
 * Named entries occupy indices 1 and above.
 */
struct WeatherConditionEntry {
  const char *key;   ///< Condition name (underscore-separated, lower-case), or null for the fallback entry
  WeatherPics pics;  ///< Picture IDs for this condition, or fallback IDs for the sentinel
};

// =============================================================================
// Lookup table
// =============================================================================

/// @brief Sun-unaware variant: same picture regardless of sun elevation.
#define WPV(id) {(id), (id)}
/// @brief Sun-aware variant: distinct pictures for day and night.
#define WPV2(up, down) {(up), (down)}

/**
 * @brief Lookup table mapping weather condition strings to picture IDs.
 *
 * Index 0 is the fallback entry (null key), used for unknown, unavailable,
 * and any unrecognised condition strings Home Assistant may send.
 * Named entries are sorted alphabetically from index 1 for readability.
 *
 * The linear search in @ref get_weather_index is O(n) over at most ~15
 * named entries - performance is not a concern at this scale.
 *
 * @note Picture IDs of 0 indicate unassigned slots - the display will
 *       not update for those combinations until IDs are filled in.
 */
constexpr WeatherConditionEntry WEATHER_CONDITIONS[] = {
    //                      legacy_light    legacy_dark      new_light      new_dark
    {nullptr, {WPV(49), WPV(1), WPV(0), WPV(0)}},                              ///< Index  0 - fallback
    {"clear_night", {WPV2(50, 63), WPV2(2, 15), WPV2(14, 1), WPV2(27, 15)}},   ///< Index  1
    {"cloudy", {WPV(51), WPV(3), WPV(2), WPV(16)}},                            ///< Index  2
    {"exceptional", {WPV2(61, 62), WPV2(13, 14), WPV(0), WPV(0)}},             ///< Index  3
    {"fog", {WPV(56), WPV(8), WPV(3), WPV(17)}},                               ///< Index  4
    {"hail", {WPV(55), WPV(7), WPV(4), WPV(18)}},                              ///< Index  5
    {"lightning", {WPV(58), WPV(10), WPV(5), WPV(19)}},                        ///< Index  6
    {"lightning_rainy", {WPV2(61, 62), WPV2(13, 14), WPV(6), WPV(20)}},        ///< Index  7
    {"partlycloudy", {WPV2(59, 60), WPV2(11, 12), WPV2(7, 8), WPV2(21, 22)}},  ///< Index  8
    {"pouring", {WPV(53), WPV(5), WPV(9), WPV(23)}},                           ///< Index  9
    {"rainy", {WPV(52), WPV(4), WPV(10), WPV(24)}},                            ///< Index 10
    {"snowy", {WPV(54), WPV(6), WPV(11), WPV(25)}},                            ///< Index 11
    {"snowy_rainy", {WPV(55), WPV(7), WPV(12), WPV(26)}},                      ///< Index 12
    {"sunny", {WPV2(50, 63), WPV2(2, 15), WPV2(13, 1), WPV2(27, 15)}},         ///< Index 13
    {"windy", {WPV(57), WPV(9), WPV(14), WPV(28)}},                            ///< Index 14
    {"windy_variant", {WPV(57), WPV(9), WPV(14), WPV(28)}},                    ///< Index 15
};

#undef WPV
#undef WPV2

// =============================================================================
// Current condition state
// =============================================================================

/**
 * @brief Index into @ref WEATHER_CONDITIONS for the current weather condition.
 *
 * Set by @ref get_weather_index when a new condition string is received from
 * the blueprint. Defaults to 0 (fallback) until the first update arrives.
 */
extern uint8_t weather_condition_index;

// =============================================================================
// Helper functions
// =============================================================================

/**
 * @brief Normalises a weather condition string for table lookup.
 *
 * Replaces hyphens with underscores in-place so that "clear-night" and
 * "clear_night" both resolve to the same key.
 *
 * @param[in,out] buf   Null-terminated string to normalise (modified in place).
 * @param[in]     size  Size of the buffer including the null terminator.
 */
inline void normalise_weather_condition(char *buf, size_t size) {
  for (size_t i = 0; i < size && buf[i] != '\0'; ++i) {
    if (buf[i] == '-')
      buf[i] = '_';
  }
}

/**
 * @brief Looks up the index of a weather condition string in @ref WEATHER_CONDITIONS.
 *
 * Normalises the input (hyphens to underscores) once before searching the
 * table. The search starts at index 1, skipping the fallback entry at index 0.
 * Returns 0 for any null, empty, or unrecognised condition string.
 *
 * @param condition  Null-terminated condition string (hyphens or underscores).
 * @return           Index into @ref WEATHER_CONDITIONS, or 0 if not found.
 */
inline uint8_t get_weather_index(const char *condition) {
  if (condition == nullptr || *condition == '\0')
    return 0;

  char buf[32] = {};
  strncpy(buf, condition, sizeof(buf) - 1);
  buf[sizeof(buf) - 1] = '\0';  // Ensure null-termination if input fills the buffer
  normalise_weather_condition(buf, sizeof(buf));

  constexpr uint8_t count = sizeof(WEATHER_CONDITIONS) / sizeof(WEATHER_CONDITIONS[0]);
  for (uint8_t i = 1; i < count; ++i) {
    if (strcmp(WEATHER_CONDITIONS[i].key, buf) == 0)
      return i;
  }
  return 0;  // Fallback
}

/**
 * @brief Returns the picture set for a given @ref WEATHER_CONDITIONS index.
 *
 * Clamps out-of-range indices to 0 (fallback) to guard against stale state.
 *
 * @param index  Index into @ref WEATHER_CONDITIONS.
 * @return       Reference to the matching @ref WeatherPics.
 */
inline const WeatherPics &get_weather_pics(uint8_t index) {
  constexpr uint8_t count = sizeof(WEATHER_CONDITIONS) / sizeof(WEATHER_CONDITIONS[0]);
  if (index >= count)
    index = 0;  // Clamp to fallback
  return WEATHER_CONDITIONS[index].pics;
}

/**
 * @brief Selects the correct picture variant based on device and theme.
 *
 * @param pics          Picture set returned by @ref get_weather_pics.
 * @param is_new_device @c true for the new device generation, @c false for legacy.
 * @param is_dark_theme @c true when the dark theme is active.
 * @return              Reference to the matching @ref WeatherPicVariant.
 */
inline const WeatherPicVariant &select_weather_variant(const WeatherPics &pics, bool is_new_device,
                                                       bool is_dark_theme) {
  if (is_new_device)
    return is_dark_theme ? pics.new_dark : pics.new_light;
  return is_dark_theme ? pics.legacy_dark : pics.legacy_light;
}

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_USE_WEATHER
