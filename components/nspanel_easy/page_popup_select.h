// page_popup_select.h

#pragma once

#ifdef NSPANEL_EASY_PAGE_POPUP_SELECT

#include <cstdint>
#include <string>
#include <vector>

namespace esphome::nspanel_easy {

/// Number of option slots on the picker page (opt0..opt7).
constexpr uint8_t POPUP_SELECT_SLOTS = 8;
/// Maximum number of options the picker accepts; bounded by the width of popup_select_cache_mask.
constexpr uint8_t POPUP_SELECT_MAX_OPTIONS = 32;
/// Nextion component ID of opt0; opt1..opt7 follow sequentially.
constexpr uint8_t POPUP_SELECT_ID_OPT0 = 6;
/// Nextion component ID of the previous-page button (btn_prev).
constexpr uint8_t POPUP_SELECT_ID_BTN_PREV = 21;
/// Nextion component ID of the next-page button (btn_next).
constexpr uint8_t POPUP_SELECT_ID_BTN_NEXT = 22;

extern std::string popup_select_cache_entity;
extern std::string popup_select_cache_context;
extern uint8_t popup_select_cache_mode;
extern uint32_t popup_select_cache_mask;
extern std::vector<std::string> popup_select_cache_options;
extern uint8_t popup_select_page;  ///< Zero-based page currently shown by the picker.

/**
 * @brief Number of pages needed to show all cached options.
 * @return Page count; at least 1, even when the cache is empty.
 */
uint8_t popup_select_page_count();

/**
 * @brief Page that holds the currently selected option.
 * @return Zero-based page of the lowest selected bit, or 0 when nothing is selected.
 */
uint8_t popup_select_selected_page();

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_POPUP_SELECT
