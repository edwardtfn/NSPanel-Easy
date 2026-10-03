// page_home.h

#pragma once

#ifdef NSPANEL_EASY_PAGE_HOME

#include <cstdint>
#include <cstring>
#include <functional>

#include "nextion_components.h"

#ifdef NSPANEL_EASY_SUBSCRIBE
#include "api_subscriptions.h"
#endif  // NSPANEL_EASY_SUBSCRIBE

#ifdef NSPANEL_EASY_HW_DISPLAY
#include "hw_display.h"
#endif  // NSPANEL_EASY_HW_DISPLAY

#ifdef NSPANEL_EASY_USE_WEATHER
#include "weather.h"
#endif  // NSPANEL_EASY_USE_WEATHER

/**
 * @file page_home.h
 * @brief Subscription-driven home page components.
 *
 * Custom buttons: unlike chips, a bound custom button is always visible. It
 * renders icon_on or icon_off according to the entity's state rather than
 * appearing and disappearing. Unbound slots are left entirely to the blueprint.
 *
 * Weather picture: resolved from the condition, the sun elevation and the
 * active theme. Index 0 of the condition table is blank in both themes, so an
 * unknown or not-yet-received condition needs no visibility handling.
 *
 * Temperatures: rendered exactly as Home Assistant reports them, with no unit
 * conversion. Indoor falls back to the panel's own sensor when the bound entity
 * has no value; outdoor is hidden, having no local equivalent.
 */

