// hw_display.cpp

#ifdef NSPANEL_EASY_HW_DISPLAY

#include "hw_display.h"

#include <cstddef>
#include <cstring>

#ifdef NSPANEL_EASY_SUBSCRIBE
#include "esphome/components/api/api_server.h"
#endif  // NSPANEL_EASY_SUBSCRIBE

namespace esphome::nspanel_easy {

static const char *TAG_COMPONENT_HW_DISPLAY = "nspanel.component.hw_display";

ThemeMode current_theme = ThemeMode::DARK;  ///< Active display theme

uint8_t brightness_current = 100;
bool brightness_dim_hold = false;
bool display_entity_keep = false;
uint8_t display_mode_eeprom = UINT8_MAX;  // Populated from boot report, UINT8_MAX = unknown
bool display_portrait = false;
bool display_valid = false;

DetailsOverlap details_overlap;  ///< Override for the entity details page

ClickInterceptor click_interceptor;

#ifdef NSPANEL_EASY_SUBSCRIBE
std::function<void(bool active)> wakeup_sensors_changed;

/// @brief Active state per binding index, as a bit mask (bindings targeting other pages stay clear).
static uint8_t wakeup_sensors_bits[(SUB_MAX + 7) / 8] = {};

bool wakeup_sensors_active() {
  for (const uint8_t bits : wakeup_sensors_bits) {
    if (bits != 0) {
      return true;
    }
  }  // for (bits)
  return false;
}

void wakeup_sensors_connection_update(bool connected) {
  static bool was_connected = false;
  if (connected == was_connected) {
    return;
  }
  was_connected = connected;

  if (connected) {
    return;  // Home Assistant sends every subscribed state again when it connects
  }  // if (connected)

  // The cached states are forgotten, so the states sent again on reconnection are applied even
  // when unchanged (they would be skipped as duplicates otherwise). Re-applying a cached state
  // instead could restore a sensor that changed while disconnected.
  for (uint16_t idx = 0; idx < sub_count; idx++) {
    if (sub_renderers[idx] == &wakeup_sensor_render) {
      sub_runtime[idx].state[0] = '\0';
      sub_runtime[idx].action[0] = '\0';  // hvac_action, used by climate bindings only
    }  // if (wakeup binding)
  }  // for (idx)

  if (!wakeup_sensors_active()) {
    return;
  }
  ESP_LOGD(TAG_COMPONENT_HW_DISPLAY, "Wake-up sensors released");
  memset(wakeup_sensors_bits, 0, sizeof(wakeup_sensors_bits));
  if (wakeup_sensors_changed) {
    wakeup_sensors_changed(false);
  }
}

void wakeup_sensor_render(const SubBinding &binding, const SubRuntime &rt, const char * /*state*/, bool /*visible*/) {
  const ptrdiff_t idx = &binding - sub_bindings;
  if (idx < 0 || idx >= static_cast<ptrdiff_t>(SUB_MAX)) {
    ESP_LOGE(TAG_COMPONENT_HW_DISPLAY, "Wake-up sensor binding out of range: %s", binding.entity);
    return;
  }

  const uint8_t mask = static_cast<uint8_t>(1U << (idx % 8));
  uint8_t &bits = wakeup_sensors_bits[idx / 8];
  // The last known state is stale while Home Assistant is not connected, and bindings are
  // re-applied on page changes, so a sensor is only active while connected
  const bool active = (rt.last_state == SUB_STATE_ON) && api::global_api_server->is_connected_with_state_subscription();
  const bool was_active = (bits & mask) != 0;
  if (active == was_active) {
    return;  // Re-applied without a change (e.g. on a page change)
  }

  if (active) {
    bits |= mask;
  } else {
    bits &= static_cast<uint8_t>(~mask);
  }  // if (active)
  ESP_LOGV(TAG_COMPONENT_HW_DISPLAY, "Wake-up sensor %s: %s", binding.entity, active ? "active" : "idle");

  if (!wakeup_sensors_changed) {
    return;
  }
  if (active) {
    wakeup_sensors_changed(true);
  } else if (!wakeup_sensors_active()) {
    wakeup_sensors_changed(false);
  }  // if (active)
}
#endif  // NSPANEL_EASY_SUBSCRIBE

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_HW_DISPLAY
