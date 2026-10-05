// pages.h

#pragma once

#include <array>
#include <cstdint>
#include <initializer_list>
#include <string>
#include "esphome/core/string_ref.h"  // For StringRef

/**
 * @file pages.h
 * Defines constants and functions related to page names for the NSPanel HA Blueprint project.
 */

namespace esphome::nspanel_easy {

// Constants
/**
 * A compile-time constant array containing the names of pages.
 * These names correspond to various pages of the Nextion TFT file in use,
 * such as settings, home, weather information, and more.
 */
constexpr const char *const page_names[] = {
    "boot",         "home",         "screensaver",  "alarm",        "button",  "buttonpage01", "buttonpage02",
    "buttonpage03", "buttonpage04", "canvas",       "climate",      "confirm", "cover",        "debug",
    "entitypage01", "entitypage02", "entitypage03", "entitypage04", "fan",     "home_smpl",    "keyb_num",
    "light",        "media_player", "notification", "popup_select", "qrcode",  "select",       "settings",
    "switch",       "theme_apply",  "timer",        "utilities",    "vacuum",  "water_heater", "weather01",
    "weather02",    "weather03",    "weather04",    "weather05"};

constexpr size_t PAGE_COUNT = sizeof(page_names) / sizeof(page_names[0]);
static_assert(PAGE_COUNT <= UINT8_MAX, "PAGE_COUNT exceeds uint8_t range");

// Global system flags - initialized to 0 (all flags false)
extern uint8_t current_page_id;
extern uint8_t home_page_id;  ///< Page used as home; every navigation to home uses this, never a literal page
extern uint8_t last_page_id;
extern uint8_t wakeup_page_id;

/**
 * Compares two null-terminated C-strings at compile time.
 * Equivalent to strcmp(a, b) == 0, but constexpr-compatible.
 * std::strcmp becomes constexpr only in C++23; this works with C++17/20.
 *
 * @param a First null-terminated string.
 * @param b Second null-terminated string.
 * @return true if strings are identical, false otherwise.
 */
constexpr bool page_names_equal(const char *a, const char *b) {
  while (*a && *b) {
    if (*a != *b)
      return false;
    ++a;
    ++b;
  }
  return *a == *b;
}

/**
 * Retrieves the index of a given page name within the page_names array.
 * When called with a string literal, this is resolved entirely at compile time.
 *
 * @param page_name The name of the page to find.
 * @return The index of the page_name in the page_names array, or UINT8_MAX if not found.
 */
constexpr uint8_t get_page_id(const char *page_name) {
  if (page_name == nullptr || *page_name == '\0')
    return UINT8_MAX;
  for (uint8_t i = 0; i < PAGE_COUNT; ++i) {
    if (page_names_equal(page_names[i], page_name))
      return i;
  }
  return UINT8_MAX;
}

/**
 * Retrieves the index of a given page name within the page_names array.
 * Runtime overload for StringRef; cannot be constexpr as StringRef is not a literal type.
 *
 * @param page_name The name of the page to find.
 * @return The index of the page_name in the page_names array, or UINT8_MAX if not found.
 */
inline uint8_t get_page_id(const esphome::StringRef &page_name) {
  if (page_name.empty())
    return UINT8_MAX;
  for (uint8_t i = 0; i < PAGE_COUNT; ++i) {
    if (page_name == page_names[i])
      return i;
  }
  return UINT8_MAX;
}

// Compile-time guards for all pages.
// Ensures no entry is accidentally removed or misspelled in page_names[].
static_assert(get_page_id("alarm") != UINT8_MAX, "Missing required page: alarm");
static_assert(get_page_id("boot") != UINT8_MAX, "Missing required page: boot");
static_assert(get_page_id("button") != UINT8_MAX, "Missing required page: button");
static_assert(get_page_id("buttonpage01") != UINT8_MAX, "Missing required page: buttonpage01");
static_assert(get_page_id("buttonpage02") != UINT8_MAX, "Missing required page: buttonpage02");
static_assert(get_page_id("buttonpage03") != UINT8_MAX, "Missing required page: buttonpage03");
static_assert(get_page_id("buttonpage04") != UINT8_MAX, "Missing required page: buttonpage04");
static_assert(get_page_id("canvas") != UINT8_MAX, "Missing required page: canvas");
static_assert(get_page_id("climate") != UINT8_MAX, "Missing required page: climate");
static_assert(get_page_id("confirm") != UINT8_MAX, "Missing required page: confirm");
static_assert(get_page_id("cover") != UINT8_MAX, "Missing required page: cover");
static_assert(get_page_id("debug") != UINT8_MAX, "Missing required page: debug");
static_assert(get_page_id("entitypage01") != UINT8_MAX, "Missing required page: entitypage01");
static_assert(get_page_id("entitypage02") != UINT8_MAX, "Missing required page: entitypage02");
static_assert(get_page_id("entitypage03") != UINT8_MAX, "Missing required page: entitypage03");
static_assert(get_page_id("entitypage04") != UINT8_MAX, "Missing required page: entitypage04");
static_assert(get_page_id("fan") != UINT8_MAX, "Missing required page: fan");
static_assert(get_page_id("home") != UINT8_MAX, "Missing required page: home");
static_assert(get_page_id("home_smpl") != UINT8_MAX, "Missing required page: home_smpl");
static_assert(get_page_id("keyb_num") != UINT8_MAX, "Missing required page: keyb_num");
static_assert(get_page_id("light") != UINT8_MAX, "Missing required page: light");
static_assert(get_page_id("media_player") != UINT8_MAX, "Missing required page: media_player");
static_assert(get_page_id("notification") != UINT8_MAX, "Missing required page: notification");
static_assert(get_page_id("popup_select") != UINT8_MAX, "Missing required page: popup_select");
static_assert(get_page_id("qrcode") != UINT8_MAX, "Missing required page: qrcode");
static_assert(get_page_id("screensaver") != UINT8_MAX, "Missing required page: screensaver");
static_assert(get_page_id("settings") != UINT8_MAX, "Missing required page: settings");
static_assert(get_page_id("select") != UINT8_MAX, "Missing required page: select");
static_assert(get_page_id("switch") != UINT8_MAX, "Missing required page: switch");
static_assert(get_page_id("theme_apply") != UINT8_MAX, "Missing required page: theme_apply");
static_assert(get_page_id("timer") != UINT8_MAX, "Missing required page: timer");
static_assert(get_page_id("utilities") != UINT8_MAX, "Missing required page: utilities");
static_assert(get_page_id("water_heater") != UINT8_MAX, "Missing required page: water_heater");
static_assert(get_page_id("weather01") != UINT8_MAX, "Missing required page: weather01");
static_assert(get_page_id("weather02") != UINT8_MAX, "Missing required page: weather02");
static_assert(get_page_id("weather03") != UINT8_MAX, "Missing required page: weather03");
static_assert(get_page_id("weather04") != UINT8_MAX, "Missing required page: weather04");
static_assert(get_page_id("weather05") != UINT8_MAX, "Missing required page: weather05");

/**
 * @brief Whether a page works without Home Assistant.
 *
 * Mirrors the TFT: every page not listed here leaves through its Preinitialize
 * event while `api==0`. The climate page is the exception, as it only works
 * offline when it shows the embedded thermostat.
 *
 * @param page_id Page to check.
 * @param climate_embedded Whether the climate page would show the embedded thermostat.
 * @return true when the page can be shown while Home Assistant is not connected.
 */
constexpr bool is_page_offline_capable(uint8_t page_id, bool climate_embedded) {
  switch (page_id) {
    case get_page_id("climate"):
      return climate_embedded;
    case get_page_id("boot"):
    case get_page_id("confirm"):
    case get_page_id("debug"):
    case get_page_id("home"):
    case get_page_id("home_smpl"):
    case get_page_id("qrcode"):
    case get_page_id("screensaver"):
    case get_page_id("settings"):
    case get_page_id("theme_apply"):
      return true;
    default:
      return false;
  }  // switch (page_id)
}

/**
 * @brief Whether a page works without Home Assistant, as the display is now.
 *
 * Uses the current embedded thermostat state of the climate page.
 *
 * @param page_id Page to check.
 * @return true when the page can be shown while Home Assistant is not connected.
 */
bool is_page_offline_capable(uint8_t page_id);

/**
 * @brief Resolve the page the display should wake up to.
 *
 * Falls back to home_page_id when Home Assistant is not connected and the
 * configured wake-up page would only bounce back.
 *
 * @param api_connected Whether the API is connected with a state subscription.
 * @param climate_embedded Whether the climate page would show the embedded thermostat.
 * @return Page id to wake up to.
 */
inline uint8_t wakeup_page_resolve(bool api_connected, bool climate_embedded) {
  if (api_connected || is_page_offline_capable(wakeup_page_id, climate_embedded)) {
    return wakeup_page_id;
  }
  return home_page_id;
}

}  // namespace esphome::nspanel_easy
