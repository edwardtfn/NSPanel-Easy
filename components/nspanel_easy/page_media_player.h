// page_media_player.h

#pragma once

#ifdef NSPANEL_EASY_PAGE_MEDIA_PLAYER

#include <cstdint>
#include <string>

#include "entity_snapshot.h"

/**
 * @file page_media_player.h
 * @brief Media player page, rendered from the Blueprint's entity snapshot (issue #636).
 */

namespace esphome::nspanel_easy {

extern uint8_t last_volume_level;     // Last volume level from Home Assistant
extern uint32_t last_media_duration;  // Last duration from Home Assistant
extern uint32_t last_media_position;  // Last position from Home Assistant

/// @brief What the media player page shows.
struct MediaPlayerView {
  bool unavailable{false};         ///< unavailable or unknown (also a missing entity): the controls are hidden
  bool off{false};                 ///< off
  bool playing{false};             ///< playing
  bool muted{false};               ///< is_volume_muted
  uint32_t supported_features{0};  ///< Supported features; a fixed mask for Music Assistant groups
  int32_t volume{0};               ///< Volume level, 0 to 100
  int32_t duration{0};             ///< Media duration, in seconds; 0 when nothing finite is loaded (e.g. a livestream)
  int32_t position{0};             ///< Media position, in seconds, within 0 and the duration
  std::string title;               ///< Media title; empty when unknown
  std::string artist;              ///< Media artist; empty when unknown
};

/**
 * @brief What the page shows for a media player.
 *
 * While playing, the position is moved forward by the time elapsed since
 * `media_position_updated_at`, as Home Assistant only updates `media_position`
 * when the player reports it; never beyond the duration.
 *
 * @param entity The media player, from the snapshot.
 * @param now Current Unix time; 0 when the clock is not set (then the reported position is used).
 * @return What the page shows.
 */
MediaPlayerView media_player_view(const SnapshotEntity &entity, int64_t now);

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_MEDIA_PLAYER
