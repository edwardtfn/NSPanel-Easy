// page_vacuum.cpp

#ifdef NSPANEL_EASY_PAGE_VACUUM

#include "page_vacuum.h"

#include <cctype>
#include <cmath>
#include <cstdlib>

#include "all_icons.h"

namespace esphome::nspanel_easy {

/**
 * @brief Parse a whole text as a finite number.
 *
 * @param text Text to parse.
 * @param value Receives the number.
 * @return false when the text is empty or not entirely a number.
 */
static bool vacuum_number(const std::string &text, float &value) {
  if (text.empty()) {
    return false;
  }
  char *end = nullptr;
  value = strtof(text.c_str(), &end);
  return end != text.c_str() && *end == '\0' && std::isfinite(value);
}

VacuumView vacuum_view(const SnapshotEntity &entity, const SnapshotEntity *battery) {
  VacuumView view;
  view.mower = entity.entity_id.compare(0, 11, "lawn_mower.") == 0;
  const std::string &state = entity.state;
  view.active =
      state == "cleaning" || state == "mowing" || state == "returning" || state == "paused" || state == "idle";
  view.docked = state == "docked";
  view.running = state == "cleaning" || state == "mowing" || state == "returning";

  const int supported = atoi(entity.attribute("supported_features", "0").c_str());
  if (view.mower) {
    view.start_pause = (supported & (1 | 2)) != 0;  // START_MOWING, PAUSE
    view.stop = false;
    view.return_home = (supported & 4) != 0;  // DOCK
  } else {
    view.start_pause = (supported & (8192 | 4)) != 0;  // START, PAUSE
    view.stop = (supported & 8) != 0;                  // STOP
    view.return_home = (supported & 16) != 0;          // RETURN_HOME
  }

  // Rounded half up (+0.5 then truncated); battery levels are never negative
  float level = 0.0f;
  if (vacuum_number(entity.attribute("battery_level"), level) ||
      (battery != nullptr && battery->exists && vacuum_number(battery->state, level))) {
    view.battery = static_cast<int>(level + 0.5f);
  }

  // As Python's str.capitalize(): first character upper case, the rest lower case
  view.state_label = state;
  for (size_t idx = 0; idx < view.state_label.size(); ++idx) {
    const unsigned char character = static_cast<unsigned char>(view.state_label[idx]);
    view.state_label[idx] = static_cast<char>(idx == 0 ? toupper(character) : tolower(character));
  }  // for each character
  return view;
}

const char *vacuum_battery_icon(int level) {
  if (level >= 95) {
    return Icons::MDI_BATTERY;
  }
  static const char *const LEVELS[] = {
      Icons::MDI_BATTERY_10, Icons::MDI_BATTERY_20, Icons::MDI_BATTERY_30, Icons::MDI_BATTERY_40, Icons::MDI_BATTERY_50,
      Icons::MDI_BATTERY_60, Icons::MDI_BATTERY_70, Icons::MDI_BATTERY_80, Icons::MDI_BATTERY_90,
  };
  // 85 to 94 is battery-90, 75 to 84 battery-80, ... below 15 battery-10
  const int step = (level + 5) / 10;  // 15 -> 2 (battery-20), 14 -> 1 (battery-10)
  return LEVELS[step < 1 ? 0 : (step > 9 ? 8 : step - 1)];
}

int vacuum_picture(const VacuumView &view, bool light_theme) {
  const int base = view.mower ? 100 : 96;  // Active dark, active light, docked dark, docked light
  return base + (view.active ? 0 : 2) + (light_theme ? 1 : 0);
}

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_VACUUM
