// page_popup_select.cpp

#ifdef NSPANEL_EASY_PAGE_POPUP_SELECT

#include "page_popup_select.h"

#include <algorithm>

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

std::vector<std::string> popup_select_visible_options(const std::vector<std::string> &options,
                                                      const std::string &current) {
  const size_t limit = POPUP_SELECT_MAX_OPTIONS;
  if (options.size() <= limit) {
    return options;
  }  // if (all options fit)
  const auto shown_end = options.begin() + limit;
  if (current.empty() || std::find(options.begin(), shown_end, current) != shown_end) {
    return std::vector<std::string>(options.begin(), shown_end);
  }  // if (no current option, or already among the first ones)
  // The current option first, then the first options but one (without the current option)
  std::vector<std::string> shown{current};
  for (auto it = options.begin(); it != options.begin() + (limit - 1); ++it) {
    if (*it != current) {
      shown.push_back(*it);
    }  // if (not the current option)
  }  // for each of the first options but one
  return shown;
}

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_POPUP_SELECT
