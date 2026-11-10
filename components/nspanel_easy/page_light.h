// page_light.h

#pragma once

#ifdef NSPANEL_EASY_PAGE_LIGHT

#include <string>

#include "entity_snapshot.h"

/**
 * @file page_light.h
 * @brief Light settings page, rendered from the Blueprint's entity snapshot (issue #636).
 */

namespace esphome::nspanel_easy {

/// @brief What a light lets the page adjust; also the page's features mask (bits 0 to 3).
struct LightFeatures {
  bool brightness{false};  ///< Bit 0: brightness slider
  bool color_temp{false};  ///< Bit 1: colour temperature slider
  bool color{false};       ///< Bit 2: colour wheel
  bool effects{false};     ///< Bit 3: effect picker

  /// @brief The features as the page's light_features_mask.
  int mask() const { return (brightness ? 1 : 0) + (color_temp ? 2 : 0) + (color ? 4 : 0) + (effects ? 8 : 0); }
};

/**
 * @brief What a light supports, as the Blueprint worked it out.
 *
 * From `supported_features`, `supported_color_modes` and the attributes the
 * light reports. As in Home Assistant, every colour mode other than on/off also
 * supports brightness, which the Blueprint detected from the `brightness`
 * attribute being present (the snapshot leaves out empty attributes).
 *
 * @param entity The light, from the snapshot.
 * @return The features; all false for an entity that is not a light.
 */
LightFeatures light_features(const SnapshotEntity &entity);

/**
 * @brief Brightness as a percentage, as the page shows it.
 *
 * @param entity The light, from the snapshot.
 * @return 0 to 100; 0 when the light reports no brightness (e.g. off).
 */
int light_brightness_pct(const SnapshotEntity &entity);

/// @brief Colour temperature range and value, in kelvin, as the page shows them.
struct LightColorTemp {
  int min{2000};      ///< min_color_temp_kelvin
  int max{6535};      ///< max_color_temp_kelvin
  int current{4268};  ///< color_temp_kelvin, or the middle of the range when out of it
};

/**
 * @brief Colour temperature of a light, as the page shows it.
 *
 * @param entity The light, from the snapshot.
 * @return The range and value; a missing or out-of-range value is the middle of the range.
 */
LightColorTemp light_color_temp(const SnapshotEntity &entity);

/**
 * @brief Current effect, trimmed; empty when none.
 *
 * @param entity The light, from the snapshot.
 * @return The effect.
 */
std::string light_current_effect(const SnapshotEntity &entity);

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_LIGHT
