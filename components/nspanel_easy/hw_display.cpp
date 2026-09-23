// hw_display.cpp

#ifdef NSPANEL_EASY_HW_DISPLAY

#include "hw_display.h"

namespace esphome::nspanel_easy {

static const char *TAG_COMPONENT_HW_DISPLAY = "nspanel.component.hw_display";

ThemeMode current_theme = ThemeMode::DARK;  ///< Active display theme

uint8_t brightness_current = 100;
bool display_entity_keep = false;
uint8_t display_mode_eeprom = UINT8_MAX;  // Populated from boot report, UINT8_MAX = unknown
bool display_portrait = false;
bool display_valid = false;

DetailsOverlap details_overlap;  ///< Override for the entity details page

ClickInterceptor click_interceptor;

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_HW_DISPLAY
