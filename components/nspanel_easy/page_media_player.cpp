// page_media_player.cpp

#ifdef NSPANEL_EASY_PAGE_MEDIA_PLAYER

#include "page_media_player.h"

#include <algorithm>
#include <cmath>

#include "esphome/core/helpers.h"  // For parse_number

namespace esphome::nspanel_easy {

uint8_t last_volume_level = 0;
uint32_t last_media_duration = 0;
uint32_t last_media_position = 0;

/**
 * @brief Numeric attribute, truncated towards zero.
 *
 * @param entity The media player, from the snapshot.
 * @param key Attribute name.
 * @return The attribute; 0 when it was not sent or is not a number.
 */
static int32_t media_player_seconds(const SnapshotEntity &entity, const std::string &key) {
  const optional<float> value = parse_number<float>(entity.attribute(key));
  return (value.has_value() && std::isfinite(*value)) ? static_cast<int32_t>(*value) : 0;
}

/**
 * @brief Text attribute, empty when it holds no media information.
 *
 * @param entity The media player, from the snapshot.
 * @param key Attribute name.
 * @return The attribute; empty when it was not sent, or is "unknown" or "unavailable".
 */
static std::string media_player_text(const SnapshotEntity &entity, const std::string &key) {
  std::string text = entity.attribute(key);
  if (text == "unknown" || text == "unavailable") {
    text.clear();
  }  // if (no media information)
  return text;
}

MediaPlayerView media_player_view(const SnapshotEntity &entity, int64_t now) {
  MediaPlayerView view;
  // Home Assistant media player states: off, on, idle, playing, paused, standby, buffering, unavailable, unknown
  view.unavailable = !entity.exists || entity.state == "unavailable" || entity.state == "unknown";
  view.off = entity.state == "off";
  view.playing = entity.state == "playing";
  view.muted = entity.attribute("is_volume_muted") == "true";

  // Music Assistant groups don't expose correct supported_features, so a known-good mask is used:
  // PAUSE(1) + VOLUME_SET(4) + VOLUME_MUTE(8) + PREVIOUS_TRACK(16) + NEXT_TRACK(32) + TURN_ON(128) +
  // TURN_OFF(256) + PLAY_MEDIA(512) + STOP(4096) + PLAY(16384) = 21437
  if (entity.attribute("mass_player_type") == "group") {
    view.supported_features = 21437;
  } else {
    const optional<uint32_t> features = parse_number<uint32_t>(entity.attribute("supported_features"));
    view.supported_features = features.value_or(0);
  }  // if (Music Assistant group)

  // Rounded to 2 decimals, then to a percentage
  const optional<float> volume = parse_number<float>(entity.attribute("volume_level"));
  if (volume.has_value() && std::isfinite(*volume)) {
    view.volume = std::clamp<int32_t>(static_cast<int32_t>(std::lround(*volume * 100.0f)), 0, 100);
  }  // if (volume level)

  view.title = media_player_text(entity, "media_title");
  view.artist = media_player_text(entity, "media_artist");

  view.duration = std::max<int32_t>(media_player_seconds(entity, "media_duration"), 0);
  view.position = media_player_seconds(entity, "media_position");
  if (view.duration > 0) {
    view.position = std::clamp<int32_t>(view.position, 0, view.duration);
    int64_t updated_at = 0;
    if (view.playing && now > 0 && parse_iso_datetime(entity.attribute("media_position_updated_at"), updated_at)) {
      // Never backwards (a clock ahead in Home Assistant) and never beyond the duration
      const int64_t elapsed = std::clamp<int64_t>(now - updated_at, 0, view.duration - view.position);
      view.position += static_cast<int32_t>(elapsed);
    }  // if (playing with a known clock)
  }  // if (finite media)
  return view;
}

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_MEDIA_PLAYER
