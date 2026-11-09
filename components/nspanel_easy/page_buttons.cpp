// page_buttons.cpp

#ifdef NSPANEL_EASY_PAGE_BUTTONS

#include "page_buttons.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>

#include "esphome/components/nextion/nextion.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"

#include "base.h"
#include "nextion_components.h"
#include "nextion_constants.h"
#include "pages.h"
#include "text.h"

#ifdef NSPANEL_EASY_HW_DISPLAY
#include "hw_display.h"
#endif  // NSPANEL_EASY_HW_DISPLAY

#ifdef NSPANEL_EASY_SUBSCRIBE

#include <cmath>
#include <cstdlib>
#include <map>

#include <esp_heap_caps.h>

#include "esphome/core/hal.h"

#endif  // NSPANEL_EASY_SUBSCRIBE

namespace esphome::nspanel_easy {

static const char *const TAG = "nspanel.page.buttons";

void button_draw(uint8_t page_id, const char *button_id, bool state, const char *icon, uint16_t icon_color,
                 uint8_t icon_font, const char *bri, const char *label, bool is_page_render, bool icon_only,
                 bool available, uint8_t bytes_per_char) {
  if (nextion_display == nullptr || current_page_id != page_id) {
    return;  // Boot has not handed the display over yet, or the page is not shown
  }

  // Availability is resolved once, here, for every button surface. `indicate`
  // forces the off appearance and greys the glyph; `hide` suppresses the
  // components entirely.
  const bool indicate = (!available && unavailable_behavior == UnavailableBehavior::INDICATE);
  const bool hide = (!available && unavailable_behavior == UnavailableBehavior::HIDE);
  const bool effective_state = state && !indicate;
  const uint16_t effective_icon_color = indicate ? Colors::RGB565_GRAY_DARK : icon_color;

  // Icon-only buttons (climate page custom buttons) are a single Nextion text
  // component holding the glyph. There are no `pic`/`bri`/`text` siblings to
  // address, and no on/off background, so only the glyph, its color and the
  // visibility are written.
  if (icon_only) {
    nextion_display->set_component_visibility(button_id, !hide);
    if (hide) {
      return;
    }
    nextion_display->set_component_text(button_id, icon);
    nextion_display->set_component_font_color(button_id, effective_icon_color);
    return;
  }  // if (icon_only)

#ifdef NSPANEL_EASY_HW_DISPLAY
  const bool light_theme = (current_theme == ThemeMode::LIGHT);
#else   // NSPANEL_EASY_HW_DISPLAY
  const bool light_theme = false;
#endif  // NSPANEL_EASY_HW_DISPLAY
  const uint8_t button_pic_id = light_theme ? BUTTON_PIC_LIGHT : BUTTON_PIC_DARK;
  const uint8_t button_pressed_pic_id = light_theme ? BUTTON_PIC_LIGHT_PRESSED : BUTTON_PIC_DARK_PRESSED;
  const uint16_t pic_id = effective_state ? button_pressed_pic_id : button_pic_id;
  const uint16_t fg_color = effective_state ? Colors::RGB565_BLUE_INDIGO : Colors::RGB565_WHITE;

  // Nextion object name limit: 14 chars + NUL = 15
  // Longest case: "button07" (8) + "icon"/"text" (4) = 12 chars - within limit
  if (strlen(button_id) > 10) {
    ESP_LOGE(TAG, "button_id '%s' exceeds 10 chars, skipping render", button_id);
    return;
  }
  char comp_pic[15];
  char comp_bri[15];
  char comp_text[15];
  char comp_icon[15];
  snprintf(comp_pic, sizeof(comp_pic), "%spic", button_id);
  snprintf(comp_bri, sizeof(comp_bri), "%sbri", button_id);
  snprintf(comp_text, sizeof(comp_text), "%stext", button_id);
  snprintf(comp_icon, sizeof(comp_icon), "%sicon", button_id);

  // Visibility is only ever written when hiding is enabled, or right after
  // leaving it, so users on the other two behaviours pay nothing for it.
  // `vis` resets to the TFT default on page entry, so a page render only
  // needs the hide direction, while a single button refresh must also be
  // able to bring the button back.
  if ((unavailable_behavior == UnavailableBehavior::HIDE || unavailable_unhide_pending) && (hide || !is_page_render)) {
    nextion_display->set_component_visibility(comp_pic, !hide);
    nextion_display->set_component_visibility(comp_bri, !hide);
    nextion_display->set_component_visibility(comp_text, !hide);
    nextion_display->set_component_visibility(comp_icon, !hide);
  }  // if (HIDE || unhide pending)
  if (hide) {
    return;
  }

  // Background images - skip for off-state buttons during page render
  // as Preinitialize already set the correct picc
  if (!is_page_render || effective_state) {
    nextion_display->set_component_picc(comp_pic, pic_id);
    nextion_display->set_component_picc(comp_bri, pic_id);
    nextion_display->set_component_picc(comp_text, pic_id);
    nextion_display->set_component_picc(comp_icon, pic_id);
  }  // if (!is_page_render || effective_state)

  // Foreground colors
  if (current_page_id != page_id) {
    return;
  }
  nextion_display->set_component_foreground_color(comp_bri, fg_color);
  nextion_display->set_component_foreground_color(comp_text, fg_color);
  nextion_display->set_component_font_color(comp_icon, effective_icon_color);
  nextion_display->set_component_font(comp_icon, icon_font);

  // Text content
  if (current_page_id != page_id) {
    return;
  }
  nextion_display->set_component_text(comp_icon, icon);
  nextion_display->set_component_text(comp_bri, (strcmp(bri, "0") == 0) ? " " : bri);

  // Button text is set on page render only
  if (is_page_render) {
    nextion_display->set_component_text(comp_text, wrapText(label, 10, bytes_per_char).c_str());
  }  // if (is_page_render)
}

#ifdef NSPANEL_EASY_SUBSCRIBE

/**
 * @brief What the subscriptions report for one button. Never persisted.
 *
 * All-zero is the unbound state, so the array can be allocated zeroed.
 */
struct ButtonSlot {
  uint16_t main;                     ///< Index of the entity's binding, plus one; 0 when unbound
  char state_prev[SUB_STATE_LEN];    ///< Previous state of the entity, to detect activations
  char value[SUB_STATE_LEN];         ///< Value attribute (brightness, position, ...) or last_triggered
  char color[SUB_STATE_LEN];         ///< Colour attribute: rgb_color (light) or action (humidifier)
  char state_entity[SUB_STATE_LEN];  ///< State of the State Indicator Entity
  char icon_attr[SUB_STATE_LEN];     ///< Last `icon` attribute, to report changes
  uint32_t flash_until;              ///< End of a momentary flash, in millis(); 0 when none
  bool has_state_entity;             ///< A State Indicator Entity is bound
  bool dirty;                        ///< Needs drawing
  bool page_render;                  ///< The next draw is a full page render (labels included)
};

/// @brief Settings of one button page, pushed by the Blueprint once per session. Never persisted.
struct ButtonPage {
  std::string label;                           ///< Page title
  std::string labels[BUTTONS_PER_PAGE];        ///< Button labels
  std::string state_values[BUTTONS_PER_PAGE];  ///< State Values; empty for the default on/off logic
  int32_t confirm{0};                          ///< Confirmation flags, bit 0 = button01
  bool configured{false};                      ///< Settings received in this session
  bool dirty{false};                           ///< Page title and confirmation flags need writing
};

/// @brief Button slots, BUTTON_PAGES x BUTTONS_PER_PAGE, allocated on first use (PSRAM when available).
static ButtonSlot *button_slots = nullptr;

static ButtonPage button_pages[BUTTON_PAGES];
static uint8_t button_icon_font = 8;
static std::string button_temperature_unit;
static uint8_t button_bytes_per_char = 1;

/// @brief When the current page first had something to draw, for the queue drain timeout; 0 when nothing pending.
static uint32_t button_pending_since = 0;

/// @brief States drawn as active by default, as in the Blueprint's enum.states.on.
static constexpr const char *BUTTON_ON_STATES[] = {
    "on",        "open",    "opening", "true",       "True",       "active",         "playing",
    "heat",      "cold",    "dry",     "armed_home", "armed_away", "armed_vacation", "armed_custom_bypass",
    "triggered", "pending", "arming",  "unlocked",   "1",
};

/**
 * @brief Allocate the slots on first use.
 *
 * @return The slot array, or nullptr when out of memory.
 */
static ButtonSlot *button_slots_get() {
  if (button_slots == nullptr) {
    const size_t bytes = sizeof(ButtonSlot) * BUTTON_PAGES * BUTTONS_PER_PAGE;
    button_slots = static_cast<ButtonSlot *>(heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM));
    if (button_slots == nullptr) {
      button_slots = static_cast<ButtonSlot *>(heap_caps_calloc(1, bytes, MALLOC_CAP_INTERNAL));
    }
    if (button_slots == nullptr) {
      ESP_LOGE(TAG, "Out of memory for the button pages");
    }
  }  // if (button_slots == nullptr)
  return button_slots;
}

