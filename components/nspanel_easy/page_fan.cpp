// page_fan.cpp

#ifdef NSPANEL_EASY_PAGE_FAN

#include "page_fan.h"

#include <cmath>
#include <cstdlib>

#include "all_icons.h"

namespace esphome::nspanel_easy {

/**
 * @brief Parse a whole attribute text as a number.
 *
 * @param text Attribute text.
 * @param value Receives the number.
 * @return false when the text is empty or not entirely a number.
 */
static bool fan_number(const std::string &text, float &value) {
  if (text.empty()) {
    return false;
  }
  char *end = nullptr;
  value = strtof(text.c_str(), &end);
  return end != text.c_str() && *end == '\0' && std::isfinite(value);
}

/**
 * @brief Trim spaces and line breaks from both ends.
 *
 * @param text Text to trim.
 * @return The trimmed text.
 */
static std::string fan_trim(const std::string &text) {
  const size_t first = text.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = text.find_last_not_of(" \t\r\n");
  return text.substr(first, last - first + 1);
}

FanFeatures fan_features(const SnapshotEntity &entity) {
  FanFeatures fan;
  const int supported = atoi(entity.attribute("supported_features", "0").c_str());
  float number = 0.0f;
  if (entity.state == "on" && fan_number(entity.attribute("percentage"), number)) {
    fan.percentage = static_cast<int>(number);
  }
  if (fan_number(entity.attribute("percentage_step"), number) && number > 0.0f) {
    fan.steps = static_cast<int>(std::nearbyint(100.0f / number));  // Half to even, as Jinja's round
  }
  fan.speed = fan.steps > 0 && (supported & 1) != 0;
  fan.speed_buttons = fan.speed && fan.steps <= 3;
  fan.speed_slider = fan.speed && !fan.speed_buttons;
  fan.oscillate = (supported & 2) != 0;
  fan.oscillating = entity.attribute("oscillating") == "true";
  const std::vector<std::string> *modes = entity.list("preset_modes");
  if (modes != nullptr) {
    fan.modes = *modes;
  }
  fan.preset_modes = (supported & 8) != 0 && !fan.modes.empty();
  fan.preset_buttons = !fan.speed && fan.modes.size() <= static_cast<size_t>(FAN_MODE_BUTTONS);
  fan.preset_mode = fan_trim(entity.attribute("preset_mode"));
  return fan;
}

int fan_active_speed(const FanFeatures &fan, bool on) {
  if (!on) {
    return 0;
  }
  return static_cast<int>(std::nearbyint(static_cast<float>(fan.percentage * fan.steps) / 100.0f));
}

int fan_slider_value(const FanFeatures &fan) {
  return static_cast<int>(std::nearbyint(static_cast<float>(fan.percentage) / 100.0f * static_cast<float>(fan.steps)));
}

const char *fan_speed_icon(int speed, int steps) {
  if (speed <= 0) {
    return Icons::MDI_FAN_OFF;
  }
  if (steps == 1) {
    return Icons::MDI_FAN;
  }
  switch (speed) {
    case 1:
      return Icons::MDI_FAN_SPEED_1;
    case 2:
      return Icons::MDI_FAN_SPEED_2;
    case 3:
      return Icons::MDI_FAN_SPEED_3;
    default:
      return "";  // The page shows speed buttons for up to 3 speeds only
  }  // switch (speed)
}

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_FAN
