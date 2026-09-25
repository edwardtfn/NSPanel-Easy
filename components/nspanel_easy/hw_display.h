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

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_HW_DISPLAY
