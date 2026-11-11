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

/// @brief What a slot of the mode row does: a kind, and an index for it.
struct ClimateSlot {
  ClimateSlotKind kind;  ///< Mode, preset, or not used
  uint8_t index;         ///< Index in CLIMATE_MODE_BUTTONS (mode) or in climate_preset_modes (preset)
};
/// @brief The slots of the mode row, as last laid out; each mode on its own button by default.
static ClimateSlot climate_slots[CLIMATE_SLOTS] = {
    {ClimateSlotKind::MODE, 0}, {ClimateSlotKind::MODE, 1}, {ClimateSlotKind::MODE, 2}, {ClimateSlotKind::MODE, 3},
    {ClimateSlotKind::MODE, 4}, {ClimateSlotKind::MODE, 5}, {ClimateSlotKind::MODE, 6},
};
/// @brief Whether the presets of the last climate rendered are shown as buttons in the mode row.
static bool climate_preset_buttons = false;

/// @brief Presets of the Home Assistant climate shown on the page, for the picker.
static std::vector<std::string> climate_preset_modes;
/// @brief Its active preset; empty when none.
static std::string climate_preset_mode;

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

/// @brief Whether a preset is the one that clears the selection.
static bool climate_preset_is_none(const std::string &preset) { return str_equals_case_insensitive(preset, "none"); }

/// @brief Keep the presets of a Home Assistant climate, and its active preset, from the snapshot.
static void climate_presets_update(const SnapshotEntity &entity) {
  const optional<uint32_t> features = parse_number<uint32_t>(entity.attribute("supported_features"));
  const std::vector<std::string> *modes = entity.list("preset_modes");
  const bool supported = (features.value_or(0) & 16) != 0 && modes != nullptr && !modes->empty();  // PRESET_MODE
  climate_preset_modes = supported ? *modes : std::vector<std::string>{};
  climate_preset_mode = climate_unusable(entity.attribute("preset_mode")) ? "" : entity.attribute("preset_mode");
}

/// @brief Icon of a preset: the one Home Assistant uses for a standard preset, else the generic one.
static const char *climate_preset_icon(const std::string &preset) {
  for (uint8_t idx = 1; idx < CLIMATE_PRESET_NAMES_COUNT; ++idx) {
    if (str_equals_case_insensitive(preset, climate_preset_names[idx])) {
      return climate_preset_icons[idx];
    }  // if (standard preset)
  }  // for each standard preset
  return climate_preset_icons[0];
}

void climate_slots_reset() {
  for (uint8_t slot = 0; slot < CLIMATE_SLOTS; ++slot) {
    climate_slots[slot] = {ClimateSlotKind::MODE, slot};
  }  // for each slot
  climate_preset_buttons = false;
}

bool climate_presets_as_buttons() { return climate_preset_buttons; }

ClimateSlotAction climate_slot_action(const uint8_t slot) {
  ClimateSlotAction action;
  if (slot >= CLIMATE_SLOTS) {
    return action;
  }  // if (not a slot)
  const ClimateSlot &entry = climate_slots[slot];
  if (entry.kind == ClimateSlotKind::MODE && entry.index < CLIMATE_SLOTS) {
    action.kind = ClimateSlotKind::MODE;
    action.value = CLIMATE_MODE_BUTTONS[entry.index].hvac_mode;
  } else if (entry.kind == ClimateSlotKind::PRESET && entry.index < climate_preset_modes.size()) {
    action.kind = ClimateSlotKind::PRESET;
    action.value = climate_preset_modes[entry.index];
    if (action.value == climate_preset_mode) {
      // A tap on the active preset leaves it, when the climate has a preset for that
      for (const std::string &preset : climate_preset_modes) {
        if (climate_preset_is_none(preset)) {
          action.value = preset;
          break;
        }  // if (the preset that clears the selection)
      }  // for each preset
    }  // if (active preset)
  }  // if (mode or preset)
  return action;
}

