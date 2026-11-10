// page_cover.cpp

#ifdef NSPANEL_EASY_PAGE_COVER

#include "page_cover.h"

#include <cmath>
#include <cstdlib>
#include <string>

#include "all_icons.h"

#ifdef NSPANEL_EASY_SUBSCRIBE
#include "api_subscriptions.h"
#endif  // NSPANEL_EASY_SUBSCRIBE

namespace esphome::nspanel_easy {

/**
 * @brief Parse a whole text as a finite number, rounded.
 *
 * @param text Text to parse.
 * @param value Receives the rounded number.
 * @return false when the text is empty or not entirely a number.
 */
static bool cover_number(const std::string &text, int &value) {
  if (text.empty()) {
    return false;
  }
  char *end = nullptr;
  const float number = strtof(text.c_str(), &end);
  if (end == text.c_str() || *end != '\0' || !std::isfinite(number)) {
    return false;
  }
  value = static_cast<int>(std::nearbyint(number));  // Half to even, as Jinja's round
  return true;
}

/**
 * @brief Icon of a cover button for a device class, from the icon rules.
 *
 * @param device_class Device class.
 * @param state "open" or "closed".
 * @param fallback Icon when the device class has no rule.
 * @return The icon codepoint.
 */
static const char *cover_icon(const std::string &device_class, const char *state, const char *fallback) {
#ifdef NSPANEL_EASY_SUBSCRIBE
  const std::string key = "cover." + device_class + "-" + state;
  const char *icon = sub_icon_rule(key.c_str());
  return icon != nullptr ? icon : fallback;
#else   // NSPANEL_EASY_SUBSCRIBE
  return fallback;
#endif  // NSPANEL_EASY_SUBSCRIBE
}

CoverView cover_view(const SnapshotEntity &entity, const SnapshotEntity *battery) {
  CoverView view;
  cover_number(entity.attribute("current_position"), view.position);
  cover_number(entity.attribute("current_tilt_position"), view.tilt);
  view.tilt_available = (atoi(entity.attribute("supported_features", "0").c_str()) & 128) != 0;

  std::string device_class = entity.attribute("device_class");
  if (device_class.empty()) {
    device_class = "window";
  }
  view.icon_open = cover_icon(device_class, "open", Icons::MDI_WINDOW_OPEN);
  view.icon_close = cover_icon(device_class, "closed", Icons::MDI_WINDOW_CLOSED);

  if (cover_number(entity.attribute("battery"), view.battery) ||
      (battery != nullptr && battery->exists && cover_number(battery->state, view.battery))) {
    view.battery_icon = view.battery >= 80   ? Icons::MDI_BATTERY_HIGH
                        : view.battery >= 40 ? Icons::MDI_BATTERY_MEDIUM
                        : view.battery >= 15 ? Icons::MDI_BATTERY_LOW
                                             : Icons::MDI_BATTERY_OUTLINE;
  } else {
    view.battery = -1;
  }
  return view;
}

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_COVER
