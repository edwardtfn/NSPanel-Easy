// pages.cpp

#include "pages.h"

#ifdef NSPANEL_EASY_PAGE_CLIMATE
#include "page_climate.h"
#endif  // NSPANEL_EASY_PAGE_CLIMATE

namespace esphome::nspanel_easy {

uint8_t current_page_id = 0;
uint8_t home_page_id = get_page_id("home");
uint8_t last_page_id = UINT8_MAX;
uint8_t wakeup_page_id = 1;

bool is_page_offline_capable(uint8_t page_id) {
#ifdef NSPANEL_EASY_PAGE_CLIMATE
  return is_page_offline_capable(page_id, climate_embedded_visible);
#else   // NSPANEL_EASY_PAGE_CLIMATE
  return is_page_offline_capable(page_id, false);
#endif  // NSPANEL_EASY_PAGE_CLIMATE
}

}  // namespace esphome::nspanel_easy