/**
 * @brief Index of a button page from its name.
 *
 * @param page Page name, e.g. "buttonpage02".
 * @return 0-based index, or UINT8_MAX when the name is not a button page.
 */
static uint8_t button_page_index(const char *page) {
  if (strncmp(page, "buttonpage", 10) != 0 || strlen(page) != 12) {
    return UINT8_MAX;
  }
  const int number = ((page[10] - '0') * 10) + (page[11] - '0');
  return (number >= 1 && number <= BUTTON_PAGES) ? static_cast<uint8_t>(number - 1) : UINT8_MAX;
}

/**
 * @brief Index of a button page from its page id.
 *
 * @param page_id Page id.
 * @return 0-based index, or UINT8_MAX when the page is not a button page.
 */
static uint8_t button_page_index(uint8_t page_id) {
  for (uint8_t idx = 0; idx < BUTTON_PAGES; ++idx) {
    char name[13];
    snprintf(name, sizeof(name), "buttonpage%02u", static_cast<unsigned>(idx + 1));
    if (get_page_id(name) == page_id) {
      return idx;
    }
  }  // for each button page
  return UINT8_MAX;
}

/**
 * @brief Parse a binding target into its button and role.
 *
 * @param page Page name, e.g. "buttonpage02".
 * @param component Component, e.g. "button03" or "button03:v".
 * @param page_idx Receives the 0-based page index.
 * @param button_idx Receives the 0-based button index.
 * @param role Receives the role character, or '\0' for the entity's state.
 * @return false when the target is not a button.
 */
