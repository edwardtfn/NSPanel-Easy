// page_popup_select.cpp

#ifdef NSPANEL_EASY_PAGE_POPUP_SELECT

#include "page_popup_select.h"

namespace esphome::nspanel_easy {

std::string popup_select_cache_entity;
std::string popup_select_cache_context;
uint8_t popup_select_cache_mode = UINT8_MAX;
uint32_t popup_select_cache_mask = 0;
std::vector<std::string> popup_select_cache_options;
uint8_t popup_select_page = 0;

uint8_t popup_select_page_count() {
  const size_t count = popup_select_cache_options.size();
  if (count == 0) {
    return 1;
  }  // if (count == 0)
  return static_cast<uint8_t>((count + POPUP_SELECT_SLOTS - 1) / POPUP_SELECT_SLOTS);
}

uint8_t popup_select_selected_page() {
  if (popup_select_cache_mask == 0) {
    return 0;
  }  // if (popup_select_cache_mask == 0)
  for (uint8_t i = 0; i < POPUP_SELECT_MAX_OPTIONS; ++i) {
    if ((popup_select_cache_mask >> i) & 1u) {
      return i / POPUP_SELECT_SLOTS;
    }  // if (bit i is set)
  }  // for each option bit
  return 0;
}

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_POPUP_SELECT
