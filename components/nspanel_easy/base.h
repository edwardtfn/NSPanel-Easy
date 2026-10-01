// base.h - Generic flag system for NSPanel HA Blueprint
#pragma once

#include "esphome/components/api/custom_api_device.h"  // For API HA event call
#include "esphome/core/application.h"                  // For App
#include "esphome/core/hal.h"                          // For delay()
#include "esphome/core/log.h"
#include <cstdint>
#include <map>
#include <string>

namespace esphome::nspanel_easy {

/**
 * @brief System-wide flags for NSPanel HA Blueprint component state tracking
 *
 * Uses bitfields to pack 16 boolean states into 2 bytes (uint16_t).
 * Each flag represents a specific component state throughout the system lifecycle.
 * Memory efficient: 16 individual bools would use 16 bytes; this uses only 2 bytes.
 */
struct SystemFlags {
  // Boot/initialization flags
  uint16_t baud_rate_set : 1;              ///< UART baud rate configured
  uint16_t nextion_ready : 1;              ///< Nextion display communication ready
  uint16_t blueprint_ready : 1;            ///< Blueprint component initialized
  uint16_t tft_ready : 1;                  ///< TFT display ready
  uint16_t boot_completed : 1;             ///< Boot script completed
  uint16_t version_check_ok : 1;           ///< All component versions verified
  uint16_t display_settings_received : 1;  ///< All display settings received

  // Runtime operation flags
  uint16_t tft_upload_active : 1;       ///< TFT firmware upload in progress
  uint16_t ota_in_progress : 1;         ///< Over-the-air update active
  uint16_t display_sleep : 1;           ///< Display is in sleep mode
  uint16_t display_resync_pending : 1;  ///< Display restarted; repaint on next page entry

  // Baud rate flags
  uint16_t baud_fallback_active : 1;  ///< UART is operating at non-primary rate (detected via scan)
  uint16_t baud_scan_success : 1;     ///< Last baud rate scan found a responsive rate

  // Reserved flags
  uint16_t reserved : 3;

  // Default constructor - all flags start as false (zero-initialized)
  SystemFlags()
      : baud_rate_set(0),
        nextion_ready(0),
        blueprint_ready(0),
        tft_ready(0),
        boot_completed(0),
        version_check_ok(0),
        display_settings_received(0),
        tft_upload_active(0),
        ota_in_progress(0),
        display_sleep(0),
        baud_fallback_active(0),
        baud_scan_success(0),
        reserved(0) {}
};

/**
 * @brief Blueprint status flags for settings step tracking
 *
 * Uses bitfields to pack 8 boolean states into 1 byte (uint8_t).
 * These flags track specific blueprint initialization steps.
 * Bits 0-2 are active flags used in percentage calculation.
 * Bits 3-7 are reserved and not used in calculations.
 */
struct BlueprintStatusFlags {
  uint8_t page_home : 1;            ///< Home page initialization completed
  uint8_t version : 1;              ///< Blueprint version received
  uint8_t hw_buttons_settings : 1;  ///< Hardware buttons settings completed
  uint8_t reserved : 5;             ///< Reserved (not used in percentage calculation)

  // Default constructor - all flags start as false (zero-initialized)
  BlueprintStatusFlags() : page_home(0), version(0), hw_buttons_settings(0), reserved(0) {}

  /**
   * `@brief` Check if all active flags are set
   * `@return` true if all active flags are set, false otherwise
   */
  bool all_active_flags_set() const {
    // All active flags must be set
    return page_home && version && hw_buttons_settings;
  }

  /**
   * `@brief` Count active flags set
   * `@return` Number of flags set
   */
  uint8_t count_active_flags_set() const { return page_home + version + hw_buttons_settings; }

  /**
   * `@brief` Calculate percentage of active flags that are set
   * `@return` Percentage (0.0-100.0) of active flags set
   */
  float get_completion_percentage() const {
    static constexpr uint8_t TOTAL_ACTIVE_FLAGS = 3;
    return (static_cast<float>(count_active_flags_set()) / TOTAL_ACTIVE_FLAGS) * 100.0f;
  }

