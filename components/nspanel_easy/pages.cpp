// pages.cpp

#include "pages.h"

#ifdef NSPANEL_EASY_PAGE_CLIMATE
#include "page_climate.h"
#endif  // NSPANEL_EASY_PAGE_CLIMATE

namespace esphome::nspanel_easy {

uint8_t current_page_id = 0;
#ifndef NSPANEL_EASY_HOME_PAGE_SIMPLE
#define NSPANEL_EASY_HOME_PAGE_SIMPLE 0
#endif  // NSPANEL_EASY_HOME_PAGE_SIMPLE

// The simple home page (#339) replaces home when selected with `home_page_simple`
uint8_t home_page_id = get_page_id(NSPANEL_EASY_HOME_PAGE_SIMPLE ? "home_simple" : "home");
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
