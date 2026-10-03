// hardware.h

#pragma once

#include <cstdint>

namespace esphome::nspanel_easy {

/**
 * @brief Combined hardware settings using bitfields
 *
 * Packs BOTH button and relay settings into a single byte (uint8_t).
 * Bits 0-3: Button settings (left enabled, left state, right enabled, right state)
 * Bits 4-7: Relay settings (relay1 local, relay1 fallback, relay2 local, relay2 fallback)
 *
 * Memory efficient: Saves 1 byte compared to separate button/relay bytes.
 * Previous: 2 bytes (1 for buttons + 1 for relays)
 * Now: 1 byte (contains both)
 */
struct HardwareSettings {
  // Button settings (bits 0-3)
  uint8_t button_left_enabled : 1;   ///< Bit 0: Left button visualization enabled
  uint8_t button_left_state : 1;     ///< Bit 1: Left button state (0=off, 1=on)
  uint8_t button_right_enabled : 1;  ///< Bit 2: Right button visualization enabled
  uint8_t button_right_state : 1;    ///< Bit 3: Right button state (0=off, 1=on)

  // Relay settings (bits 4-7)
  uint8_t relay1_local : 1;     ///< Bit 4: Relay 1 local control enabled
  uint8_t relay1_fallback : 1;  ///< Bit 5: Relay 1 fallback mode enabled
  uint8_t relay2_local : 1;     ///< Bit 6: Relay 2 local control enabled
  uint8_t relay2_fallback : 1;  ///< Bit 7: Relay 2 fallback mode enabled

  // Default constructor - all flags start as false (zero-initialized)
  HardwareSettings()
      : button_left_enabled(0),
        button_left_state(0),
        button_right_enabled(0),
        button_right_state(0),
        relay1_local(0),
        relay1_fallback(0),
        relay2_local(0),
        relay2_fallback(0) {}
};

static_assert(sizeof(HardwareSettings) == 1, "HardwareSettings must be exactly 1 byte");

// Note: hardware_settings_raw is declared as uint8_t in ESPHome YAML with restore_value: true
// Use the helper function below to access it as a HardwareSettings struct

/**
 * @brief Convert raw uint8_t to HardwareSettings struct
 * @param raw The raw byte value containing packed settings
 * @return HardwareSettings struct with all bitfields populated
 */
inline HardwareSettings from_raw(uint8_t raw) {
  HardwareSettings s;
  s.button_left_enabled = (raw >> 0) & 1;
  s.button_left_state = (raw >> 1) & 1;
  s.button_right_enabled = (raw >> 2) & 1;
  s.button_right_state = (raw >> 3) & 1;
  s.relay1_local = (raw >> 4) & 1;
  s.relay1_fallback = (raw >> 5) & 1;
  s.relay2_local = (raw >> 6) & 1;
  s.relay2_fallback = (raw >> 7) & 1;
  return s;
}

/**
 * @brief Convert HardwareSettings struct to raw uint8_t
 * @param s The HardwareSettings struct to pack
 * @return Raw byte value with all settings packed as bitfields
 */
inline uint8_t to_raw(const HardwareSettings &s) {
  return (s.button_left_enabled << 0) | (s.button_left_state << 1) | (s.button_right_enabled << 2) |
         (s.button_right_state << 3) | (s.relay1_local << 4) | (s.relay1_fallback << 5) | (s.relay2_local << 6) |
         (s.relay2_fallback << 7);
}

/**
 * @brief Get hardware settings from the raw uint8_t
 * @param raw_value The uint8_t global variable from YAML
 * @return HardwareSettings struct (copy, not reference)
 */
inline HardwareSettings get_hardware_settings(uint8_t raw_value) { return from_raw(raw_value); }

/**
 * @brief Update raw value from hardware settings
 * @param raw_value Reference to the uint8_t global variable from YAML
 * @param settings The HardwareSettings to write
 */
inline void set_hardware_settings(uint8_t &raw_value, const HardwareSettings &settings) {
  raw_value = to_raw(settings);
}

/**
 * @brief Screen area (bounding box) of a hardware button bar
 *
 * The bar is drawn with rounded ends: the full box minus one pixel at each end of its length,
 * plus a one-pixel line along its middle covering the full length.
 */
struct HwButtonBarArea {
  uint16_t x;  ///< Left edge
  uint16_t y;  ///< Top edge
  uint16_t w;  ///< Width
  uint16_t h;  ///< Height
  /**
   * @brief Whether the bar is vertical (US Landscape model, buttons on the side)
   * @return true when the bar is taller than wide (h > w), false otherwise
   */
  constexpr bool is_vertical() const { return h > w; }
};

/**
 * @brief Hardware button bar areas, indexed by [display model][button]
 *
 * Display model: 0 = EU, 1 = US Portrait, 2 = US Landscape (display_mode - 1).
 * Button: 0 = left, 1 = right.
 */
constexpr HwButtonBarArea HW_BUTTON_BAR_AREAS[3][2] = {
    {{47, 312, 120, 3}, {288, 312, 120, 3}},  // EU
    {{16, 471, 120, 3}, {183, 471, 120, 3}},  // US Portrait
    {{467, 173, 3, 120}, {467, 27, 3, 120}},  // US Landscape
};

}  // namespace esphome::nspanel_easy
