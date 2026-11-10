// page_vacuum.h

#pragma once

#ifdef NSPANEL_EASY_PAGE_VACUUM

#include <string>

#include "entity_snapshot.h"

/**
 * @file page_vacuum.h
 * @brief Vacuum and lawn mower page, rendered from the Blueprint's entity snapshot (issue #636).
 */

namespace esphome::nspanel_easy {

/// @brief What the vacuum / lawn mower page shows, as the Blueprint worked it out.
struct VacuumView {
  bool mower{false};        ///< lawn_mower entity (else vacuum)
  bool active{false};       ///< cleaning, mowing, returning, paused or idle: the active picture
  bool docked{false};       ///< docked
  bool running{false};      ///< cleaning, mowing or returning: the pause icon
  bool start_pause{false};  ///< Start/pause button available
  bool stop{false};         ///< Stop button available (vacuums only)
  bool return_home{false};  ///< Return button available (hidden while docked)
  int battery{-1};          ///< Battery level, rounded half up; -1 when unknown
  std::string state_label;  ///< State, first letter upper case and the rest lower case
};

/**
 * @brief What the page shows for a vacuum or lawn mower.
 *
 * The battery level comes from the entity's `battery_level` attribute or, as
 * Home Assistant 2026.9 moved it, from the device's battery sensor, sent in the
 * snapshot with the role "battery".
 *
 * @param entity The vacuum or lawn mower, from the snapshot.
 * @param battery The device's battery sensor, from the snapshot; nullptr when there is none.
 * @return What the page shows.
 */
VacuumView vacuum_view(const SnapshotEntity &entity, const SnapshotEntity *battery);

/**
 * @brief Battery icon for a level: mdi:battery-10 to mdi:battery, in steps of 10%.
 *
 * @param level Battery level, 0 to 100.
 * @return The icon codepoint.
 */
const char *vacuum_battery_icon(int level);

/**
 * @brief Picture of the page for the state and theme.
 *
 * @param view What the page shows.
 * @param light_theme Whether the panel uses the light theme.
 * @return The picture id.
 */
int vacuum_picture(const VacuumView &view, bool light_theme);

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_VACUUM
