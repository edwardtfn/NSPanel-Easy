// page_climate.h

#pragma once

#ifdef NSPANEL_EASY_PAGE_CLIMATE

#include <cstdint>
#include <string>

#include "nextion_components.h"  // For HMIComponent

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
}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_CLIMATE
