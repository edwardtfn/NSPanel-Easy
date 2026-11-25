// page_screensaver.h

#pragma once

#ifdef NSPANEL_EASY_PAGE_SCREENSAVER

#include "nextion_components.h"  // For HMIComponent
#include "pages.h"               // For page_names and get_page_id

/**
 * @file page_screensaver.h
 * @brief Nextion component definitions for the Screensaver page.
 *
 * This file contains all component constants specific to the Screensaver page
 * of the NSPanel interface, along with the persistent visibility flag used to
 * avoid redundant display updates.
 */

namespace esphome::nspanel_easy {

namespace hmi::screensaver {

/**
 * @namespace screensaver
 * @brief Components for the Screensaver page.
 *
 * Component ID mapping for the Screensaver page.
 * Based on the Nextion HMI design file.
 * Note: All components are local scope, so names do not include a page prefix.
 */

// Display components
constexpr HMIComponent TEXT = {"text", 4};  ///< Time/clock label (word-wrap enabled, 1000 chars max)
// Stacked time (font 12 on the US model): one component per line, each centered on its own.
// Present in both TFTs with the same IDs; hidden on page load and shown by ESPHome when used.
constexpr HMIComponent TIME_HOUR = {"time_hour", 18};          ///< Stacked time: hour (screensaver font)
constexpr HMIComponent TIME_MINUTE = {"time_minute", 19};      ///< Stacked time: minute (screensaver font)
constexpr HMIComponent TIME_MERIDIEM = {"time_meridiem", 20};  ///< Stacked time: meridiem (smaller fixed font)

// Touch capture components
constexpr HMIComponent WAKEUP = {"wakeup", 1};  ///< Full-screen wake-up touch area
constexpr HMIComponent SWIPE = {"swipe", 2};    ///< Swipe gesture capture area

// Timers (for reference — not visual, excluded from ALL[])
constexpr HMIComponent TIMER_SWIPESTORE = {"swipestore", 3};  ///< Swipe coordinate sampling timer (50 ms)

// All visual components for iteration (timers and touch caps excluded)
constexpr HMIComponent ALL[] = {TEXT, TIME_HOUR, TIME_MINUTE, TIME_MERIDIEM};

constexpr size_t COMPONENT_COUNT = sizeof(ALL) / sizeof(ALL[0]);

}  // namespace hmi::screensaver
}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_SCREENSAVER