static bool button_parse_target(const char *page, const char *component, uint8_t &page_idx, uint8_t &button_idx,
                                char &role) {
  page_idx = button_page_index(page);
  if (page_idx == UINT8_MAX || strncmp(component, "button", 6) != 0) {
    return false;
  }
  const size_t len = strlen(component);
  if (len == 8) {
    role = '\0';
  } else if (len == 10 && component[8] == ':') {
    role = component[9];
  } else {
    return false;
  }
  const int number = ((component[6] - '0') * 10) + (component[7] - '0');
  if (number < 1 || number > BUTTONS_PER_PAGE) {
    return false;
  }
  button_idx = static_cast<uint8_t>(number - 1);
  return true;
}

/**
 * @brief Whether an entity_id belongs to a domain.
 *
 * @param entity_id Full entity_id.
 * @param domain Domain name.
 * @return true when the entity is in the domain.
 */
static bool button_domain_is(const char *entity_id, const char *domain) {
  const size_t len = strlen(domain);
  return strncmp(entity_id, domain, len) == 0 && entity_id[len] == '.';
}

/**
 * @brief Parse a whole string as a finite number.
 *
 * @param text Text to parse.
 * @param out Receives the value.
 * @return false when the text is empty or not entirely a number.
 */
static bool button_parse_number(const char *text, float &out) {
  if (text == nullptr || text[0] == '\0') {
    return false;
  }
  char *end = nullptr;
  out = strtof(text, &end);
  return end != text && *end == '\0' && std::isfinite(out);
}

/**
 * @brief Parse an rgb_color attribute, as Home Assistant sends it: "[255, 180, 107]" or "(255, 180, 107)".
 *
 * @param text Attribute value.
 * @param out Receives the RGB565 colour.
 * @return false when the text is not three numbers.
 */
