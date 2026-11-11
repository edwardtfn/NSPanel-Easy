// page_timer.cpp

#ifdef NSPANEL_EASY_PAGE_TIMER

#include "page_timer.h"

#include <cstdio>
#include <cstdlib>

namespace esphome::nspanel_easy {

int timer_parse_duration(const std::string &text) {
  int parts[3] = {0, 0, 0};
  int count = 0;
  size_t start = 0;
  while (count < 3) {
    const size_t colon = text.find(':', start);
    parts[count++] = atoi(text.substr(start, colon == std::string::npos ? std::string::npos : colon - start).c_str());
    if (colon == std::string::npos) {
      break;
    }
    start = colon + 1;
  }  // while (count < 3)
  if (text.empty() || text.find(':') == std::string::npos) {
    return 0;  // As the Blueprint: only "H:MM:SS" and "M:SS" are durations
  }
  if (count == 3) {
    return (parts[0] * 3600) + (parts[1] * 60) + parts[2];
  }
  return (parts[0] * 60) + parts[1];
}

TimerView timer_view(const SnapshotEntity &entity, int64_t now) {
  TimerView view;
  if (entity.state == "idle") {
    view.state = 0;
  } else if (entity.state == "active") {
    view.state = 1;
  } else if (entity.state == "paused") {
    view.state = 2;
  }

  const int duration = timer_parse_duration(entity.attribute("duration"));
  const int hours = duration / 3600;
  const int minutes = (duration % 3600) / 60;
  const int seconds = duration % 60;
  char label[24];
  if (hours > 0) {
    snprintf(label, sizeof(label), "%d:%02d:%02d", hours, minutes, seconds);
  } else if (minutes > 0) {
    snprintf(label, sizeof(label), "%d:%02d", minutes, seconds);
  } else {
    snprintf(label, sizeof(label), "%d", seconds);
  }
  view.duration_label = label;

  const int remaining = timer_parse_duration(entity.attribute("remaining"));
  if (view.state == 1) {
    int64_t finishes_at = 0;
    if (now > 0 && parse_iso_datetime(entity.attribute("finishes_at"), finishes_at)) {
      // Never below 0, as the Blueprint did
      const int64_t left = finishes_at - now;
      view.remaining = left > 0 ? static_cast<int>(left) : 0;
    } else {
      view.remaining = remaining;
    }
  } else if (view.state == 2) {
    view.remaining = remaining;
  }
  return view;
}

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_TIMER
