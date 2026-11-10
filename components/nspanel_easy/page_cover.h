// page_cover.h

#pragma once

#ifdef NSPANEL_EASY_PAGE_COVER

#include <string>

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

/// @brief A position the user asked for on the page, kept on screen until the cover gets there (or stops).
struct CoverPending {
  int target{-1};     ///< Position asked for, 0 to 100; -1 when nothing is pending
  bool moved{false};  ///< The cover reported opening or closing since the request
};

extern CoverPending cover_pending_position;  ///< Position asked for with the position slider
extern CoverPending cover_pending_tilt;      ///< Tilt asked for with the tilt slider
extern int cover_shown_position;             ///< Position on screen; -1 when not written since the page opened
extern int cover_shown_tilt;                 ///< Tilt on screen; -1 when not written since the page opened

/**
 * @brief Position to show while a request from the page may still be on its way.
 *
 * Covers keep reporting the old position for a while after a request, and then the
 * intermediate positions while they move. Showing those would move the slider back to the
 * old position and then step by step to the new one, so the requested position is shown
 * instead, until the cover reports it, stops (open or closed) after moving, or the request is
 * cleared. An unknown or unavailable state keeps the request.
 *
 * @param pending The request; cleared when it is done.
 * @param reported Position reported by the cover; -1 when it reports none.
 * @param state State of the cover.
 * @param clear Drop the request (page opened, or the entity went quiet).
 * @return The position to show; -1 when there is none.
 */
int cover_position_to_show(CoverPending &pending, int reported, const std::string &state, bool clear);

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_COVER