static bool button_parse_rgb(const char *text, uint16_t &out) {
  unsigned red = 0;
  unsigned green = 0;
  unsigned blue = 0;
  if (sscanf(text, "%*[[(] %u , %u , %u", &red, &green, &blue) != 3 || red > 255 || green > 255 || blue > 255) {
    return false;
  }
  out = static_cast<uint16_t>(((red >> 3) << 11) | ((green >> 2) << 5) | (blue >> 3));
  return true;
}

/**
 * @brief Whether a state change of the entity is an activation shown with a momentary flash.
 *
 * Buttons, input buttons and scenes report their last activation as their state.
 * An automation is also flashed when it is turned on or off. Automation and
 * script runs are detected from their last_triggered attribute instead.
 *
 * @param entity_id Entity of the button.
 * @param prev Previous state; empty before the first one.
 * @param now New state.
 * @return true when the change is an activation.
 */
static bool button_state_activation(const char *entity_id, const char *prev, const char *now) {
  if (prev[0] == '\0' || strcmp(prev, now) == 0 || strcmp(prev, "unavailable") == 0 ||
      sub_state_in(now, SUB_UNUSABLE_STATES)) {
    return false;
  }
  if (button_domain_is(entity_id, "button") || button_domain_is(entity_id, "input_button") ||
      button_domain_is(entity_id, "scene")) {
    return true;
  }
  if (button_domain_is(entity_id, "automation")) {
    return (strcmp(prev, "on") == 0 || strcmp(prev, "off") == 0) && (strcmp(now, "on") == 0 || strcmp(now, "off") == 0);
  }
  return false;
}

/**
 * @brief Mark a button for drawing.
 *
 * @param slot Button slot.
 */
static void button_mark_dirty(ButtonSlot &slot) {
  slot.dirty = true;
  if (button_pending_since == 0) {
    button_pending_since = millis() | 1;  // Never 0, which means nothing is pending
  }
}

/**
 * @brief Value shown next to the icon, as the Blueprint formatted it.
 *
 * @param entity_id Entity of the button.
 * @param state State of the entity.
 * @param value Value attribute.
 * @param out Receives the text, or an empty string for none.
 * @param size Size of out.
 */
static void button_format_value(const char *entity_id, const char *state, const char *value, char *out, size_t size) {
  out[0] = '\0';
  float number = 0.0f;
  if (sub_state_in(state, SUB_UNUSABLE_STATES) || !button_parse_number(value, number)) {
    return;
  }
  if (button_domain_is(entity_id, "light")) {
    if (strcmp(state, "on") == 0) {
      snprintf(out, size, "%d%%", static_cast<int>(lroundf(number * 100.0f / 255.0f)));
    }
  } else if (button_domain_is(entity_id, "fan")) {
    if (strcmp(state, "on") == 0 && lroundf(number) > 0) {
      snprintf(out, size, "%ld%%", lroundf(number));
    }
  } else if (button_domain_is(entity_id, "cover")) {
    if (strcmp(state, "open") == 0 || strcmp(state, "opening") == 0 || strcmp(state, "closing") == 0) {
      snprintf(out, size, "%ld%%", lroundf(number));
    }
  } else if (button_domain_is(entity_id, "climate")) {
    const std::string &unit = button_temperature_unit;
    const bool whole =
        number >= 100.0f || unit == "F" || unit == "f" || unit == "°F" || unit == "°f" || unit == "K" || unit == "k";
    snprintf(out, size, whole ? "%.0f%s" : "%.1f%s", number, unit.c_str());
  } else if (button_domain_is(entity_id, "humidifier")) {
    snprintf(out, size, "%ld%%", lroundf(number));
  }  // by domain
  if (strcmp(out, "0%") == 0) {
    out[0] = '\0';  // A zero value is not shown
  }
}

/**
 * @brief Icon colour from the colour attribute, for the domains that have one.
 *
 * @param entity_id Entity of the button.
 * @param state State of the entity.
 * @param color Colour attribute: rgb_color (light) or action (humidifier).
 * @param color_off Colour of the inactive state.
 * @param out Receives the colour.
 * @return false when the domain has no colour attribute, or it gives no colour.
 */
