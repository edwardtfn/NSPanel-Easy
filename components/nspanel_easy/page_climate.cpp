// page_climate.cpp

#ifdef NSPANEL_EASY_PAGE_CLIMATE

#include "page_climate.h"

#include <algorithm>
#include <cinttypes>
#include <cmath>
#include <cstring>

#include "base.h"
#include "esphome/components/nextion/nextion.h"
#include "esphome/core/helpers.h"  // For parse_number
#include "esphome/core/log.h"
#include "pages.h"  // For get_page_id

#ifdef NSPANEL_EASY_PAGE_BUTTONS
#include "page_buttons.h"  // For button_draw
#endif                     // NSPANEL_EASY_PAGE_BUTTONS

#ifdef NSPANEL_EASY_PAGE_POPUP_SELECT
#include "page_popup_select.h"
#endif  // NSPANEL_EASY_PAGE_POPUP_SELECT

namespace esphome::nspanel_easy {

static const char *const TAG = "nspanel.page.climate";

bool climate_embedded_visible = false;

/// @brief Presets of the Home Assistant climate shown on the page, for the picker.
static std::vector<std::string> climate_preset_modes;
/// @brief Its active preset; empty when none.
static std::string climate_preset_mode;
/// @brief Fan modes of the Home Assistant climate shown on the page, for the picker.
static std::vector<std::string> climate_fan_modes;
/// @brief Its active fan mode; empty when unknown.
static std::string climate_fan_mode;

/**
 * @brief Numeric attribute.
 *
 * @param entity The climate, from the snapshot.
 * @param key Attribute name.
 * @return The attribute; empty when it was not sent or is not a finite number.
 */
static optional<float> climate_number(const SnapshotEntity &entity, const char *key) {
  const optional<float> value = parse_number<float>(entity.attribute(key));
  if (value.has_value() && std::isfinite(*value)) {
    return value;
  }  // if (finite number)
  return {};
}

/**
 * @brief Temperature attribute rounded to one decimal, as shown.
 *
 * @param entity The climate, from the snapshot.
 * @param key Attribute name.
 * @return The temperature; -999 when it was not sent or is not a number.
 */
static float climate_temperature(const SnapshotEntity &entity, const char *key) {
  const optional<float> value = climate_number(entity, key);
  return value.has_value() ? std::round(*value * 10.0f) / 10.0f : -999.0f;
}

/**
 * @brief Whether a state or attribute carries no usable value.
 *
 * @param text The state or attribute.
 * @return true for empty, unknown, unavailable or none.
 */
static bool climate_unusable(const std::string &text) {
  return text.empty() || text == "unknown" || text == "unavailable" || text == "None" || text == "none";
}

ClimateView climate_view(const SnapshotEntity &entity) {
  ClimateView view;
  const optional<uint32_t> features = parse_number<uint32_t>(entity.attribute("supported_features"));
  view.supported_features = features.value_or(0);

  // Slider step, in tenths: target_temp_step, else target_temperature_step, else 0.5
  optional<float> step = climate_number(entity, "target_temp_step");
  if (!step.has_value()) {
    step = climate_number(entity, "target_temperature_step");
  }  // if (no target_temp_step)
  const int32_t step_tenths = static_cast<int32_t>(std::lround(std::fabs(step.value_or(0.5f)) * 10.0f));
  view.temp_step = (step_tenths > 0) ? step_tenths : 10;  // A step that rounds to zero would break the slider

  // Slider range, in tenths; the minimum is rounded up to a multiple of the step
  const int32_t min_tenths =
      static_cast<int32_t>(std::lround(climate_number(entity, "min_temp").value_or(7.0f) * 10.0f));
  const int32_t max_tenths =
      static_cast<int32_t>(std::lround(climate_number(entity, "max_temp").value_or(35.0f) * 10.0f));
  const int32_t remainder = ((min_tenths % view.temp_step) + view.temp_step) % view.temp_step;
  view.temp_offset = (remainder == 0) ? min_tenths : min_tenths + view.temp_step - remainder;
  view.total_steps = (max_tenths >= view.temp_offset) ? (max_tenths - view.temp_offset) / view.temp_step : 0;

  view.current_temp = climate_temperature(entity, "current_temperature");
  if ((view.supported_features & 1) != 0) {  // TARGET_TEMPERATURE
    view.target_temp = climate_temperature(entity, "temperature");
  }  // if (target temperature)
  if ((view.supported_features & 2) != 0) {  // TARGET_TEMPERATURE_RANGE
    view.target_temp_high = climate_temperature(entity, "target_temp_high");
    view.target_temp_low = climate_temperature(entity, "target_temp_low");
  }  // if (target temperature range)

  // Icon of what the climate is doing, else of its mode; checked in this order, as "heat_cool" contains "heat"
  const std::string &hvac_action = entity.attribute("hvac_action");
  const std::string &action = climate_unusable(hvac_action) ? entity.state : hvac_action;
  struct ActionIcon {
    const char *text;
    const char *icon;
  };
  static constexpr ActionIcon ACTION_ICONS[] = {
      {"off", Icons::MDI_NONE},
      {"heat_cool", Icons::MDI_AUTORENEW},
      {"heat", Icons::MDI_THERMOMETER_LINES},  // Also "heating"
      {"cool", Icons::MDI_SNOWFLAKE},          // Also "cooling"
      {"dry", Icons::MDI_WATER_PERCENT},       // Also "drying"
      {"fan", Icons::MDI_FAN},                 // Also "fan_only"
      {"auto", Icons::MDI_REFRESH_AUTO},
      {"idle", Icons::MDI_THERMOMETER},
  };
  for (const ActionIcon &entry : ACTION_ICONS) {
    if (action.find(entry.text) != std::string::npos) {
      view.icon = entry.icon;
      break;
    }  // if (action matches)
  }  // for each action icon
  return view;
}

void climate_mode_buttons_render(const SnapshotEntity &entity) {
  if (nextion_display == nullptr) {
    return;
  }  // if (no display)
  const std::vector<std::string> *modes = entity.list("hvac_modes");
  for (const ClimateModeButton &button : CLIMATE_MODE_BUTTONS) {
    const bool supported =
        modes != nullptr && std::find(modes->begin(), modes->end(), button.hvac_mode) != modes->end();
    if (supported) {
      nextion_display->show_component(button.name);
      nextion_display->set_component_font_color(
          button.font_color_path, (entity.state == button.hvac_mode) ? button.color_active : button.color_inactive);
    } else {
      nextion_display->hide_component(button.name);
    }  // if (supported)
  }  // for (button : CLIMATE_MODE_BUTTONS)
}

void climate_presets_render(const SnapshotEntity &entity) {
  const optional<uint32_t> features = parse_number<uint32_t>(entity.attribute("supported_features"));
  const std::vector<std::string> *modes = entity.list("preset_modes");
  const bool supported = (features.value_or(0) & 16) != 0 && modes != nullptr && !modes->empty();  // PRESET_MODE
  climate_preset_modes = supported ? *modes : std::vector<std::string>{};
  climate_preset_mode = climate_unusable(entity.attribute("preset_mode")) ? "" : entity.attribute("preset_mode");
  if (nextion_display == nullptr) {
    return;
  }  // if (no display)
  if (!supported) {
    nextion_display->hide_component(hmi::climate::PRESET_BUTTON.name);
    return;
  }  // if (no presets)

  // Standard presets get the icon Home Assistant uses for them; none or a custom preset, the generic one
  const char *icon = climate_preset_icons[0];
  bool active = false;
  if (!climate_preset_mode.empty()) {
    active = !str_equals_case_insensitive(climate_preset_mode, "none");
    for (uint8_t idx = 1; idx < CLIMATE_PRESET_NAMES_COUNT; ++idx) {
      if (str_equals_case_insensitive(climate_preset_mode, climate_preset_names[idx])) {
        icon = climate_preset_icons[idx];
        break;
      }  // if (standard preset)
    }  // for each standard preset
  }  // if (a preset is set)
  nextion_display->set_component_text(hmi::climate::PRESET_BUTTON.name, icon);
  // Theme colors from the display, so the button follows the light and dark themes
  nextion_display->send_command_printf("climate.%s.pco=%s", hmi::climate::PRESET_BUTTON.name,
                                       active ? "color_pco_main" : "color_pco_secondary");
  nextion_display->show_component(hmi::climate::PRESET_BUTTON.name);
}

bool climate_presets_to_picker(const std::string &entity_id) {
#ifdef NSPANEL_EASY_PAGE_POPUP_SELECT
  if (climate_preset_modes.empty()) {
    ESP_LOGW(TAG, "%s has no presets", entity_id.c_str());
    return false;
  }  // if (no presets)
  popup_select_cache_entity = entity_id;
  popup_select_cache_context = CLIMATE_PRESET_MODE_CONTEXT;
  popup_select_cache_mode = POPUP_SELECT_MODE_SINGLE;
  popup_select_cache_options = popup_select_visible_options(climate_preset_modes, climate_preset_mode);
  popup_select_cache_mask = 0;
  for (size_t idx = 0; idx < popup_select_cache_options.size(); ++idx) {
    if (popup_select_cache_options[idx] == climate_preset_mode) {
      popup_select_cache_mask = (1u << idx);
      break;
    }  // if (active preset)
  }  // for each option
  return true;
#else   // NSPANEL_EASY_PAGE_POPUP_SELECT
  (void) entity_id;
  return false;
#endif  // NSPANEL_EASY_PAGE_POPUP_SELECT
}

void climate_fan_modes_render(const SnapshotEntity &entity) {
  const optional<uint32_t> features = parse_number<uint32_t>(entity.attribute("supported_features"));
  const std::vector<std::string> *modes = entity.list("fan_modes");
  const bool supported = (features.value_or(0) & 8) != 0 && modes != nullptr && !modes->empty();  // FAN_MODE
  climate_fan_modes = supported ? *modes : std::vector<std::string>{};
  climate_fan_mode = climate_unusable(entity.attribute("fan_mode")) ? "" : entity.attribute("fan_mode");
  if (nextion_display == nullptr) {
    return;
  }  // if (no display)
  if (!supported) {
    nextion_display->hide_component(hmi::climate::FAN_BUTTON.name);
    return;
  }  // if (no fan modes)

  // The common fan modes get their own icon; any other mode, the generic one.
  // Not the plain fan icon, which the page uses for the "fan only" HVAC mode.
  struct FanModeIcon {
    const char *mode;  ///< Fan mode, as Home Assistant names it
    const char *icon;  ///< Icon shown on the fan mode button
  };
  static constexpr FanModeIcon FAN_MODE_ICONS[] = {{"auto", Icons::MDI_FAN_AUTO},
                                                   {"off", Icons::MDI_FAN_OFF},
                                                   {"low", Icons::MDI_FAN_SPEED_1},
                                                   {"medium", Icons::MDI_FAN_SPEED_2},
                                                   {"high", Icons::MDI_FAN_SPEED_3}};
  const char *icon = Icons::MDI_FAN_CHEVRON_UP;
  for (const FanModeIcon &entry : FAN_MODE_ICONS) {
    if (str_equals_case_insensitive(climate_fan_mode, entry.mode)) {
      icon = entry.icon;
      break;
    }  // if (common fan mode)
  }  // for each common fan mode
  nextion_display->set_component_text(hmi::climate::FAN_BUTTON.name, icon);
  // Theme color from the display, so the button follows the light and dark themes
  nextion_display->send_command_printf("climate.%s.pco=color_pco_secondary", hmi::climate::FAN_BUTTON.name);
  nextion_display->show_component(hmi::climate::FAN_BUTTON.name);
}

bool climate_fan_modes_to_picker(const std::string &entity_id) {
#ifdef NSPANEL_EASY_PAGE_POPUP_SELECT
  if (climate_fan_modes.empty()) {
    ESP_LOGW(TAG, "%s has no fan modes", entity_id.c_str());
    return false;
  }  // if (no fan modes)
  popup_select_cache_entity = entity_id;
  popup_select_cache_context = CLIMATE_FAN_MODE_CONTEXT;
  popup_select_cache_mode = POPUP_SELECT_MODE_SINGLE;
  popup_select_cache_options = popup_select_visible_options(climate_fan_modes, climate_fan_mode);
  popup_select_cache_mask = 0;
  for (size_t idx = 0; idx < popup_select_cache_options.size(); ++idx) {
    if (popup_select_cache_options[idx] == climate_fan_mode) {
      popup_select_cache_mask = (1u << idx);
      break;
    }  // if (active fan mode)
  }  // for each option
  return true;
#else   // NSPANEL_EASY_PAGE_POPUP_SELECT
  (void) entity_id;
  return false;
#endif  // NSPANEL_EASY_PAGE_POPUP_SELECT
}

#ifdef NSPANEL_EASY_SUBSCRIBE
void climate_sub_render(const SubBinding &binding, const SubRuntime &rt, const char *state, bool /*visible*/) {
  if (strcmp(binding.component, "button08") != 0 && strcmp(binding.component, "button09") != 0) {
    ESP_LOGW(TAG, "'%s' is not a climate custom button", binding.component);
    return;
  }  // if (not a custom button)
  if (!rt.has_appearance) {
    return;  // Nothing to draw until the Blueprint has pushed the appearance in this session
  }  // if (no appearance)

  // The Blueprint's icon (override, or the entity's own icon) wins; otherwise resolved from the
  // domain, device class and state
  const bool active = (rt.last_state == SUB_STATE_ON);
  const char *icon = active ? rt.icon_on : rt.icon_off;
  uint16_t color = active ? rt.color_on : rt.color_off;
  if (icon[0] == '\0') {
    const SubAppearance look = resolve_binding_appearance(binding, state, rt.color_on, rt.color_off);
    if (look.icon != nullptr) {
      icon = look.icon;
      color = look.color;
    }  // if (resolved)
  }  // if (no icon from the Blueprint)
  if (icon[0] == '\0') {
    icon = Icons::MDI_ALERT_CIRCLE;
  }  // if (still no icon)
  const bool available = !api_stale && rt.last_state != SUB_STATE_UNAVAILABLE;
  ESP_LOGV(TAG, "%s: icon_color=%" PRIu16 " available=%s", binding.component, color, YESNO(available));
#ifdef NSPANEL_EASY_PAGE_BUTTONS
  // Drawn as the other buttons, so every button surface follows the same rules; only while the page is shown
  button_draw(get_page_id("climate"), binding.component, false, icon, color, 0, "", "", false, true, available, 1);
#endif  // NSPANEL_EASY_PAGE_BUTTONS
}
#endif  // NSPANEL_EASY_SUBSCRIBE

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_CLIMATE