void climate_mode_buttons_render(const SnapshotEntity &entity) {
  climate_presets_update(entity);

  // The HVAC modes the climate supports, in display order, and its presets, without the one that clears them
  uint8_t mode_list[CLIMATE_SLOTS];
  uint8_t mode_count = 0;
  const std::vector<std::string> *modes = entity.list("hvac_modes");
  for (uint8_t idx = 0; idx < CLIMATE_SLOTS; ++idx) {
    if (modes != nullptr &&
        std::find(modes->begin(), modes->end(), CLIMATE_MODE_BUTTONS[idx].hvac_mode) != modes->end()) {
      mode_list[mode_count++] = idx;
    }  // if (supported)
  }  // for each mode
  uint8_t preset_list[CLIMATE_PRESET_BUTTONS_MAX];
  uint8_t preset_count = 0;
  bool presets_fit = true;
  for (size_t idx = 0; idx < climate_preset_modes.size() && idx <= UINT8_MAX; ++idx) {
    if (climate_preset_is_none(climate_preset_modes[idx])) {
      continue;
    }  // if (the preset that clears the selection)
    if (preset_count >= CLIMATE_PRESET_BUTTONS_MAX) {
      presets_fit = false;
      break;
    }  // if (too many presets)
    preset_list[preset_count++] = static_cast<uint8_t>(idx);
  }  // for each preset

  // Layout of the row. With a few presets, they are shown as buttons (#609): the modes from the left and
  // the presets from the right, so each group stays together. Otherwise each mode is on its own button.
  climate_preset_buttons = presets_fit && preset_count > 0 && (mode_count + preset_count) <= CLIMATE_SLOTS;
  for (uint8_t slot = 0; slot < CLIMATE_SLOTS; ++slot) {
    climate_slots[slot] = {ClimateSlotKind::NONE, 0};
  }  // for each slot
  if (climate_preset_buttons) {
    for (uint8_t idx = 0; idx < mode_count; ++idx) {
      climate_slots[idx] = {ClimateSlotKind::MODE, mode_list[idx]};
    }  // for each mode
    for (uint8_t idx = 0; idx < preset_count; ++idx) {
      climate_slots[CLIMATE_SLOTS - preset_count + idx] = {ClimateSlotKind::PRESET, preset_list[idx]};
    }  // for each preset
  } else {
    for (uint8_t idx = 0; idx < mode_count; ++idx) {
      climate_slots[mode_list[idx]] = {ClimateSlotKind::MODE, mode_list[idx]};
    }  // for each mode
  }  // if (presets as buttons)

  if (nextion_display == nullptr) {
    return;
  }  // if (no display)
  for (uint8_t slot = 0; slot < CLIMATE_SLOTS; ++slot) {
    const ClimateModeButton &button = CLIMATE_MODE_BUTTONS[slot];  // The component of the slot
    const ClimateSlot &entry = climate_slots[slot];
    if (entry.kind == ClimateSlotKind::MODE) {
      const ClimateModeButton &mode = CLIMATE_MODE_BUTTONS[entry.index];
      nextion_display->set_component_text(button.name, mode.icon);
      nextion_display->set_component_font_color(
          button.font_color_path, (entity.state == mode.hvac_mode) ? mode.color_active : mode.color_inactive);
      nextion_display->show_component(button.name);
    } else if (entry.kind == ClimateSlotKind::PRESET) {
      const std::string &preset = climate_preset_modes[entry.index];
      nextion_display->set_component_text(button.name, climate_preset_icon(preset));
      // Theme colors from the display, so the button follows the light and dark themes
      nextion_display->send_command_printf("%s.pco=%s", button.font_color_path,
                                           (preset == climate_preset_mode) ? "color_pco_main" : "color_pco_secondary");
      nextion_display->show_component(button.name);
    } else {
      nextion_display->hide_component(button.name);
    }  // if (mode, preset or not used)
  }  // for each slot
}

void climate_presets_render(const SnapshotEntity &entity) {
  climate_presets_update(entity);
  if (nextion_display == nullptr) {
    return;
  }  // if (no display)
  if (climate_preset_modes.empty() || climate_preset_buttons) {
    // No presets, or shown as buttons in the mode row (see climate_mode_buttons_render)
    nextion_display->hide_component(hmi::climate::PRESET_BUTTON.name);
    return;
  }  // if (no preset picker)

  // Standard presets get the icon Home Assistant uses for them; none or a custom preset, the generic one
  const bool active = !climate_preset_mode.empty() && !climate_preset_is_none(climate_preset_mode);
  const char *icon = climate_preset_icon(climate_preset_mode);
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