static bool button_attribute_color(const char *entity_id, const char *state, const char *color, uint16_t color_off,
                                   uint16_t &out) {
  if (button_domain_is(entity_id, "light")) {
    return strcmp(state, "on") == 0 && button_parse_rgb(color, out);
  }
  if (button_domain_is(entity_id, "humidifier")) {
    if (strcmp(state, "off") == 0 || strcmp(color, "off") == 0) {
      out = color_off;
      return true;
    }
    if (strcmp(color, "humidifying") == 0) {
      out = Colors::RGB565_BLUE;
      return true;
    }
    if (strcmp(color, "drying") == 0) {
      out = Colors::RGB565_ORANGE;
      return true;
    }
    return false;
  }  // humidifier
  if (button_domain_is(entity_id, "timer")) {
    if (strcmp(state, "active") == 0) {
      out = Colors::RGB565_GREEN;
      return true;
    }
    if (strcmp(state, "paused") == 0) {
      out = Colors::RGB565_YELLOW;  // Amber
      return true;
    }
  }  // timer
  return false;
}

/**
 * @brief Draw one button from what its subscriptions reported.
 *
 * @param page_idx 0-based page index.
 * @param button_idx 0-based button index.
 * @param slot Button slot.
 * @param page_render Full page render (labels included).
 */
static void button_draw_slot(uint8_t page_idx, uint8_t button_idx, ButtonSlot &slot, bool page_render) {
  char page_name[13];
  snprintf(page_name, sizeof(page_name), "buttonpage%02u", static_cast<unsigned>(page_idx + 1));
  char button_id[9];
  snprintf(button_id, sizeof(button_id), "button%02u", static_cast<unsigned>(button_idx + 1));
  const uint8_t page_id = get_page_id(page_name);
  const ButtonPage &page = button_pages[page_idx];

  if (slot.main == 0) {
    return;  // Unbound: the button keeps the page's defaults
  }
  const uint16_t idx = slot.main - 1;
  const SubBinding &binding = sub_bindings[idx];
  const SubRuntime &rt = sub_runtime[idx];
  if (!rt.has_appearance) {
    return;  // Nothing to draw until the Blueprint has pushed the appearance in this session
  }

  const char *entity_id = binding.entity;
  const char *entity_state = rt.state;
  const bool available = !api_stale && strcmp(entity_state, "unavailable") != 0;

  // Button state (pressed background): the State Value when set, otherwise the
  // on/off logic of the State Indicator Entity (or the entity itself)
  const char *indicator_state = slot.has_state_entity ? slot.state_entity : entity_state;
  const std::string &state_value = page.state_values[button_idx];
  bool state = false;
  if (!state_value.empty()) {
    state = (state_value == indicator_state);
  } else {
    state =
        (sub_state_in(indicator_state, BUTTON_ON_STATES) &&
         (!button_domain_is(entity_id, "automation") || slot.has_state_entity)) ||
        (button_domain_is(entity_id, "climate") && indicator_state[0] != '\0' && strcmp(indicator_state, "off") != 0);
  }  // if (state_value)
  if (slot.flash_until != 0 && !slot.has_state_entity) {
    state = true;  // Momentary activation
  }

  // Icon and colour: the Blueprint's icons for the active and inactive states,
  // resolved on the device for the domains with more states
  const bool active = (rt.last_state == SUB_STATE_ON);
  const char *icon = active ? rt.icon_on : rt.icon_off;
  uint16_t color = active ? rt.color_on : rt.color_off;
  if (icon[0] == '\0') {
    const char *effective = rt.state;
    if (binding.domain == SUB_DOMAIN_CLIMATE && rt.action[0] != '\0' && !sub_state_in(rt.action, SUB_UNUSABLE_STATES)) {
      effective = rt.action;
    }
    const SubAppearance look = resolve_sub_appearance(static_cast<SubDomain>(binding.domain), binding.device_class,
                                                      effective, rt.color_on, rt.color_off);
    if (look.icon != nullptr) {
      icon = look.icon;
      color = look.color;
    }
  }  // if (icon[0] == '\0')
  if (icon[0] == '\0') {
    icon = "\uE027";  // mdi:alert-circle
  }
  uint16_t attribute_color = 0;
  if (button_attribute_color(entity_id, entity_state, slot.color, rt.color_off, attribute_color)) {
    color = attribute_color;
  }

  char bri[24];
  button_format_value(entity_id, entity_state, slot.value, bri, sizeof(bri));

  ESP_LOGV(TAG, "%s.%s: state=%s icon_color=%" PRIu16 " bri='%s' available=%s", page_name, button_id, ONOFF(state),
           color, bri, YESNO(available));
  button_draw(page_id, button_id, state, icon, color, button_icon_font, bri, page.labels[button_idx].c_str(),
              page_render, false, available, button_bytes_per_char);
}

