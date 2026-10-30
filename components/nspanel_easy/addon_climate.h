// addon_climate.h

#pragma once

#ifdef NSPANEL_EASY_ADDON_CLIMATE_BASE

#include <cstdint>
#include <string>
#include "all_icons.h"
#include "nextion_constants.h"

namespace esphome::nspanel_easy {

// Global var for the friendly name of the embedded climate entity
extern std::string addon_climate_friendly_name;

extern bool is_addon_climate_visible;

// =============================================================================
// Climate Enumerations
// =============================================================================

/**
 * @enum ClimateAction
 * @brief Current action of the climate device.
 *
 * Represents what the climate device is currently doing.
 * Values match ESPHome's climate component definitions.
 */
enum ClimateAction : uint8_t {
  CLIMATE_ACTION_OFF = 0,      ///< Climate device is off (inactive or no power)
  CLIMATE_ACTION_COOLING = 2,  ///< Climate device is actively cooling
  CLIMATE_ACTION_HEATING = 3,  ///< Climate device is actively heating
  CLIMATE_ACTION_IDLE = 4,     ///< Climate device is idle (monitoring but no action needed)
  CLIMATE_ACTION_DRYING = 5,   ///< Climate device is drying
  CLIMATE_ACTION_FAN = 6       ///< Climate device is in fan only mode
};

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

/**
 * @enum ClimateChipVisibility
 * @brief Controls when the climate chip is shown on the home screen.
 *
 * Sent by the blueprint on boot via `action_component_val` with `page: "mem"`
 * and stored in the `climate_chip_visibility` global.
 */
enum ClimateChipVisibility : uint8_t {
  CLIMATE_CHIP_ACTIVE_ONLY = 0,  ///< Show chip only when actively heating/cooling/drying/fan
  CLIMATE_CHIP_ALWAYS = 1,       ///< Always show chip when a climate entity is configured
  CLIMATE_CHIP_NEVER = 2         ///< Never show the chip, regardless of climate state
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

// =============================================================================
// Climate Icon Lookup Tables
// =============================================================================

/**
 * @brief Lookup table for CLIMATE_ACTION_OFF with different modes.
 *
 * Array indices correspond to ClimateMode enum values (0-6):
 * - [0] CLIMATE_MODE_OFF
 * - [1] CLIMATE_MODE_HEAT_COOL
 * - [2] CLIMATE_MODE_COOL
 * - [3] CLIMATE_MODE_HEAT
 * - [4] CLIMATE_MODE_FAN_ONLY
 * - [5] CLIMATE_MODE_DRY
 * - [6] CLIMATE_MODE_AUTO
 *
 * When the climate device is OFF, the icon reflects the selected mode
 * but uses grey color to indicate inactive state.
 */
constexpr IconData climate_off_mode_icons[] = {
    {Icons::MDI_NONE, Colors::RGB565_BLACK},          // CLIMATE_MODE_OFF (0)
    {Icons::MDI_AUTORENEW, Colors::RGB565_GRAY},      // CLIMATE_MODE_HEAT_COOL (1)
    {Icons::MDI_SNOWFLAKE, Colors::RGB565_GRAY},      // CLIMATE_MODE_COOL (2)
    {Icons::MDI_FIRE, Colors::RGB565_GRAY},           // CLIMATE_MODE_HEAT (3)
    {Icons::MDI_FAN, Colors::RGB565_GRAY},            // CLIMATE_MODE_FAN_ONLY (4)
    {Icons::MDI_WATER_PERCENT, Colors::RGB565_GRAY},  // CLIMATE_MODE_DRY (5)
    {Icons::MDI_CALENDAR_SYNC, Colors::RGB565_GRAY}   // CLIMATE_MODE_AUTO (6)
};

/**
 * @brief Lookup table for active climate action states.
 *
 * Array indices correspond to ClimateAction enum values:
 * - [0-1] Unused (reserved)
 * - [2] CLIMATE_ACTION_COOLING
 * - [3] CLIMATE_ACTION_HEATING
 * - [4] CLIMATE_ACTION_IDLE
 * - [5] CLIMATE_ACTION_DRYING
 * - [6] CLIMATE_ACTION_FAN
 *
 * When the climate device is actively operating, the icon and color
 * reflect the current action being performed.
 *
 * @note Indices 0-1 are unused to maintain alignment with the
 *       ClimateAction enum values.
 */
constexpr IconData climate_action_icons[] = {
    {Icons::MDI_NONE, Colors::RGB565_BLACK},            // Unused index 0
    {Icons::MDI_NONE, Colors::RGB565_BLACK},            // Unused index 1
    {Icons::MDI_SNOWFLAKE, Colors::RGB565_BLUE},        // CLIMATE_ACTION_COOLING (2)
    {Icons::MDI_FIRE, Colors::RGB565_DEEP_ORANGE},      // CLIMATE_ACTION_HEATING (3)
    {Icons::MDI_THERMOMETER, Colors::RGB565_GRAY},      // CLIMATE_ACTION_IDLE (4)
    {Icons::MDI_WATER_PERCENT, Colors::RGB565_ORANGE},  // CLIMATE_ACTION_DRYING (5)
    {Icons::MDI_FAN, Colors::RGB565_CYAN}               // CLIMATE_ACTION_FAN (6)
};

/**
 * @struct ClimateModeButton
 * @brief HVAC mode button on the climate page, used for the embedded thermostat.
 *
 * Each button selects one ClimateMode. The button is shown only when the
 * thermostat traits support that mode, so the page matches the configuration.
 */
struct ClimateModeButton {
  const char *name;             ///< Component name, without page prefix (`vis` does not accept one)
  const char *font_color_path;  ///< Component path with page prefix, for the font color
  ClimateMode mode;             ///< HVAC mode selected by the button
  uint16_t color_active;        ///< Font color when the mode is active
  uint16_t color_inactive;      ///< Font color when the mode is supported but not active
};

/**
 * @brief HVAC mode buttons on the climate page, in display order.
 *
 * Components and modes match the `climate,hvac_mode,<mode>` events sent by the HMI.
 */
constexpr ClimateModeButton CLIMATE_MODE_BUTTONS[] = {
    {"button01", "climate.button01", CLIMATE_MODE_AUTO, Colors::RGB565_GREEN, Colors::RGB565_GRAY_LIGHT},
    {"button02", "climate.button02", CLIMATE_MODE_HEAT_COOL, Colors::RGB565_WHITE, Colors::RGB565_GRAY_LIGHT},
    {"button03", "climate.button03", CLIMATE_MODE_HEAT, Colors::RGB565_DEEP_ORANGE, Colors::RGB565_GRAY_LIGHT},
    {"button04", "climate.button04", CLIMATE_MODE_COOL, Colors::RGB565_BLUE, Colors::RGB565_GRAY_LIGHT},
    {"button05", "climate.button05", CLIMATE_MODE_DRY, Colors::RGB565_ORANGE, Colors::RGB565_GRAY_LIGHT},
    {"button06", "climate.button06", CLIMATE_MODE_FAN_ONLY, Colors::RGB565_CYAN, Colors::RGB565_GRAY_LIGHT},
    {"button07", "climate.button07", CLIMATE_MODE_OFF, Colors::RGB565_BLUE_INDIGO, Colors::RGB565_GRAY},
};

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_ADDON_CLIMATE_BASE
