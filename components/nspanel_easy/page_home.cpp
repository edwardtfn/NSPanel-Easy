// page_home.cpp

#ifdef NSPANEL_EASY_PAGE_HOME

#include "page_home.h"

#include <cctype>
#include <cinttypes>

#include "esphome/components/nextion/nextion.h"
#include "esphome/core/log.h"

#include "nextion_components.h"

#ifdef NSPANEL_EASY_SUBSCRIBE

#include <cstdio>

#include "esphome/core/helpers.h"

#include "text.h"

#endif  // NSPANEL_EASY_SUBSCRIBE

namespace esphome::nspanel_easy {

static const char *const TAG = "nspanel.page.home";

uint8_t parse_home_button_index(const char *component) {
  constexpr uint8_t PREFIX_LEN = 6;  // strlen("button")
  if (strlen(component) != PREFIX_LEN + 2 || strncmp(component, "button", PREFIX_LEN) != 0) {
    return UINT8_MAX;
  }
  const char tens = component[PREFIX_LEN];
  const char units = component[PREFIX_LEN + 1];
  if (!std::isdigit(static_cast<unsigned char>(tens)) || !std::isdigit(static_cast<unsigned char>(units))) {
    return UINT8_MAX;
  }
  const uint8_t number = static_cast<uint8_t>(((tens - '0') * 10) + (units - '0'));
  if (number < 1 || number > HOME_BUTTON_COUNT) {
    return UINT8_MAX;
  }
  return static_cast<uint8_t>(number - 1);
}

uint32_t home_vis_mask = 0;

bool home_vis_set(const char *component, bool visible) {
  if (nextion_display == nullptr) {
    return false;  // Boot has not handed the display over yet
  }

  for (const auto &entry : HOME_VIS_BITS) {
    if (strcmp(component, entry.component) != 0) {
      continue;
    }

    const uint32_t bit = (1UL << entry.bit);
    const uint32_t updated = visible ? (home_vis_mask | bit) : (home_vis_mask & ~bit);
    if (updated == home_vis_mask) {
      return false;  // Nothing changed; avoid a redundant Nextion write
    }
    home_vis_mask = updated;
    nextion_display->send_command_printf("vis_home=%" PRIu32, home_vis_mask);
    return true;
  }  // for each known home component

  ESP_LOGW(TAG, "'%s' has no visibility bit", component);
  return false;
}

void home_vis_resend() {
  if (nextion_display == nullptr) {
    ESP_LOGE(TAG, "Missing Nextion display pointer");
    return;
  }

  // Unconditional: home_vis_set() suppresses unchanged writes, so a display
  // restart needs the mask pushed again even though the shadow is unchanged.
  nextion_display->send_command_printf("vis_home=%" PRIu32, home_vis_mask);

  if (!is_home_page()) {
    return;  // The page's Preinitialize event will apply the mask on entry
  }

  // The page is already showing, so its Preinitialize event has run with
  // whatever the mask held at the time. Writing the variable does not repaint,
  // so each bit is applied to the live components as well.
  for (const auto &entry : HOME_VIS_BITS) {
    const bool visible = ((home_vis_mask >> entry.bit) & 1U) != 0;
    nextion_display->set_component_visibility(entry.component, visible);
  }
}

#ifdef NSPANEL_EASY_SUBSCRIBE

HomeButtonState home_button_states[HOME_BUTTON_COUNT] = {};

bool indoor_temp_bound = false;
bool indoor_temp_valid = false;
std::function<void()> indoor_temp_fallback;

/// @brief Last rendered text for each subscribed temperature component.
static char indoor_temp_text[TEMP_TEXT_LEN] = {};
static char outdoor_temp_text[TEMP_TEXT_LEN] = {};

/// @brief Whether the outdoor temperature component is currently shown.
static Visibility outdoor_temp_shown = Visibility::UNKNOWN;

/// @brief Whether a disconnection handed the indoor temperature to the embedded sensor.
static bool indoor_temp_offline = false;

/**
 * @brief Build the scoped Nextion name for a home custom button.
 *
 * Home is a global page and its buttons are global components, so the scoped
 * form resolves from any page. That is what allows a state change arriving
 * while another page is showing to be applied straight away, rather than
 * waiting for a repaint on entry.
 *
 * @param idx Zero-based slot index.
 * @param out Destination buffer.
 * @param size Size of the destination buffer.
 */
static void home_button_target(uint8_t idx, char *out, size_t size) {
  snprintf(out, size, "home.button%02" PRIu8, static_cast<uint8_t>(idx + 1));
}

/**
 * @brief Render a subscription binding onto a home custom button.
 *
 * Unlike a chip, a bound custom button is always visible: it renders the active
 * or the inactive appearance according to the classified state rather than
 * appearing and disappearing.
 *
 * @param binding The binding being rendered.
 * @param rt Runtime state, including blueprint-supplied appearance.
 * @param state Effective state string; hvac_action for climate when usable.
 * @param idx Zero-based slot index, already validated by the caller.
 */
static void home_button_render(const SubBinding &binding, const SubRuntime &rt, const char *state, uint8_t idx) {
  HomeButtonState &button = home_button_states[idx];
  button.bound = true;  // Blocks blueprint pushes for this slot

  // Nothing to draw until a binding push has supplied appearance. Appearance is
  // not persisted, so a button stays as the blueprint last left it until then.
  if (!rt.has_appearance) {
    return;
  }

  // An unavailable entity is handled before anything is resolved: the
  // behaviour is shared with the button pages and the climate custom buttons
  // through UnavailableBehavior, so the three surfaces cannot drift apart.
  const bool unavailable = (rt.last_state == SUB_STATE_UNAVAILABLE);
  if (unavailable && unavailable_behavior == UnavailableBehavior::HIDE) {
    if (button.shown) {
      button.shown = false;
      home_vis_set(binding.component, false);
    }  // if (button.shown)
    return;
  }  // if (unavailable && HIDE)

  // SUB_STATE_TRANSITIONAL and SUB_STATE_NEITHER both fall to the inactive
  // appearance, matching how an unavailable entity renders today.
  const bool active = (rt.last_state == SUB_STATE_ON);
  const char *icon = active ? rt.icon_on : rt.icon_off;
  uint16_t color = active ? rt.color_on : rt.color_off;

  // An icon pushed by the blueprint wins; otherwise resolve from the domain and
  // state, which is what the multi-state domains rely on.
  if (icon[0] == '\0') {
    const SubAppearance look = resolve_sub_appearance(static_cast<SubDomain>(binding.domain), binding.device_class,
                                                      state, rt.color_on, rt.color_off);
    if (look.icon != nullptr) {
      icon = look.icon;
      color = look.color;
    }
  }
  if (icon[0] == '\0') {
    ESP_LOGW(TAG, "%s has no icon", binding.component);
    return;
  }

  // Greying happens after resolution so the entity keeps its own icon: the
  // button reads as "this thing, currently unreachable" rather than as a
  // generic placeholder.
  if (unavailable && unavailable_behavior == UnavailableBehavior::INDICATE) {
    color = Colors::RGB565_GRAY_DARK;
  }  // if (unavailable && INDICATE)
  const size_t icon_len = strlen(icon);
  if (icon_len >= sizeof(button.icon)) {
    // A truncated copy would leave the strcmp below permanently unequal, so the
    // renderer would write to the Nextion on every state change.
    ESP_LOGW(TAG, "%s icon does not fit (%zu bytes); skipping", binding.component, icon_len);
    return;
  }

  const bool icon_changed = (strcmp(button.icon, icon) != 0);
  if (icon_changed) {
    memcpy(button.icon, icon, icon_len + 1);  // Length checked above, so the null terminator fits
  }

  const bool color_changed = (button.color != color);
  button.color = color;

  ESP_LOGV(TAG, "%s: icon='%s' color=%" PRIu16 " appearance=%s", binding.component, icon, color,
           YESNO(rt.has_appearance));

  // Scoped writes persist in display RAM and reach the live component when home
  // is showing, so no repaint on page entry is needed.
  char target[14];  // "home.buttonNN" + null terminator
  home_button_target(idx, target, sizeof(target));
  if (icon_changed) {
    nextion_display->set_component_text(target, button.icon);
  }
  if (color_changed) {
    nextion_display->set_component_font_color(target, color);
  }
  if (!button.shown) {
    // A bound button is always visible. The blueprint used to send this with
    // every icon push; nothing else sets it now.
    button.shown = true;
    home_vis_set(binding.component, true);
  }
}

#ifdef NSPANEL_EASY_USE_WEATHER

void home_weather_resolve() {
  if (nextion_display == nullptr) {
    return;  // Boot has not handed the display over yet
  }

  // is_new_device is always false: the "Easy" TFT project is abandoned.
  const WeatherPicVariant &variant =
      select_weather_variant(get_weather_pics(weather_condition_index), false, current_theme != ThemeMode::LIGHT);
  const uint16_t pic = sun_info.is_up ? variant.sun_up : variant.sun_down;

  // Index 0 resolves to a blank picture in both themes, so an unknown or
  // not-yet-received condition needs no visibility handling of its own.
  ESP_LOGD(TAG, "Update weather pic: %" PRIu16 " (condition %" PRIu8 ", sun up: %s)", pic, weather_condition_index,
           YESNO(sun_info.is_up));
  nextion_display->set_component_pic(hmi::home::WEATHER.name, static_cast<uint8_t>(pic));
}

/**
 * @brief Render a subscription binding onto the home weather picture.
 *
 * The picture depends on the condition, the sun elevation and the active theme,
 * so the condition is stored and the shared resolver does the rest. The same
 * resolver runs on theme changes and on sunrise/sunset.
 *
 * @param state Raw weather condition string from Home Assistant.
 */
static void home_weather_render(const char *state) {
  const uint8_t index = get_weather_index(state);
  if (index == weather_condition_index) {
    return;  // Nothing changed; avoid a redundant Nextion write
  }
  weather_condition_index = index;
  home_weather_resolve();
}

#endif  // NSPANEL_EASY_USE_WEATHER

/**
 * @brief Format a temperature the way every other page in the project does.
 *
 * Fahrenheit renders without decimals, Celsius with one, matching
 * display_embedded_temp() and the climate page. The value is rendered exactly
 * as Home Assistant reported it: no unit conversion is attempted, since a
 * subscribed entity carries no reliable native unit.
 *
 * @param value Temperature as reported by Home Assistant.
 * @param out Destination buffer.
 * @param size Size of the destination buffer.
 */
static void format_temperature(float value, char *out, size_t size) {
  char buffer[TEMP_TEXT_LEN];
  const char *separator = (units_separator_str != nullptr) ? units_separator_str->c_str() : "";
#if NSPANEL_EASY_HW_TEMPERATURE_IS_FAHRENHEIT
  snprintf(buffer, sizeof(buffer), "%.0f%s\u00b0F", value, separator);
#else   // Celsius
  snprintf(buffer, sizeof(buffer), "%.1f%s\u00b0C", value, separator);
#endif  // NSPANEL_EASY_HW_TEMPERATURE_IS_FAHRENHEIT

  // Guarded indexing: an empty separator would be undefined behaviour here.
  const char decimal =
      (decimal_separator_str == nullptr || decimal_separator_str->empty()) ? '.' : (*decimal_separator_str)[0];
  const std::string adjusted = adjustDecimalSeparator(buffer, decimal);
  strncpy(out, adjusted.c_str(), size - 1);
  out[size - 1] = '\0';
}

/**
 * @brief Render a subscription binding onto the indoor temperature.
 *
 * The subscription owns the component only while it has a usable number. When
 * the bound entity has no value, display_embedded_temp() takes back over and
 * shows the panel's own sensor, so the component is never left blank and needs
 * no visibility handling of its own.
 *
 * @param state Raw state string, or the current_temperature attribute for climate.
 */
static void home_indoor_temp_render(const char *state) {
  indoor_temp_bound = true;

  const auto value = parse_number<float>(state);
  if (!value.has_value()) {
    // Unavailable, unknown, empty or simply not a number. Hand the component
    // back rather than waiting for the next embedded sensor publication, which
    // can be minutes away.
    if (indoor_temp_valid) {
      indoor_temp_valid = false;
      indoor_temp_text[0] = '\0';
      ESP_LOGD(TAG, "Indoor temperature unavailable; falling back to the embedded sensor");
      if (indoor_temp_fallback) {
        indoor_temp_fallback();
      }
    }
    return;
  }

  indoor_temp_valid = true;
  char text[TEMP_TEXT_LEN];
  format_temperature(*value, text, sizeof(text));
  if (strcmp(indoor_temp_text, text) == 0) {
    return;  // Nothing changed; avoid a redundant Nextion write
  }
  strncpy(indoor_temp_text, text, sizeof(indoor_temp_text) - 1);
  indoor_temp_text[sizeof(indoor_temp_text) - 1] = '\0';

  nextion_display->set_component_text(hmi::home::INDR_TEMP.name, indoor_temp_text);
}

void home_api_connection_update(bool connected) {
  if (!connected) {
    // Only a valid subscribed value needs handing back; otherwise the embedded
    // sensor already owns the component.
    if (indoor_temp_bound && indoor_temp_valid) {
      ESP_LOGD(TAG, "API disconnected; indoor temperature value is stale");
      indoor_temp_offline = true;
      home_indoor_temp_render(SUB_UNAVAILABLE_STATES[0]);
    }  // if (indoor_temp_bound && indoor_temp_valid)
    return;
  }  // if (!connected)

  if (indoor_temp_offline) {
    indoor_temp_offline = false;
    // Renderers skip redundant writes, so re-applying every binding only
    // repaints what the disconnection changed.
    sub_render_all();
  }  // if (indoor_temp_offline)
}

/**
 * @brief Render a subscription binding onto the outdoor temperature.
 *
 * Shown while the bound entity reports a usable number, hidden otherwise. There
 * is no local fallback for this component, so hiding it is the only sensible
 * response to an unavailable entity.
 *
 * @param state Raw state string, or the temperature attribute for weather.
 */
static void home_outdoor_temp_render(const char *state) {
  const auto value = parse_number<float>(state);
  const bool visible = value.has_value();

  if (visible) {
    char text[TEMP_TEXT_LEN];
    format_temperature(*value, text, sizeof(text));
    if (strcmp(outdoor_temp_text, text) != 0) {
      strncpy(outdoor_temp_text, text, sizeof(outdoor_temp_text) - 1);
      outdoor_temp_text[sizeof(outdoor_temp_text) - 1] = '\0';
      nextion_display->set_component_text(hmi::home::OUTDOOR_TEMP.name, outdoor_temp_text);
    }
  } else {
    outdoor_temp_text[0] = '\0';
  }

  const Visibility wanted = visible ? Visibility::SHOWN : Visibility::HIDDEN;
  if (outdoor_temp_shown == wanted) {
    return;  // Visibility unchanged
  }
  outdoor_temp_shown = wanted;

  // The mask persists visibility across page loads; the scoped vis command
  // reaches the live component when home is the visible page.
  home_vis_set("outdoor_temp", visible);
  nextion_display->set_component_visibility(hmi::home::OUTDOOR_TEMP.name, visible);
}

void home_sub_render(const SubBinding &binding, const SubRuntime &rt, const char *state, bool visible) {
  if (nextion_display == nullptr) {
    return;  // Boot has not handed the display over yet
  }

  const uint8_t idx = parse_home_button_index(binding.component);
  if (idx != UINT8_MAX) {
    home_button_render(binding, rt, state, idx);
    return;
  }
#ifdef NSPANEL_EASY_USE_WEATHER
  if (strcmp(binding.component, "weather") == 0) {
    home_weather_render(state);
    return;
  }
#endif  // NSPANEL_EASY_USE_WEATHER
  if (strcmp(binding.component, "indr_temp") == 0) {
    home_indoor_temp_render(state);
    return;
  }
  if (strcmp(binding.component, "outdoor_temp") == 0) {
    home_outdoor_temp_render(state);
    return;
  }
  ESP_LOGW(TAG, "'%s' is not a subscribable home component", binding.component);
}

void home_button_repaint() {
  if (nextion_display == nullptr) {
    ESP_LOGE(TAG, "Missing Nextion display pointer");
    return;
  }

  // Scoped names throughout: this runs from resync_display, which fires on the
  // first non-boot page entry after a display restart, and that page is not
  // necessarily home.
  char target[14];  // "home.buttonNN" + null terminator
  for (uint8_t idx = 0; idx < HOME_BUTTON_COUNT; ++idx) {
    HomeButtonState &button = home_button_states[idx];
    if (!button.bound || button.icon[0] == '\0') {
      continue;  // Unbound slots stay with whatever the blueprint drew
    }
    home_button_target(idx, target, sizeof(target));
    nextion_display->set_component_text(target, button.icon);
    nextion_display->set_component_font_color(target, button.color);
    // `shown` is deliberately left alone: this repaints text and color only,
    // while visibility is restored separately by home_vis_resend() from the
    // mask. A bound button can now be hidden (UnavailableBehavior::HIDE), and
    // claiming it shown here would stop home_button_render() from ever
    // unhiding it once its entity returns.
  }  // for each custom button slot

  // Temperatures are repainted from the same shadow, for the same reason: the
  // display lost everything drawn on it.
  if (indoor_temp_bound && indoor_temp_valid && indoor_temp_text[0] != '\0') {
    nextion_display->set_component_text(hmi::home::INDR_TEMP.name, indoor_temp_text);
  }
  if (outdoor_temp_shown == Visibility::SHOWN && outdoor_temp_text[0] != '\0') {
    nextion_display->set_component_text(hmi::home::OUTDOOR_TEMP.name, outdoor_temp_text);
  }
}

#endif  // NSPANEL_EASY_SUBSCRIBE

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_HOME