void buttons_setup(const std::string &page, const std::string &page_label, const std::vector<std::string> &labels,
                   const std::vector<std::string> &state_values, int32_t confirm, int32_t icon_font,
                   const std::string &temperature_unit, uint8_t bytes_per_char) {
  const uint8_t page_idx = button_page_index(page.c_str());
  if (page_idx == UINT8_MAX) {
    ESP_LOGW(TAG, "'%s' is not a button page", page.c_str());
    return;
  }
  ButtonPage &settings = button_pages[page_idx];
  settings.label = page_label;
  for (uint8_t idx = 0; idx < BUTTONS_PER_PAGE; ++idx) {
    settings.labels[idx] = (idx < labels.size()) ? labels[idx] : "";
    settings.state_values[idx] = (idx < state_values.size()) ? state_values[idx] : "";
  }  // for each button
  settings.confirm = confirm;
  settings.configured = true;
  button_icon_font = (icon_font > 0 && icon_font < 256) ? static_cast<uint8_t>(icon_font) : 8;
  button_temperature_unit = temperature_unit;
  button_bytes_per_char = (bytes_per_char > 0) ? bytes_per_char : 1;

  // A page already showing is rendered again, so new labels show without leaving it
  if (button_page_index(current_page_id) == page_idx) {
    buttons_page_open(current_page_id);
  }
}

void buttons_page_open(uint8_t page_id) {
  const uint8_t page_idx = button_page_index(page_id);
  if (page_idx == UINT8_MAX) {
    return;
  }
  ButtonSlot *slots = button_slots_get();
  if (slots == nullptr) {
    return;
  }
  button_pages[page_idx].dirty = true;
  for (uint8_t idx = 0; idx < BUTTONS_PER_PAGE; ++idx) {
    ButtonSlot &slot = slots[(page_idx * BUTTONS_PER_PAGE) + idx];
    slot.page_render = true;
    button_mark_dirty(slot);
  }  // for each button
}

void buttons_loop() {
  ButtonSlot *slots = button_slots;
  if (slots == nullptr) {
    return;  // Nothing bound or set up yet
  }

  // Momentary flashes end on time on every page, so a page opened later draws the button off
  const uint32_t now = millis();
  for (uint16_t idx = 0; idx < BUTTON_PAGES * BUTTONS_PER_PAGE; ++idx) {
    ButtonSlot &slot = slots[idx];
    if (slot.flash_until != 0 && static_cast<int32_t>(now - slot.flash_until) >= 0) {
      slot.flash_until = 0;
      button_mark_dirty(slot);
    }
  }  // for each slot

  if (button_pending_since == 0) {
    return;
  }
  const uint8_t page_idx = button_page_index(current_page_id);
  if (page_idx == UINT8_MAX || nextion_display == nullptr) {
    button_pending_since = 0;  // Drawn by buttons_page_open() when a button page opens
    return;
  }

  // Wait for the Nextion queue to drain before drawing. Workaround for
  // esphome/esphome#19018: with `command_spacing`, a command sent directly while
  // spacing-deferred entries are still waiting is queued behind them, and its ack
  // removes an entry that was never sent, so one component keeps its previous
  // appearance. The timeout keeps a stuck entry from blocking the draw.
  const size_t queued = nextion_display->queue_size();
  if (queued > 0 && (now - button_pending_since) < BUTTON_RENDER_QUEUE_TIMEOUT_MS) {
    return;
  }
  if (queued > 0) {
    ESP_LOGD(TAG, "Render queue wait timed out (%zu queued)", queued);
  }
  button_pending_since = 0;

  ButtonPage &page = button_pages[page_idx];
  if (page.dirty) {
    page.dirty = false;
    if (page.configured) {
      nextion_display->set_component_text("page_label", page.label.c_str());
      nextion_display->set_component_value("confirm", page.confirm);
    }
  }  // if (page.dirty)

  for (uint8_t idx = 0; idx < BUTTONS_PER_PAGE; ++idx) {
    ButtonSlot &slot = slots[(page_idx * BUTTONS_PER_PAGE) + idx];
    if (!slot.dirty) {
      continue;
    }
    const bool page_render = slot.page_render;
    slot.dirty = false;
    slot.page_render = false;
    button_draw_slot(page_idx, idx, slot, page_render);
  }  // for each button on the page
}

