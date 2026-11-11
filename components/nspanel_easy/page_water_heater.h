// page_water_heater.h

#pragma once

#ifdef NSPANEL_EASY_PAGE_WATER_HEATER

#include <cstdint>
#include <string>

#include "all_icons.h"
#include "entity_snapshot.h"
#include "nextion_constants.h"

/**
 * @file page_water_heater.h
 * @brief Water heater page, also used for humidifiers, rendered from the Blueprint's entity snapshot (issue #636).
 */

namespace esphome::nspanel_easy {

/**
 * @struct WaterHeaterModeButton
 * @brief Operation mode button on the water heater page.
 *
 * The HMI reports each button as `water_heater,operation_mode,<mode>`. For a humidifier, the
 * first seven buttons hold its modes, in the order it reports them, and the last one is the power button.
 */
struct WaterHeaterModeButton {
  const char *name;       ///< Component name, without page prefix (`vis` does not accept one)
  const char *mode;       ///< Water heater operation mode, as the HMI reports it
  const char *icon;       ///< Icon of the mode
  uint16_t color_active;  ///< Font color when the mode is active
};

/// @brief Operation mode buttons, in display order. The colors follow the climate page.
constexpr WaterHeaterModeButton WATER_HEATER_MODE_BUTTONS[] = {
    {"button01", "on", Icons::MDI_WATER_BOILER, Colors::RGB565_ORANGE},
    {"button02", "eco", Icons::MDI_LEAF, Colors::RGB565_GREEN},
    {"button03", "electric", Icons::MDI_LIGHTNING_BOLT, Colors::RGB565_YELLOW},
    {"button04", "gas", Icons::MDI_FIRE, Colors::RGB565_DEEP_ORANGE},
    {"button05", "heat_pump", Icons::MDI_HEAT_PUMP, Colors::RGB565_BLUE},
    {"button06", "performance", Icons::MDI_HEAT_WAVE, Colors::RGB565_RED},
    {"button07", "high_demand", Icons::MDI_SPEEDOMETER, Colors::RGB565_RED},
    {"button08", "off", Icons::MDI_POWER, Colors::RGB565_BLUE_INDIGO},
};

/// @brief Number of operation mode buttons.
constexpr uint8_t WATER_HEATER_MODE_BUTTONS_COUNT = sizeof(WATER_HEATER_MODE_BUTTONS) / sizeof(WaterHeaterModeButton);

/// @brief Font color of a supported mode that is not active, as on the climate page.
constexpr uint16_t WATER_HEATER_MODE_INACTIVE_COLOR = Colors::RGB565_GRAY_LIGHT;

/// @brief What the page shows: the slider and the values next to it.
struct WaterHeaterView {
  bool humidifier{false};             ///< A humidifier: humidity instead of temperature
  float current{-999.0f};             ///< Current temperature (or humidity); -999 when unknown
  float target{-999.0f};              ///< Target temperature (or humidity); -999 when not supported
  int32_t temp_step{10};              ///< Slider step, in tenths
  int32_t total_steps{0};             ///< Slider steps from the minimum to the maximum
  int32_t temp_offset{0};             ///< Slider minimum, in tenths, a multiple of the step
  const char *icon{Icons::MDI_NONE};  ///< Icon next to the target
};

/**
 * @brief What the page shows for a water heater or a humidifier.
 *
 * @param entity The water heater or humidifier, from the snapshot.
 * @return The slider settings, the values and the icon.
 */
WaterHeaterView water_heater_view(const SnapshotEntity &entity);

/**
 * @brief A value as shown on the page: one decimal when the step has one, none otherwise.
 *
 * @param value The value; -999 when unknown.
 * @param temp_step Slider step, in tenths.
 * @param suffix Text after the value (separator and unit).
 * @param decimal_separator Decimal separator of the display language.
 * @param unknown Text when the value is unknown.
 * @return The text.
 */
std::string water_heater_value_text(float value, int32_t temp_step, const std::string &suffix, char decimal_separator,
                                    const char *unknown);

/**
 * @brief Draw the mode buttons: the water heater's operation modes, or the humidifier's modes and power button.
 *
 * Also keeps what a button press needs (see water_heater_humidifier_press()).
 *
 * @param entity The water heater or humidifier, from the snapshot.
 */
void water_heater_buttons_render(const SnapshotEntity &entity);

/**
 * @brief Action for a press on a mode button while the page shows a humidifier.
 *
 * @param mode Water heater operation mode the HMI reported for the button.
 * @param action Receives the action: `humidifier.set_mode`, `humidifier.turn_on` or `humidifier.turn_off`.
 * @param value Receives the humidifier mode for `humidifier.set_mode`; empty otherwise.
 * @return false when the button has no humidifier mode.
 */
bool water_heater_humidifier_press(const std::string &mode, std::string &action, std::string &value);

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_WATER_HEATER
