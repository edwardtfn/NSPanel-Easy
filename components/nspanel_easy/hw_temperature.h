// hw_temperature.h

#pragma once

#if defined(NSPANEL_EASY_HW_TEMPERATURE) && defined(NSPANEL_EASY_SUBSCRIBE)

#include <cstdint>
#include <functional>

#include "esphome/core/optional.h"

#include "api_subscriptions.h"  // For SubBinding and SubRuntime

/**
 * @file hw_temperature.h
 * @brief Temperature reference from Home Assistant: auto-calibration of the panel's sensor,
 * and optionally the direct source of the embedded thermostat.
 *
 * By default the panel keeps regulating on its own sensor at all times, and the
 * reference only feeds the Temperature Correction, so the calibration in place when
 * Home Assistant becomes unavailable is kept for the whole outage.
 *
 * When the binding targets the "thermostat" component, the embedded thermostat
 * (Climate add-on) also regulates on the reference directly while it can be trusted,
 * falling back to the calibrated sensor otherwise.
 *
 * Calibration pauses, keeping the current correction, while the reference has no
 * usable value, Home Assistant is stale (see api_stale), the reference has not
 * changed for longer than the configured maximum age, or the offset it implies is
 * out of range (e.g. a reference reporting in a different unit).
 */

namespace esphome::nspanel_easy {

static constexpr float TEMP_CALIBRATION_MAX_OFFSET_C = 10.0f;  ///< Largest plausible offset in °C, the correction range

/**
 * @brief Invoked when the reference changes value or usability while it drives the thermostat.
 *
 * Assigned from addon_climate_base.yaml, where the thermostat's temperature sensor is visible.
 */
extern std::function<void()> temp_reference_changed;

/**
 * @brief Track the subscription binding holding the temperature reference.
 *
 * Registered for the "calibration" page in sub_resolve_renderer(). Nothing is
 * drawn. The component selects the mode: "temperature" calibrates only, while
 * "thermostat" also makes the reference the thermostat's direct source. Bindings
 * are re-applied on every page change and the same value is re-sent on
 * reconnection, so only a changed value refreshes the reference's age.
 *
 * A climate reference is bound twice: its current_temperature attribute to the
 * "temperature" (or "thermostat") component, and its state to "availability". Home Assistant sends
 * nothing when an attribute disappears, as it does when the entity becomes
 * unavailable, so only the state reveals that the last value is no longer valid.
 *
 * The value is read in the unit set by `temp_units`; a mismatch is caught by the
 * plausibility checks of its consumers.
 *
 * @param binding Binding being applied.
 * @param rt Runtime state; its raw state tells whether a climate reference is available.
 * @param state Raw state string, or the current_temperature attribute for climate.
 * @param visible Unused: nothing is drawn.
 */
void temp_reference_render(const SubBinding &binding, const SubRuntime &rt, const char *state, bool visible);

/**
 * @brief Whether the binding asks for the reference to drive the embedded thermostat.
 *
 * @return true when the reference is bound to the "thermostat" component.
 */
bool temp_reference_drives_thermostat();

/**
 * @brief Get the reference when it is usable and recent.
 *
 * @param max_age_ms Longest time without a change of the reference before it is ignored.
 * @return The reference in °C, or nothing when it is unusable, unavailable, stale or too old.
 */
optional<float> temp_reference_get(uint32_t max_age_ms);

/**
 * @brief Check whether the reference has just exceeded its maximum age.
 *
 * Called periodically, since a reference that stops changing triggers no other event.
 *
 * @param max_age_ms Longest time without a change of the reference before it is ignored.
 * @return true once, when the reference has just become too old.
 */
bool temp_reference_aged_out(uint32_t max_age_ms);

/**
 * @brief Take one calibration sample.
 *
 * Moves the correction a fraction @p alpha of the way towards the offset between
 * the reference and the panel's uncorrected reading, which smooths out short
 * variations of the panel's self-heating.
 *
 * @param raw_c Panel's sensor reading before the correction, in °C.
 * @param correction_c Correction currently applied, in °C. Seeds the smoothing on the first sample.
 * @param max_age_ms Longest time without a change of the reference before calibration pauses.
 * @param alpha Smoothing factor per sample, between 0 and 1.
 * @return New correction in °C, or nothing when calibration is paused.
 */
optional<float> temp_calibration_sample(float raw_c, float correction_c, uint32_t max_age_ms, float alpha);

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_HW_TEMPERATURE && NSPANEL_EASY_SUBSCRIBE