  /**
   * @brief Reset all blueprint status flags to their initial state (false)
   *
   * Clears all active flags and reserved bits, returning the
   * struct to its default-constructed state.
   */
  void reset() {
    page_home = false;
    version = false;
    hw_buttons_settings = false;
    reserved = 0;
  }
};

// Global system flags - initialized to all flags false
extern SystemFlags system_flags;

// Global blueprint status flags - initialized to all flags false
extern BlueprintStatusFlags blueprint_status_flags;

/**
 * @brief Check if device is ready to accept new tasks
 * @return true if device is ready (no blocking operations active), false otherwise
 */
inline bool is_device_ready_for_tasks() {
  // Boot flag must be set to consider the system ready
  // Device is NOT ready if any of these blocking operations are active
  return system_flags.boot_completed && !system_flags.ota_in_progress && !system_flags.tft_upload_active;
}

/**
 * @brief Check if all non-reserved blueprint status flags are set
 * @return true if all active flags are set, false otherwise
 */
inline bool is_blueprint_fully_ready() {
  bool fully_ready = blueprint_status_flags.all_active_flags_set();

  // Update system flag only if it changed
  if (fully_ready != system_flags.blueprint_ready) {
    system_flags.blueprint_ready = fully_ready;
  }

  return fully_ready;
}

/// @brief Feed the watchdog timer with an optional delay
///
/// This utility function combines a delay with a watchdog timer feed operation.
/// It's commonly used in long-running operations to prevent watchdog timeouts
/// while giving the system time to process other tasks.
///
/// @param ms Delay duration in milliseconds before feeding the watchdog (default: 5ms)
///
/// @note This function is blocking - execution will pause for the specified duration
/// @note Minimum recommended delay is 5ms to allow system task processing
///
/// @code
/// // Example usage with default 5ms delay
/// feed_wdt_delay();
///
/// // Example usage with custom 25ms delay
/// feed_wdt_delay(25);
///
/// // Common pattern in loops
/// for (uint8_t i = 0; i < 10; ++i) {
///   // ... do work ...
///   feed_wdt_delay(5);  // Prevent watchdog timeout
/// }
/// @endcode
///
/// @see delay()
/// @see App.feed_wdt()
inline void feed_wdt_delay(uint32_t ms = 5) {
  esphome::delay(ms);       // Block execution for specified milliseconds
  esphome::App.feed_wdt();  // Reset the watchdog timer
}

// Cached device name to avoid repeated lookups and string copies
extern std::string cached_device_name;

/**
 * @brief API version (MAJOR.MINOR) provided by this firmware.
 *
 * Set from the `api_version` substitution during boot (see nspanel_esphome_version.yaml).
 * While set, fire_ha_event() adds it to every event, so the Blueprint can check it before calling any action.
 */
extern const char *firmware_api_version;

/**
 * @brief How a button bound to an unavailable entity is rendered.
 *
 * Applies to every per-entity button surface: the button pages, the climate
 * page custom buttons and the home page custom buttons. Stored as a single
 * byte so it can be pushed from the blueprint as a plain integer.
 *
 * Only genuinely unavailable entities are affected. An entity reporting
 * "unknown" is left alone: a `button` or `script` entity that has never run
 * reports "unknown" for legitimate reasons and is still actionable.
 */
enum class UnavailableBehavior : uint8_t {
  RENDER_AS_OFF = 0,  ///< Legacy behaviour: indistinguishable from an off entity
  INDICATE = 1,       ///< Forced off, icon and label greyed, click ignored
  HIDE = 2,           ///< Components hidden, click unreachable
};

/// @brief Active unavailable-entity behaviour, pushed by the blueprint.
extern UnavailableBehavior unavailable_behavior;

/**
 * @brief Set when the behaviour changes away from HIDE, cleared on page change.
 *
 * Button page renders only write visibility under HIDE. A button hidden on the
 * page currently shown would otherwise stay hidden after switching to another
 * behaviour, since the refresh that follows is not a page entry and nothing
 * restores visibility. While set, refreshes restore it as well. A page change
 * resets visibility to the TFT defaults, so the flag is no longer needed after it.
 */
extern bool unavailable_unhide_pending;

/**
 * @brief Fire a Home Assistant event for NSPanel HA Blueprint
 *
 * Automatically adds device_name, type and, when available, api_version to the event data.
 *
 * @param type Event type (e.g., "button_click", "page_changed", "boot")
 * @param data Additional event data (device_name, type and api_version added automatically)
 *
 * @note The event name is automatically set to "esphome.nspanel_easy"
 * @note Call init_device_name_cache() during boot before using this function
 *
 * @code
 * fire_ha_event("button_click", {
 *   {"page", "home"},
 *   {"component", "bt_left"}
 * });
 * @endcode
 */
void fire_ha_event(const std::string &type, std::map<std::string, std::string> data = {});

}  // namespace esphome::nspanel_easy
