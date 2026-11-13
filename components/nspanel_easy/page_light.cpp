// page_light.cpp

#ifdef NSPANEL_EASY_PAGE_LIGHT

#include "page_light.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace esphome::nspanel_easy {

/**
 * @brief Parse a whole attribute text as a number.
 *
 * @param text Attribute text.
 * @param value Receives the number.
 * @return false when the text is empty or not entirely a number.
 */
static bool light_number(const std::string &text, float &value) {
  if (text.empty()) {
    return false;
  }
  char *end = nullptr;
  value = strtof(text.c_str(), &end);
  return end != text.c_str() && *end == '\0' && std::isfinite(value);
}

LightFeatures light_features(const SnapshotEntity &entity) {
  LightFeatures features;
  if (!entity.exists || entity.entity_id.compare(0, 6, "light.") != 0) {
    return features;
  }
  const int supported = atoi(entity.attribute("supported_features", "0").c_str());
  const std::vector<std::string> *modes = entity.list("supported_color_modes");
  auto has_mode = [modes](std::initializer_list<const char *> wanted) {
    if (modes == nullptr) {
      return false;
    }
    for (const char *mode : wanted) {
      if (std::find(modes->begin(), modes->end(), mode) != modes->end()) {
        return true;
      }
    }  // for each wanted mode
    return false;
  };
  float number = 0.0f;
  features.brightness = (supported & 1) != 0 ||
                        has_mode({"brightness", "color_temp", "hs", "xy", "rgb", "rgbw", "rgbww", "white"}) ||
                        (light_number(entity.attribute("brightness"), number) && number >= 0.0f);
  features.color_temp = (supported & 2) != 0 || has_mode({"color_temp"}) ||
                        (light_number(entity.attribute("color_temp_kelvin"), number) && number >= 0.0f);
  // Not from rgb_color: Home Assistant reports it for every light able to show a colour, also colour
  // temperature only lights (as a tuple, which the Blueprint's `is list` test never matched)
  features.color = (supported & 16) != 0 || has_mode({"hs", "xy", "rgb", "rgbw", "rgbww"});
  features.effects = (supported & 4) != 0;
  // The `white` colour mode is a single white, selected with a button as in Home Assistant. With a colour
  // temperature, the slider already brings the light back to white, and the button is its tab.
  features.white = !features.color_temp && has_mode({"white"});
  return features;
}

int light_brightness_pct(const SnapshotEntity &entity) {
  float brightness = 0.0f;
  if (!light_number(entity.attribute("brightness"), brightness)) {
    return 0;
  }
  return static_cast<int>(lroundf(static_cast<float>(static_cast<int>(brightness)) * 100.0f / 255.0f));
}

LightColorTemp light_color_temp(const SnapshotEntity &entity) {
  LightColorTemp temp;
  float number = 0.0f;
  if (light_number(entity.attribute("min_color_temp_kelvin"), number)) {
    temp.min = static_cast<int>(number);
  }
  if (light_number(entity.attribute("max_color_temp_kelvin"), number)) {
    temp.max = static_cast<int>(number);
  }
  const int current = light_number(entity.attribute("color_temp_kelvin"), number) ? static_cast<int>(number) : -1;
  temp.current = (current >= temp.min && current <= temp.max) ? current : (temp.min + temp.max) / 2;
  return temp;
}

std::string light_current_effect(const SnapshotEntity &entity) {
  std::string effect = entity.attribute("effect");
  const size_t first = effect.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return "";
  }
  const size_t last = effect.find_last_not_of(" \t\r\n");
  return effect.substr(first, last - first + 1);
}

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_LIGHT
