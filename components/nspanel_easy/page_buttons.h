// page_buttons.h

#pragma once

#ifdef NSPANEL_EASY_PAGE_BUTTONS

#include <cstdint>
#include <string>
#include <vector>

#include "nextion_components.h"

#ifdef NSPANEL_EASY_SUBSCRIBE
#include "api_subscriptions.h"
#endif  // NSPANEL_EASY_SUBSCRIBE

/**
 * @file page_buttons.h
 * @brief Drawing of the per-entity buttons, and the button pages driven by subscriptions.
 *
 * Every per-entity button (button pages and climate custom buttons) is drawn by
 * button_draw(), so behaviour added there reaches every surface.
 *
 * With subscriptions, the button pages are rendered entirely on the device. The
 * Blueprint pushes, once per session:
 *   - one binding per button for the state of its entity, carrying the icons and
 *     colours for the active and inactive states;
 *   - optional bindings for the value shown next to the icon (brightness,
 *     position, temperature, humidity), for the State Indicator Entity, for the
 *     run time of automations and scripts (momentary flash), and for the entity's
 *     own `icon` attribute;
 *   - the page settings (labels, State Values, confirmation flags), which are not
 *     persisted, through buttons_setup().
 *
 * The extra bindings use the button's component with a role suffix, e.g.
 * "button03:v", so every target stays unique for the subscription engine.
 *
 * An `icon` attribute change is resolved on the panel, with the icon names the
 * Blueprint sends once per session (see sub_icon_lookup()).
 */

namespace esphome::nspanel_easy {

static constexpr uint8_t BUTTON_PAGES = 4;      ///< buttonpage01 to buttonpage04
static constexpr uint8_t BUTTONS_PER_PAGE = 8;  ///< button01 to button08

static constexpr uint8_t BUTTON_PIC_DARK = 46;           ///< Button background, dark theme
static constexpr uint8_t BUTTON_PIC_DARK_PRESSED = 47;   ///< Active button background, dark theme
static constexpr uint8_t BUTTON_PIC_LIGHT = 79;          ///< Button background, light theme
static constexpr uint8_t BUTTON_PIC_LIGHT_PRESSED = 80;  ///< Active button background, light theme

/// @brief How long a button for an automation, button, input button, scene or script shows on after an activation.
static constexpr uint32_t BUTTON_MOMENTARY_MS = 800;

/// @brief Longest wait for the Nextion queue to drain before a subscription render is drawn anyway.
static constexpr uint32_t BUTTON_RENDER_QUEUE_TIMEOUT_MS = 25;

/**
 * @brief Draw one per-entity button.
 *
 * Availability is resolved here for every surface: `indicate` forces the off
 * appearance and greys the glyph; `hide` suppresses the components entirely.
 *
 * @param page_id Page the button belongs to; nothing is drawn when it is not the current page.
 * @param button_id Button name, e.g. "button03" (at most 10 characters).
 * @param state Active (pressed background) or not.
 * @param icon Icon codepoint.
 * @param icon_color RGB565 icon colour.
 * @param icon_font Nextion font of the icon.
 * @param bri Value shown next to the icon, or "0"/"" for none.
 * @param label Button label; only written on a page render.
 * @param is_page_render Full page render (skips what the page's preinitialization already set).
 * @param icon_only Single-component button (climate custom buttons).
 * @param available False when the bound entity is unavailable.
 * @param bytes_per_char Bytes per character of the display language, for label wrapping.
 */
void button_draw(uint8_t page_id, const char *button_id, bool state, const char *icon, uint16_t icon_color,
                 uint8_t icon_font, const char *bri, const char *label, bool is_page_render, bool icon_only,
                 bool available, uint8_t bytes_per_char);

#ifdef NSPANEL_EASY_SUBSCRIBE

/**
 * @brief Store the settings of one button page, pushed by the Blueprint once per session.
 *
 * @param page Page name, e.g. "buttonpage01".
 * @param page_label Page title.
 * @param labels Button labels, in button order.
 * @param state_values State Values, in button order; empty for the default on/off logic.
 * @param confirm Bit mask of the buttons asking for a confirmation (bit 0 = button01).
 * @param icon_font Nextion font of the button icons.
 * @param temperature_unit Unit shown after a climate's current temperature.
 * @param bytes_per_char Bytes per character of the display language, for label wrapping.
 */
void buttons_setup(const std::string &page, const std::string &page_label, const std::vector<std::string> &labels,
                   const std::vector<std::string> &state_values, int32_t confirm, int32_t icon_font,
                   const std::string &temperature_unit, uint8_t bytes_per_char);

/**
 * @brief Render a button page completely, as it opens.
 *
 * @param page_id Page that just opened; ignored when it is not a button page.
 */
void buttons_page_open(uint8_t page_id);

/**
 * @brief Draw pending button updates and end expired momentary flashes.
 *
 * Called periodically. Draws wait for the Nextion queue to drain (see
 * esphome/esphome#19018), for at most BUTTON_RENDER_QUEUE_TIMEOUT_MS.
 */
void buttons_loop();

/**
 * @brief Subscription renderer for the button pages.
 *
 * Stores what the binding carries and marks its button for drawing; the draw
 * itself happens in buttons_loop().
 */
void buttons_sub_render(const SubBinding &binding, const SubRuntime &rt, const char *state, bool visible);

#endif  // NSPANEL_EASY_SUBSCRIBE

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_BUTTONS