// `state` (hvac_action for a climate) and `visible` are not used: a button reads the raw strings from rt
void buttons_sub_render(const SubBinding &binding, const SubRuntime &rt, const char * /*state*/, bool /*visible*/) {
  uint8_t page_idx = 0;
  uint8_t button_idx = 0;
  char role = '\0';
  if (!button_parse_target(binding.page, binding.component, page_idx, button_idx, role)) {
    ESP_LOGW(TAG, "'%s.%s' is not a button", binding.page, binding.component);
    return;
  }
  ButtonSlot *slots = button_slots_get();
  if (slots == nullptr) {
    return;
  }
  ButtonSlot &slot = slots[(page_idx * BUTTONS_PER_PAGE) + button_idx];
  const uint16_t idx = static_cast<uint16_t>(&binding - sub_bindings);

  switch (role) {
    case '\0': {  // The entity's state
      slot.main = idx + 1;
      if (button_state_activation(binding.entity, slot.state_prev, rt.state)) {
        slot.flash_until = (millis() + BUTTON_MOMENTARY_MS) | 1;  // Never 0, which means no flash
      }
      strncpy(slot.state_prev, rt.state, sizeof(slot.state_prev) - 1);
      slot.state_prev[sizeof(slot.state_prev) - 1] = '\0';
      break;
    }  // case entity state

    case 'v': {  // Value attribute, or last_triggered for automations and scripts
      const bool run = button_domain_is(binding.entity, "automation") || button_domain_is(binding.entity, "script");
      if (run && slot.value[0] != '\0' && strcmp(slot.value, rt.state) != 0 &&
          !sub_state_in(rt.state, SUB_UNUSABLE_STATES)) {
        slot.flash_until = (millis() + BUTTON_MOMENTARY_MS) | 1;  // A new run started
      }
      strncpy(slot.value, rt.state, sizeof(slot.value) - 1);
      slot.value[sizeof(slot.value) - 1] = '\0';
      break;
    }  // case value

    case 'c': {  // Colour attribute
      strncpy(slot.color, rt.state, sizeof(slot.color) - 1);
      slot.color[sizeof(slot.color) - 1] = '\0';
      break;
    }  // case colour

    case 's': {  // State Indicator Entity
      slot.has_state_entity = true;
      strncpy(slot.state_entity, rt.state, sizeof(slot.state_entity) - 1);
      slot.state_entity[sizeof(slot.state_entity) - 1] = '\0';
      break;
    }  // case state entity

    case 'i': {  // The entity's own icon attribute
      // The first value is the one the Blueprint resolved for this session; only later changes are reported
      if (rt.state[0] == '\0' || strcmp(slot.icon_attr, rt.state) == 0) {
        return;
      }
      const bool report = slot.icon_attr[0] != '\0' && strncmp(rt.state, "mdi:", 4) == 0;
      strncpy(slot.icon_attr, rt.state, sizeof(slot.icon_attr) - 1);
      slot.icon_attr[sizeof(slot.icon_attr) - 1] = '\0';
      if (report) {
        char component[9];
        snprintf(component, sizeof(component), "button%02u", static_cast<unsigned>(button_idx + 1));
        ESP_LOGD(TAG, "%s.%s: icon changed to '%s'", binding.page, component, rt.state);
        fire_ha_event("icon_request", {{"page", binding.page}, {"component", component}, {"icon", rt.state}});
      }
      return;  // Nothing to draw until the Blueprint answers
    }  // case icon

    default:
      ESP_LOGW(TAG, "'%s.%s': unknown role", binding.page, binding.component);
      return;
  }  // switch (role)

  button_mark_dirty(slot);
}

#endif  // NSPANEL_EASY_SUBSCRIBE

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_BUTTONS
