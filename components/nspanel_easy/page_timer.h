// page_timer.h

#pragma once

#ifdef NSPANEL_EASY_PAGE_TIMER

#include <cstdint>
#include <string>

#include "entity_snapshot.h"

/**
 * @file page_timer.h
 * @brief Timer page, rendered from the Blueprint's entity snapshot (issue #636).
 */

namespace esphome::nspanel_easy {

/// @brief What the timer page shows.
struct TimerView {
  int state{-1};               ///< 0 idle, 1 active, 2 paused, -1 anything else
  int remaining{0};            ///< Seconds left
  std::string duration_label;  ///< Duration as H:MM:SS, M:SS or S
};

/**
 * @brief Parse a timer duration ("H:MM:SS" or "M:SS") into seconds.
 *
 * @param text Duration as Home Assistant reports it.
 * @return Seconds; 0 when the text is empty or not a duration.
 */
int timer_parse_duration(const std::string &text);

/**
 * @brief Parse an ISO 8601 date and time with an offset ("2026-11-09T20:15:00+00:00") into a Unix time.
 *
 * @param text Date and time.
 * @param epoch Receives the Unix time.
 * @return false when the text is not a date and time with an offset (or Z).
 */
bool timer_parse_datetime(const std::string &text, int64_t &epoch);

/**
 * @brief What the page shows for a timer.
 *
 * While active, the seconds left come from `finishes_at`, as `remaining` is only
 * updated when the timer starts, pauses or finishes; while paused, from
 * `remaining`.
 *
 * @param entity The timer, from the snapshot.
 * @param now Current Unix time; 0 when the clock is not set (then `remaining` is used).
 * @return The state, seconds left and duration label.
 */
TimerView timer_view(const SnapshotEntity &entity, int64_t now);

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_TIMER
