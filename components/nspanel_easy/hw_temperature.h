// hw_temperature.h

#pragma once

#if defined(NSPANEL_EASY_HW_TEMPERATURE) && defined(NSPANEL_EASY_SUBSCRIBE)

#include <cstdint>

#include "esphome/core/optional.h"

#include "api_subscriptions.h"  // For SubBinding and SubRuntime

/**
 * @file hw_temperature.h
 * @brief Auto-calibration of the panel's temperature sensor against a Home Assistant reference.
 *
 * The panel keeps regulating on its own sensor at all times. The reference only
 * feeds the Temperature Correction, so the calibration in place when Home
 * Assistant becomes unavailable is kept for the whole outage.
 *
 * Calibration pauses, keeping the current correction, while the reference has no
 * usable value, Home Assistant is stale (see api_stale), the reference has not
 * changed for longer than the configured maximum age, or the offset it implies is
 * out of range (e.g. a reference reporting in a different unit).
 */

namespace esphome::nspanel_easy {

static constexpr float TEMP_CALIBRATION_MAX_OFFSET_C = 10.0f;  ///< Largest plausible offset in °C, the correction range

/**
 * @brief Track a subscription binding holding the calibration reference.
 *
 * Registered for the "calibration" page in sub_resolve_renderer(). Nothing is
 * drawn: the value is kept for temp_calibration_sample(). Bindings are re-applied
 * on every page change, so only a changed value refreshes the reference's age.
 *
 * A climate reference is bound twice: its current_temperature attribute to the
 * "temperature" component, and its state to "availability". Home Assistant sends
 * nothing when an attribute disappears, as it does when the entity becomes
 * unavailable, so only the state reveals that the last value is no longer valid.
 *
 * The value is read in the unit set by `temp_units`; a mismatch is caught by the
 * plausibility check in temp_calibration_sample().
 *
 * @param binding Binding being applied.
 * @param rt Unused: the state is passed separately.
 * @param state Raw state string, or the current_temperature attribute for climate.
 * @param visible Unused: nothing is drawn.
 */
void temp_reference_render(const SubBinding &binding, const SubRuntime &rt, const char *state, bool visible);

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
