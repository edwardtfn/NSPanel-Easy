// page_water_heater.cpp

#ifdef NSPANEL_EASY_PAGE_WATER_HEATER

#include "page_water_heater.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

#include "esphome/components/nextion/nextion.h"
#include "esphome/core/helpers.h"  // For parse_number
#include "nextion_components.h"    // For nextion_display
#include "text.h"                  // For adjustDecimalSeparator

namespace esphome::nspanel_easy {

/// @brief Modes of the humidifier shown on the page, in the order it reports them, for its button presses.
static std::vector<std::string> humidifier_modes;
/// @brief The humidifier shown on the page is on.
static bool humidifier_on = false;

/// @brief Icons of Home Assistant's standard humidifier modes; other modes get MDI_TUNE.
struct HumidifierModeIcon {
  const char *mode;
  const char *icon;
};
static constexpr HumidifierModeIcon HUMIDIFIER_MODE_ICONS[] = {
    {"normal", Icons::MDI_WATER_PERCENT}, {"eco", Icons::MDI_LEAF},       {"away", Icons::MDI_ACCOUNT_ARROW_RIGHT},
    {"boost", Icons::MDI_ROCKET_LAUNCH},  {"comfort", Icons::MDI_SOFA},   {"home", Icons::MDI_HOME},
    {"sleep", Icons::MDI_SLEEP},          {"auto", Icons::MDI_AUTO_MODE}, {"baby", Icons::MDI_BABY_CARRIAGE},
};

/**
 * @brief Icon of a humidifier mode.
 *
 * @param mode Humidifier mode.
 * @return The icon; nullptr when the mode is not a standard one.
 */
static const char *humidifier_mode_icon(const std::string &mode) {
  for (const HumidifierModeIcon &entry : HUMIDIFIER_MODE_ICONS) {
    if (mode == entry.mode) {
      return entry.icon;
    }  // if (standard mode)
  }  // for each standard mode
  return nullptr;
}

/**
 * @brief Numeric attribute.
 *
 * @param entity The water heater or humidifier, from the snapshot.
 * @param key Attribute name.
 * @return The attribute; empty when it was not sent or is not a finite number.
 */
static optional<float> water_heater_number(const SnapshotEntity &entity, const char *key) {
  const optional<float> value = parse_number<float>(entity.attribute(key));
  if (value.has_value() && std::isfinite(*value)) {
    return value;
  }  // if (finite number)
  return {};
}

WaterHeaterView water_heater_view(const SnapshotEntity &entity) {
  WaterHeaterView view;
  view.humidifier = entity.entity_id.compare(0, 11, "humidifier.") == 0;
  const optional<uint32_t> features = parse_number<uint32_t>(entity.attribute("supported_features"));

  const optional<float> current =
      water_heater_number(entity, view.humidifier ? "current_humidity" : "current_temperature");
  if (current.has_value()) {
    view.current = std::round(*current * 10.0f) / 10.0f;
  }  // if (current value)
  // Humidifiers always have a target humidity, water heaters only with TARGET_TEMPERATURE (bit 1)
  if (view.humidifier || (features.value_or(0) & 1) != 0) {
    const optional<float> target = water_heater_number(entity, view.humidifier ? "humidity" : "temperature");
    if (target.has_value()) {
      view.target = std::round(*target * 10.0f) / 10.0f;
    }  // if (target value)
  }  // if (target supported)

  // Slider step, in tenths. humidifier.set_humidity only accepts whole percentages, so a humidifier steps by
  // whole numbers; a step that rounds to zero would break the slider, so it falls back to 1 unit.
  float step = 0.5f;
  if (view.humidifier) {
    step = std::round(std::fabs(water_heater_number(entity, "target_humidity_step").value_or(1.0f)));
  } else {
    optional<float> temp_step = water_heater_number(entity, "target_temp_step");
    if (!temp_step.has_value()) {
      temp_step = water_heater_number(entity, "target_temperature_step");
    }  // if (no target_temp_step)
    step = temp_step.value_or(0.5f);
  }  // if (humidifier)
  const int32_t step_tenths = static_cast<int32_t>(std::lround(std::fabs(step) * 10.0f));
  view.temp_step = (step_tenths > 0) ? step_tenths : 10;

  // Limits rounded inward, so the slider never offers a value outside the entity's range; the minimum is
  // then rounded up to a multiple of the step
  const float min_value =
      water_heater_number(entity, view.humidifier ? "min_humidity" : "min_temp").value_or(view.humidifier ? 0 : 7);
  const float max_value =
      water_heater_number(entity, view.humidifier ? "max_humidity" : "max_temp").value_or(view.humidifier ? 100 : 35);
  const int32_t min_tenths = static_cast<int32_t>(std::ceil(std::round(min_value * 10000.0f) / 1000.0f));
  const int32_t max_tenths = static_cast<int32_t>(std::floor(std::round(max_value * 10000.0f) / 1000.0f));
  const int32_t remainder = ((min_tenths % view.temp_step) + view.temp_step) % view.temp_step;
  view.temp_offset = (remainder == 0) ? min_tenths : min_tenths + view.temp_step - remainder;
  view.total_steps = (max_tenths >= view.temp_offset) ? (max_tenths - view.temp_offset) / view.temp_step : 0;

  // Icon next to the target: the humidifier mode, or the water heater operation mode
  if (view.humidifier) {
    const char *mode_icon = humidifier_mode_icon(entity.attribute("mode"));
    if (entity.state == "off") {
      view.icon = Icons::MDI_AIR_HUMIDIFIER_OFF;
    } else {
      view.icon = (mode_icon != nullptr) ? mode_icon : Icons::MDI_AIR_HUMIDIFIER;
    }  // if (off)
  } else {
    const std::string operation_mode = entity.attribute("operation_mode", entity.state);
    view.icon = Icons::MDI_WATER_BOILER;
    if (operation_mode == "off") {
      view.icon = Icons::MDI_WATER_BOILER_OFF;
    } else {
      for (const WaterHeaterModeButton &button : WATER_HEATER_MODE_BUTTONS) {
        if (operation_mode == button.mode) {
          view.icon = button.icon;
          break;
        }  // if (mode of the button)
      }  // for each mode button
    }  // if (off)
  }  // if (humidifier)
  return view;
}

std::string water_heater_value_text(float value, int32_t temp_step, const std::string &suffix, char decimal_separator,
                                    const char *unknown) {
  if (value <= -999.0f) {
    return unknown;
  }  // if (unknown)
  char buffer[24];
  snprintf(buffer, sizeof(buffer), (temp_step % 10 != 0) ? "%.1f" : "%.0f", value);
  return adjustDecimalSeparator(buffer, decimal_separator) + suffix;
}

/**
 * @brief Draw one mode button.
 *
 * @param button The button.
 * @param icon Its icon.
 * @param color Its font color.
 */
static void water_heater_button_draw(const WaterHeaterModeButton &button, const char *icon, uint16_t color) {
  nextion_display->set_component_text(button.name, icon);
  nextion_display->set_component_font_color(button.name, color);
  nextion_display->show_component(button.name);
}

void water_heater_buttons_render(const SnapshotEntity &entity) {
  const bool humidifier = entity.entity_id.compare(0, 11, "humidifier.") == 0;
  humidifier_on = humidifier && entity.state == "on";
  const std::vector<std::string> *modes = entity.list(humidifier ? "available_modes" : "operation_list");
  humidifier_modes = (humidifier && modes != nullptr) ? *modes : std::vector<std::string>{};
  if (nextion_display == nullptr) {
    return;
  }  // if (no display)

  if (humidifier) {
    // The modes on the first buttons, in the order the humidifier reports them; the power button last
    const uint16_t color =
        (entity.attribute("device_class") == "dehumidifier") ? Colors::RGB565_ORANGE : Colors::RGB565_BLUE;
    const std::string current_mode = entity.attribute("mode");
    for (uint8_t idx = 0; idx + 1 < WATER_HEATER_MODE_BUTTONS_COUNT; ++idx) {
      const WaterHeaterModeButton &button = WATER_HEATER_MODE_BUTTONS[idx];
      if (idx >= humidifier_modes.size()) {
        nextion_display->hide_component(button.name);
        continue;
      }  // if (no mode for the button)
      const std::string &mode = humidifier_modes[idx];
      const char *icon = humidifier_mode_icon(mode);
      water_heater_button_draw(button, (icon != nullptr) ? icon : Icons::MDI_TUNE,
                               (humidifier_on && mode == current_mode) ? color : WATER_HEATER_MODE_INACTIVE_COLOR);
    }  // for each mode button
    water_heater_button_draw(WATER_HEATER_MODE_BUTTONS[WATER_HEATER_MODE_BUTTONS_COUNT - 1], Icons::MDI_POWER,
                             humidifier_on ? color : WATER_HEATER_MODE_INACTIVE_COLOR);
    return;
  }  // if (humidifier)

  // The operation modes the water heater supports, the current one highlighted
  const std::string operation_mode = entity.attribute("operation_mode", entity.state);
  for (const WaterHeaterModeButton &button : WATER_HEATER_MODE_BUTTONS) {
    const bool supported = modes != nullptr && std::find(modes->begin(), modes->end(), button.mode) != modes->end();
    if (!supported) {
      nextion_display->hide_component(button.name);
      continue;
    }  // if (not supported)
    water_heater_button_draw(button, button.icon,
                             (operation_mode == button.mode) ? button.color_active : WATER_HEATER_MODE_INACTIVE_COLOR);
  }  // for each mode button
}

bool water_heater_humidifier_press(const std::string &mode, std::string &action, std::string &value) {
  value.clear();
  for (uint8_t idx = 0; idx < WATER_HEATER_MODE_BUTTONS_COUNT; ++idx) {
    if (mode != WATER_HEATER_MODE_BUTTONS[idx].mode) {
      continue;
    }  // if (not the pressed button)
    if (idx + 1 == WATER_HEATER_MODE_BUTTONS_COUNT) {  // Power button
      action = humidifier_on ? "humidifier.turn_off" : "humidifier.turn_on";
      return true;
    }  // if (power button)
    if (idx >= humidifier_modes.size()) {
      return false;
    }  // if (no mode for the button)
    action = "humidifier.set_mode";
    value = humidifier_modes[idx];
    return true;
  }  // for each mode button
  return false;
}

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_WATER_HEATER
