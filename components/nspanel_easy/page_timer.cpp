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

/**
 * @brief Days from 1970-01-01 to a civil date (proleptic Gregorian calendar).
 *
 * @param year Year.
 * @param month Month, 1 to 12.
 * @param day Day, 1 to 31.
 * @return Days since the Unix epoch.
 */
static int64_t timer_days_from_civil(int64_t year, unsigned month, unsigned day) {
  year -= month <= 2 ? 1 : 0;
  const int64_t era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

bool timer_parse_datetime(const std::string &text, int64_t &epoch) {
  int year = 0;
  unsigned month = 0;
  unsigned day = 0;
  unsigned hour = 0;
  unsigned minute = 0;
  unsigned second = 0;
  int consumed = 0;
  if (sscanf(text.c_str(), "%d-%u-%uT%u:%u:%u%n", &year, &month, &day, &hour, &minute, &second, &consumed) != 6 ||
      month < 1 || month > 12 || day < 1 || day > 31) {
    return false;
  }
  const char *rest = text.c_str() + consumed;
  if (*rest == '.') {  // Fraction of a second
    ++rest;
    while (*rest >= '0' && *rest <= '9') {
      ++rest;
    }
  }
  int offset = 0;
  if (*rest == 'Z') {
    offset = 0;
  } else if (*rest == '+' || *rest == '-') {
    unsigned offset_hours = 0;
    unsigned offset_minutes = 0;
    if (sscanf(rest + 1, "%u:%u", &offset_hours, &offset_minutes) != 2) {
      return false;
    }
    offset = static_cast<int>((offset_hours * 3600) + (offset_minutes * 60)) * (*rest == '-' ? -1 : 1);
  } else {
    return false;  // A time without an offset cannot be placed
  }
  epoch = (timer_days_from_civil(year, month, day) * 86400) + (hour * 3600) + (minute * 60) + second - offset;
  return true;
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
    if (now > 0 && timer_parse_datetime(entity.attribute("finishes_at"), finishes_at)) {
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
