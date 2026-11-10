// page_cover.h

#pragma once

#ifdef NSPANEL_EASY_PAGE_COVER

#include "entity_snapshot.h"

/**
 * @file page_cover.h
 * @brief Cover settings page, rendered from the Blueprint's entity snapshot (issue #636).
 */

namespace esphome::nspanel_easy {

/// @brief What the cover page shows, as the Blueprint worked it out.
struct CoverView {
  int position{-1};              ///< current_position, rounded; -1 when the cover reports none
  int tilt{-1};                  ///< current_tilt_position, rounded; -1 when the cover reports none
  bool tilt_available{false};    ///< SET_TILT_POSITION: the position and tilt sliders, else one centered slider
  const char *icon_open{""};     ///< Icon of the open button, by device class (window when none)
  const char *icon_close{""};    ///< Icon of the close button, by device class (window when none)
  int battery{-1};               ///< Battery level, rounded; -1 when unknown
  const char *battery_icon{""};  ///< Battery icon for the level; empty when unknown
};

/**
 * @brief What the page shows for a cover.
 *
 * The battery level comes from the cover's `battery` attribute or, when it has
 * none, from the battery sensor the Blueprint found for it (role "battery").
 *
 * @param entity The cover, from the snapshot.
 * @param battery The battery sensor, from the snapshot; nullptr when there is none.
 * @return What the page shows.
 */
CoverView cover_view(const SnapshotEntity &entity, const SnapshotEntity *battery);

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_COVER