namespace esphome::nspanel_easy {

inline bool is_home_page() { return current_page_id == get_page_id("home"); }

/**
 * @brief Bit positions in the Nextion vis_home bitmask.
 *
 * The display's home page Preinitialize event walks these bits to restore
 * visibility, since Nextion does not retain vis across page loads. Companion
 * components (*_icon) follow their parent and have no bit of their own.
 */
struct HomeVisBit {
  const char *component;  ///< Unscoped component name as the blueprint sends it
  uint8_t bit;            ///< Bit position in vis_home
};

static constexpr HomeVisBit HOME_VIS_BITS[] = {
    {"indr_temp", 0},     {"indr_temp_icon", 0}, {"outdoor_temp", 1},  {"bt_icon", 2},       {"bt_utilities", 3},
    {"bt_notific", 4},    {"bt_qrcode", 5},      {"bt_entities", 6},   {"button01", 7},      {"button02", 8},
    {"button03", 9},      {"button04", 10},      {"button05", 11},     {"button06", 12},     {"button07", 13},
    {"value01", 14},      {"value01_icon", 14},  {"value02", 15},      {"value02_icon", 15}, {"value03", 16},
    {"value03_icon", 16}, {"value04", 17},       {"value04_icon", 17},
};

extern uint32_t home_vis_mask;

bool home_vis_set(const char *component, bool visible);

void home_vis_resend();

/// @brief Number of custom button slots on the home page (button01..button07).
static constexpr uint8_t HOME_BUTTON_COUNT = 7;

/**
 * @brief Parse the slot index out of a custom button component name.
 *
 * Used as a probe by both the subscription renderer and the confirmation gate:
 * a component that is not a custom button is an ordinary outcome here, so no
 * warning is emitted. Callers report on their own terms.
 *
 * @param component Component name, expected as "button%02u".
 * @return Zero-based slot index, or UINT8_MAX if the name is not a custom button.
 */
uint8_t parse_home_button_index(const char *component);

#ifdef NSPANEL_EASY_SUBSCRIBE

/**
 * @brief Last rendered appearance of a home custom button.
 *
 * Lets a state change skip redundant Nextion writes, and lets the page be
 * repainted on entry without a round trip to Home Assistant.
 */
struct HomeButtonState {
  char icon[4];    ///< UTF-8 MDI codepoint (1-3 bytes + null terminator)
  uint16_t color;  ///< RGB565 foreground color
  bool bound;      ///< Driven by a subscription; the blueprint does not own it
  bool shown;      ///< Whether the component has been made visible
};
static_assert(sizeof(HomeButtonState::icon) >= 4, "Icons are BMP private-use codepoints: 3 bytes + null");

/// @brief Shadow state array; one entry per custom button slot.
extern HomeButtonState home_button_states[HOME_BUTTON_COUNT];

/// @brief Buffer size for a formatted temperature, e.g. "-12,3 \u00b0C".
static constexpr size_t TEMP_TEXT_LEN = 15;

/**
 * @brief Whether a subscription targets the indoor temperature component.
 *
 * Read by display_embedded_temp(), which must not write the component while the
 * subscription owns it.
 */
extern bool indoor_temp_bound;

/// @brief Whether the bound indoor entity last reported a usable number.
extern bool indoor_temp_valid;

/**
 * @brief Invoked when a bound indoor temperature stops being usable.
 *
 * Assigned from hw_temperature.yaml, where display_embedded_temp() is visible.
 * Without it the embedded sensor would only take back over on its next
 * publication, which can be minutes away.
 */
extern std::function<void()> indoor_temp_fallback;

/**
 * @brief Render a subscription binding onto its home page component.
 *
 * Registered for the "home" page in sub_resolve_renderer(), so every binding
 * targeting this page arrives here. Dispatches on the component name, since
 * renderers are resolved per page rather than per component. Receives an
 * already-classified state and decides only how to draw it.
 *
 * @param binding The binding being rendered.
 * @param rt Runtime state, including blueprint-supplied appearance.
 * @param state Effective state string; hvac_action for climate when usable.
 * @param visible Unused: home page targets manage their own visibility.
 */
void home_sub_render(const SubBinding &binding, const SubRuntime &rt, const char *state, bool visible);

/**
 * @brief Repaint every bound home component from its shadow state.
 *
 * Called when the home page is entered, since the Nextion side does not retain
 * what was drawn on a page that has been left. Covers the custom buttons and
 * both temperatures; the weather picture is repainted by home_weather_resolve().
 */
void home_button_repaint();

#ifdef NSPANEL_EASY_USE_WEATHER

/**
 * @brief Resolve and draw the home weather picture.
 *
 * Combines the stored condition index, the sun elevation and the active theme
 * into a single picture ID. Call after anything those three depend on changes:
 * a new condition from the subscription, a theme change, sunrise or sunset, and
 * once the sun coordinates have been applied.
 *
 * Safe to call before the display pointer has been handed over; it returns
 * without drawing in that case.
 */
void home_weather_resolve();

#endif  // NSPANEL_EASY_USE_WEATHER

#endif  // NSPANEL_EASY_SUBSCRIBE

namespace hmi::home {

/**
 * @namespace home
 * @brief Components for the Home page.
 *
 * Component ID mapping for the Home page (ID: 0)
 * Based on the Nextion HMI design file.
 */

// Time and date components
constexpr HMIComponent TIME = {"home.time", 3};           ///< Time display (5 chars max)
constexpr HMIComponent DATE = {"home.date", 6};           ///< Date display (25 chars max)
constexpr HMIComponent MERIDIEM = {"home.meridiem", 32};  ///< AM/PM indicator (6 chars max)

// Temperature and weather components
constexpr HMIComponent INDR_TEMP = {"home.indr_temp", 4};             ///< Indoor temperature (8 chars max)
constexpr HMIComponent INDR_TEMP_ICON = {"home.indr_temp_icon", 27};  ///< Indoor temp icon (3 chars max)
constexpr HMIComponent OUTDOOR_TEMP = {"home.outdoor_temp", 5};       ///< Outdoor temperature (8 chars max)
constexpr HMIComponent WEATHER = {"home.weather", 7};                 ///< Weather picture

// Value display components
constexpr HMIComponent VALUE01 = {"home.value01", 8};             ///< Value 1 text (30 chars max)
constexpr HMIComponent VALUE01_ICON = {"home.value01_icon", 21};  ///< Value 1 icon (3 chars max)
constexpr HMIComponent VALUE02 = {"home.value02", 25};            ///< Value 2 text (30 chars max)
constexpr HMIComponent VALUE02_ICON = {"home.value02_icon", 24};  ///< Value 2 icon (3 chars max)
constexpr HMIComponent VALUE03 = {"home.value03", 45};            ///< Value 3 text (30 chars max)
constexpr HMIComponent VALUE03_ICON = {"home.value03_icon", 44};  ///< Value 3 icon (3 chars max)
constexpr HMIComponent VALUE04 = {"home.value04", 22};            ///< Value 4 text (30 chars max)
constexpr HMIComponent VALUE04_ICON = {"home.value04_icon", 23};  ///< Value 4 icon (3 chars max)

// Chip components (top row)
constexpr HMIComponent CHIP_RELAY1 = {"home.chip_relay1", 11};    ///< Relay 1 chip (3 chars max)
constexpr HMIComponent CHIP_RELAY2 = {"home.chip_relay2", 12};    ///< Relay 2 chip (3 chars max)
constexpr HMIComponent CHIP_CLIMATE = {"home.chip_climate", 13};  ///< Climate chip (3 chars max)
constexpr HMIComponent CHIP01 = {"home.chip01", 14};              ///< Chip 1 (3 chars max)
constexpr HMIComponent CHIP02 = {"home.chip02", 15};              ///< Chip 2 (3 chars max)
constexpr HMIComponent CHIP03 = {"home.chip03", 16};              ///< Chip 3 (3 chars max)
constexpr HMIComponent CHIP04 = {"home.chip04", 17};              ///< Chip 4 (3 chars max)
constexpr HMIComponent CHIP05 = {"home.chip05", 18};              ///< Chip 5 (3 chars max)
constexpr HMIComponent CHIP06 = {"home.chip06", 19};              ///< Chip 6 (3 chars max)
constexpr HMIComponent CHIP07 = {"home.chip07", 20};              ///< Chip 7 (3 chars max)

// Button components
constexpr HMIComponent BUTTON01 = {"home.button01", 35};  ///< Button 1 (3 chars max)
constexpr HMIComponent BUTTON02 = {"home.button02", 36};  ///< Button 2 (3 chars max)
constexpr HMIComponent BUTTON03 = {"home.button03", 37};  ///< Button 3 (3 chars max)
constexpr HMIComponent BUTTON04 = {"home.button04", 40};  ///< Button 4 (3 chars max)
constexpr HMIComponent BUTTON05 = {"home.button05", 41};  ///< Button 5 (3 chars max)
constexpr HMIComponent BUTTON06 = {"home.button06", 42};  ///< Button 6 (3 chars max)
constexpr HMIComponent BUTTON07 = {"home.button07", 31};  ///< Button 7 (3 chars max)

// Bottom navigation buttons
constexpr HMIComponent BT_NOTIFIC = {"home.bt_notific", 28};      ///< Notifications button (3 chars max)
constexpr HMIComponent BT_QRCODE = {"home.bt_qrcode", 29};        ///< QR code button (3 chars max)
constexpr HMIComponent BT_ENTITIES = {"home.bt_entities", 30};    ///< Entities button (3 chars max)
constexpr HMIComponent BT_UTILITIES = {"home.bt_utilities", 43};  ///< Utilities button (3 chars max)

// System indicators
constexpr HMIComponent WIFI_ICON = {"home.wifi_icon", 26};  ///< WiFi status icon (5 chars max)
constexpr HMIComponent BT_ICON = {"home.bt_icon", 46};      ///< Bluetooth/system icon (5 chars max)

// Variables (for reference - these are not visual components)
constexpr HMIComponent VAR_LASTCLICK = {"home.lastclick", 33};    ///< Last click variable
constexpr HMIComponent VAR_CLICK_COMP = {"home.click_comp", 38};  ///< Clicked component variable

// Touch components
constexpr HMIComponent SWIPE = {"home.swipe", 1};  ///< Swipe gesture handler

// Timers (for reference)
constexpr HMIComponent TIMER_SWIPESTORE = {"home.swipestore", 2};        ///< Swipe store timer
constexpr HMIComponent TIMER_SETTINGS = {"home.settings_timer", 34};     ///< Settings timer
constexpr HMIComponent TIMER_CLICK = {"home.click_timer", 39};           ///< Click timer
constexpr HMIComponent TIMER_SHORT_CLICK = {"home.short_click_tm", 47};  ///< Short click timer

// Global localization
constexpr HMIComponent UNITS_SEP = {"home.units_sep", 48};  ///< Localization - Units separator

}  // namespace hmi::home
}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_HOME
