// page_climate.h

#pragma once

#ifdef NSPANEL_EASY_PAGE_CLIMATE

#include <cstdint>
#include <string>
#include <vector>

#include "all_icons.h"
#include "entity_snapshot.h"
#include "nextion_components.h"  // For HMIComponent
#include "nextion_constants.h"

#ifdef NSPANEL_EASY_SUBSCRIBE
#include "api_subscriptions.h"  // For the climate custom buttons
#endif                          // NSPANEL_EASY_SUBSCRIBE

/**
 * @file page_climate.h
 * @brief Climate page: shared by the embedded thermostat (drawn from the climate add-on) and the
 *        Home Assistant climates (drawn from the Blueprint's entity snapshot, issue #636).
 */

namespace esphome::nspanel_easy {

extern bool climate_embedded_visible;

/// Popup select context used by the climate page preset picker.
constexpr const char *CLIMATE_PRESET_MODE_CONTEXT = "climate_preset_mode";

namespace hmi::climate {

/**
 * @namespace climate
 * @brief Components for the Climate page.
 *
 * Component ID mapping for the Climate page, based on the Nextion HMI design file.
 * Only the components referenced from ESPHome are listed here.
 * Names have no page prefix: the page has local scope, and `vis` does not accept one.
 */

constexpr HMIComponent PRESET_BUTTON = {"preset_button", 51};  ///< Preset mode button (3 chars max)

}  // namespace hmi::climate

// =============================================================================
// Climate Modes
// =============================================================================

/**
 * @enum ClimateMode
 * @brief Mode the climate device is set to.
 *
 * Represents the operating mode selected for the climate device.
 * Values match ESPHome's climate component definitions.
 */
enum ClimateMode : uint8_t {
  CLIMATE_MODE_OFF = 0,        ///< Climate device is set to off
  CLIMATE_MODE_HEAT_COOL = 1,  ///< Climate device is set to auto (heat/cool)
  CLIMATE_MODE_COOL = 2,       ///< Climate device is set to cool only
  CLIMATE_MODE_HEAT = 3,       ///< Climate device is set to heat only
  CLIMATE_MODE_FAN_ONLY = 4,   ///< Climate device is set to fan only
  CLIMATE_MODE_DRY = 5,        ///< Climate device is set to dry/dehumidify
  CLIMATE_MODE_AUTO = 6        ///< Climate device is set to automatic mode
};

// =============================================================================
// Climate Preset Names
// =============================================================================

/// Number of standard climate presets (values 0-7 of ESPHome's `climate::ClimatePreset`).
constexpr uint8_t CLIMATE_PRESET_NAMES_COUNT = 8;

/**
 * @brief Display names of ESPHome's standard climate presets.
 *
 * Array indices correspond to ESPHome's `climate::ClimatePreset` enum values (0-7):
 * - [0] CLIMATE_PRESET_NONE
 * - [1] CLIMATE_PRESET_HOME
 * - [2] CLIMATE_PRESET_AWAY
 * - [3] CLIMATE_PRESET_BOOST
 * - [4] CLIMATE_PRESET_COMFORT
 * - [5] CLIMATE_PRESET_ECO
 * - [6] CLIMATE_PRESET_SLEEP
 * - [7] CLIMATE_PRESET_ACTIVITY
 *
 * Written as users name them in the thermostat `preset` list. `ClimateCall::set_preset()`
 * matches standard preset names case-insensitively, so these can be sent back unchanged.
 */
constexpr const char *climate_preset_names[CLIMATE_PRESET_NAMES_COUNT] = {
    "None",     // CLIMATE_PRESET_NONE (0)
    "Home",     // CLIMATE_PRESET_HOME (1)
    "Away",     // CLIMATE_PRESET_AWAY (2)
    "Boost",    // CLIMATE_PRESET_BOOST (3)
    "Comfort",  // CLIMATE_PRESET_COMFORT (4)
    "Eco",      // CLIMATE_PRESET_ECO (5)
    "Sleep",    // CLIMATE_PRESET_SLEEP (6)
    "Activity"  // CLIMATE_PRESET_ACTIVITY (7)
};

/**
 * @brief Icons of ESPHome's standard climate presets, shown on the climate page preset button.
 *
 * Array indices correspond to ESPHome's `climate::ClimatePreset` enum values (0-7), as in
 * `climate_preset_names`. The icons match the ones Home Assistant uses for these presets.
 * `CLIMATE_PRESET_NONE` uses the generic preset icon, which is also used for custom presets.
 */
constexpr const char *climate_preset_icons[CLIMATE_PRESET_NAMES_COUNT] = {
    Icons::MDI_TUNE_VARIANT,         // CLIMATE_PRESET_NONE (0)
    Icons::MDI_HOME,                 // CLIMATE_PRESET_HOME (1)
    Icons::MDI_ACCOUNT_ARROW_RIGHT,  // CLIMATE_PRESET_AWAY (2)
    Icons::MDI_ROCKET_LAUNCH,        // CLIMATE_PRESET_BOOST (3)
    Icons::MDI_SOFA,                 // CLIMATE_PRESET_COMFORT (4)
    Icons::MDI_LEAF,                 // CLIMATE_PRESET_ECO (5)
    Icons::MDI_BED,                  // CLIMATE_PRESET_SLEEP (6)
    Icons::MDI_MOTION_SENSOR         // CLIMATE_PRESET_ACTIVITY (7)
};
// =============================================================================
// HVAC Mode Buttons
// =============================================================================

/**
 * @struct ClimateModeButton
 * @brief HVAC mode button on the climate page.
 *
 * Each button selects one HVAC mode. The button is shown only when the climate supports
 * that mode (the embedded thermostat's traits, or a Home Assistant climate's `hvac_modes`),
 * so the page matches the device. Both use the same colors.
 */
struct ClimateModeButton {
  const char *name;             ///< Component name, without page prefix (`vis` does not accept one)
  const char *font_color_path;  ///< Component path with page prefix, for the font color
  ClimateMode mode;             ///< HVAC mode selected by the button
  const char *hvac_mode;        ///< The same mode, as Home Assistant names it
  uint16_t color_active;        ///< Font color when the mode is active
  uint16_t color_inactive;      ///< Font color when the mode is supported but not active
};

/**
 * @brief HVAC mode buttons on the climate page, in display order.
 *
 * Components and modes match the `climate,hvac_mode,<mode>` events sent by the HMI.
 */
constexpr ClimateModeButton CLIMATE_MODE_BUTTONS[] = {
    {"button01", "climate.button01", CLIMATE_MODE_AUTO, "auto", Colors::RGB565_GREEN, Colors::RGB565_GRAY_LIGHT},
    {"button02", "climate.button02", CLIMATE_MODE_HEAT_COOL, "heat_cool", Colors::RGB565_WHITE,
     Colors::RGB565_GRAY_LIGHT},
    {"button03", "climate.button03", CLIMATE_MODE_HEAT, "heat", Colors::RGB565_DEEP_ORANGE, Colors::RGB565_GRAY_LIGHT},
    {"button04", "climate.button04", CLIMATE_MODE_COOL, "cool", Colors::RGB565_BLUE, Colors::RGB565_GRAY_LIGHT},
    {"button05", "climate.button05", CLIMATE_MODE_DRY, "dry", Colors::RGB565_ORANGE, Colors::RGB565_GRAY_LIGHT},
    {"button06", "climate.button06", CLIMATE_MODE_FAN_ONLY, "fan_only", Colors::RGB565_CYAN, Colors::RGB565_GRAY_LIGHT},
    {"button07", "climate.button07", CLIMATE_MODE_OFF, "off", Colors::RGB565_BLUE_INDIGO, Colors::RGB565_GRAY},
};

// =============================================================================
// Home Assistant Climates
// =============================================================================

/// @brief What the page shows for a Home Assistant climate, as `set_climate` takes it.
struct ClimateView {
  float current_temp{-999.0f};        ///< Current temperature; -999 when unknown
  uint32_t supported_features{0};     ///< Supported features
  float target_temp{-999.0f};         ///< Target temperature; -999 when not supported
  float target_temp_high{-999.0f};    ///< Upper target of a range; -999 when not supported
  float target_temp_low{-999.0f};     ///< Lower target of a range; -999 when not supported
  int32_t temp_step{10};              ///< Slider step, in tenths
  int32_t total_steps{0};             ///< Slider steps from the minimum to the maximum
  int32_t temp_offset{70};            ///< Slider minimum, in tenths, a multiple of the step
  const char *icon{Icons::MDI_NONE};  ///< Icon of the current action (or mode) next to the target
};

/**
 * @brief What the page shows for a Home Assistant climate.
 *
 * @param entity The climate, from the snapshot.
 * @return The values for `set_climate`.
 */
ClimateView climate_view(const SnapshotEntity &entity);

/**
 * @brief Draw the HVAC mode buttons of a Home Assistant climate: only its modes, the current one highlighted.
 *
 * @param entity The climate, from the snapshot.
 */
void climate_mode_buttons_render(const SnapshotEntity &entity);

/**
 * @brief Keep the presets of a Home Assistant climate for the picker, and draw the preset button.
 *
 * The button is shown only when the climate supports presets; its icon shows the active standard
 * preset (generic icon for none or a custom preset), and its color whether a preset is active.
 *
 * @param entity The climate, from the snapshot.
 */
void climate_presets_render(const SnapshotEntity &entity);

/**
 * @brief Fill the picker with the presets of the Home Assistant climate shown on the page.
 *
 * @param entity_id The climate, which the picker reports with the selected preset.
 * @return false when the climate has no presets.
 */
bool climate_presets_to_picker(const std::string &entity_id);

#ifdef NSPANEL_EASY_SUBSCRIBE
/**
 * @brief Render a subscription binding onto a climate page custom button (button08 or button09).
 *
 * @param binding The binding.
 * @param rt Its runtime state.
 * @param state Effective state.
 * @param visible Unused: the buttons are always shown, unless unavailable and hidden.
 */
void climate_sub_render(const SubBinding &binding, const SubRuntime &rt, const char *state, bool visible);
#endif  // NSPANEL_EASY_SUBSCRIBE

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_CLIMATE
