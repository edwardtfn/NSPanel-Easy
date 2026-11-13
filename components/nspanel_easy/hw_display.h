// hw_display.h

#pragma once

#ifdef NSPANEL_EASY_HW_DISPLAY

#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "esphome/core/defines.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "pages.h"
#include "base.h"

#ifdef NSPANEL_EASY_SUBSCRIBE
#include "api_subscriptions.h"
#endif  // NSPANEL_EASY_SUBSCRIBE

namespace esphome::nspanel_easy {

/**
 * @brief Display theme modes.
 *
 * Stored as a single byte (uint8_t) for use in Nextion EEPROM
 * and ESPHome globals with restore_value.
 */
enum class ThemeMode : uint8_t {
  DARK = 0,   ///< Dark theme  - light text on dark background
  LIGHT = 1,  ///< Light theme - dark text on light background
};
extern ThemeMode current_theme;  ///< Active display theme

extern uint8_t brightness_current;
/**
 * @brief Holds the display at the dimmed brightness when timers are reset.
 *
 * Raised by a hardware button press when `wakeup_with_button_press_dimmed` is enabled, either
 * while waking the display or while it is already dimmed. While raised, `timer_dim` restores the
 * dimmed brightness instead of the regular one. Cleared by a touch on the screen or when the
 * display goes to sleep.
 */
extern bool brightness_dim_hold;
extern bool display_entity_keep;
extern uint8_t display_mode_eeprom;

/**
 * @brief ID of each display model, by the position of its option in the "Display model" selector.
 *
 * The ID is what the display stores (EEPROM) and reports. It was the position plus one until the
 * model with the buttons on the left was added: that one is listed next to the US Landscape model,
 * but its ID comes after Blank's, as the IDs already stored on the displays cannot change.
 *
 * Positions: EU, US, US Landscape, US Landscape (buttons on the left), Blank.
 */
constexpr uint8_t DISPLAY_MODE_IDS[] = {1, 2, 3, 5, 4};

/// @brief Number of display models.
constexpr uint8_t DISPLAY_MODE_COUNT = sizeof(DISPLAY_MODE_IDS) / sizeof(DISPLAY_MODE_IDS[0]);

/**
 * @brief ID of the display model at a position of the "Display model" selector.
 *
 * @param index Position of the option.
 * @return The ID stored on the display, or 0 when the position is not a display model.
 */
constexpr uint8_t display_mode_id_of(size_t index) { return index < DISPLAY_MODE_COUNT ? DISPLAY_MODE_IDS[index] : 0; }

/**
 * @brief Position in the "Display model" selector of the display model with an ID.
 *
 * @param mode_id The ID stored on the display.
 * @return The position of the option, or UINT8_MAX when the ID is not a display model.
 */
constexpr uint8_t display_mode_index_of(uint8_t mode_id) {
  for (uint8_t index = 0; index < DISPLAY_MODE_COUNT; ++index) {
    if (DISPLAY_MODE_IDS[index] == mode_id) {
      return index;
    }  // if (found)
  }  // for each display model
  return UINT8_MAX;
}
extern bool display_portrait;
extern bool display_valid;

/**
 * @brief Title and icon override for the entity details page.
 *
 * Set by the Blueprint through `component_text_list` (`page: mem`, `id: details_overlap`)
 * right before `entity_details_show`, from the button that opened the page. Reported back
 * in the `page_changed` event only while `entity` matches the detailed entity, so a page
 * opened by any other caller never shows a stale override. Cleared together with the
 * detailed entity when the panel leaves the entity details pages.
 */
struct DetailsOverlap {
  std::string entity;  ///< Entity the override belongs to
  std::string title;   ///< Page title, blank to use the entity's name
  std::string icon;    ///< Icon as "mdi:<name>", blank to use the entity's icon
};
extern DetailsOverlap details_overlap;  ///< Override for the entity details page

/**
 * @brief Optional local handler for a click, tried before it reaches Home Assistant.
 *
 * Assigned from a page package that needs to act on a click itself rather than
 * forward it, currently the home page routing a custom button through the
 * confirmation dialog. Returning true consumes the event; whatever handled it
 * is responsible for re-emitting the click if the user goes ahead.
 *
 * @param page Page that produced the event, as reported by the display.
 * @param event "short_click" or "long_click".
 * @param component Unscoped component name, e.g. "button01".
 * @return true when the click was handled locally and must not reach Home Assistant.
 */
using ClickInterceptor =
    std::function<bool(const std::string &page, const std::string &event, const std::string &component)>;
extern ClickInterceptor click_interceptor;

#ifdef NSPANEL_EASY_SUBSCRIBE
/**
 * @brief Invoked when the wake-up sensors change between idle and active.
 *
 * Assigned from hw_display.yaml, where the wake-up and timer scripts are visible.
 * Called with true whenever a sensor turns active (each activation wakes the display
 * and resets its timers), and with false once the last active sensor is released, so
 * the dim and sleep timers start counting from the end of the activity.
 */
extern std::function<void(bool active)> wakeup_sensors_changed;

/**
 * @brief Whether any wake-up sensor is active.
 *
 * While true, the display is kept on: the dim and sleep timers do not act when they expire.
 *
 * @return true when at least one wake-up sensor is active.
 */
bool wakeup_sensors_active();

/**
 * @brief Follow the connection to Home Assistant for the wake-up sensors.
 *
 * Acts only on connection changes. When the connection is lost, every wake-up sensor is
 * released, as their states can no longer be trusted, so a sensor that was active cannot
 * keep the display on indefinitely. Their cached states (and climate hvac_action) are also
 * forgotten, so the states Home Assistant sends again on reconnection are applied, even when
 * unchanged.
 *
 * @param connected Whether Home Assistant is connected with state subscriptions.
 */
void wakeup_sensors_connection_update(bool connected);

/**
 * @brief Track a subscription binding targeting the wake-up sensors.
 *
 * Registered for the "wakeup" page in sub_resolve_renderer(). Nothing is drawn: the
 * binding only reports whether its sensor is active, which it never is while Home Assistant
 * is not connected. Bindings are re-applied on every page change, so only transitions are
 * acted upon.
 *
 * @param binding Binding being applied.
 * @param rt Runtime state, holding the classification of the sensor state.
 * @param state Unused: the classification in rt is used instead.
 * @param visible Unused: wake-up sensors are never inverted.
 */
void wakeup_sensor_render(const SubBinding &binding, const SubRuntime &rt, const char *state, bool visible);
#endif  // NSPANEL_EASY_SUBSCRIBE

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_HW_DISPLAY
