// page_graph.h

#pragma once

#ifdef NSPANEL_EASY_PAGE_GRAPH

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>
#include "esphome/core/helpers.h"  // For ExternalRAMAllocator and optional
#include "nextion_components.h"    // For HMIComponent
#include "page_home.h"             // For HOME_VALUE_COUNT
#include "pages.h"                 // For page_names and get_page_id

/**
 * @file page_graph.h
 * @brief Nextion component definitions and data handling for the Graph page.
 *
 * The Graph page shows the last 24 hours of a numeric Home Assistant sensor as 5-minute means,
 * opened by tapping one of the Home page entity values. The Blueprint sends the values read from
 * the recorder statistics; the panel keeps them, scales them to the waveform height and draws them.
 */

namespace esphome::nspanel_easy {

static constexpr size_t GRAPH_POINTS = 288;          ///< One point per 5-minute mean over 24 hours (waveform width)
static constexpr uint32_t GRAPH_SLOT_SECONDS = 300;  ///< Length of one statistics period, in seconds
static constexpr uint8_t GRAPH_HEIGHT = 180;     ///< Waveform height in pixels; values are sent as 0..GRAPH_HEIGHT-1
static constexpr uint8_t GRAPH_BATCH_SIZE = 16;  ///< Points sent to the display per batch while drawing

static constexpr uint8_t GRAPH_TICK_HOURS = 6;  ///< Hours between two ticks on the time axis

/// @brief Graph settings sent by the Blueprint when the page opens.
struct GraphConfig {
  std::string title;     ///< Page title, usually the entity's friendly name
  std::string icon;      ///< Icon glyph shown next to the title
  std::string unit;      ///< Unit appended to the min/max labels
  optional<float> min;   ///< Fixed lower bound of the scale; automatic when empty
  optional<float> max;   ///< Fixed upper bound of the scale; automatic when empty
  uint8_t decimals = 1;  ///< Decimals shown on the min/max labels
};

extern std::array<std::string, HOME_VALUE_COUNT> graph_entities;  ///< Entity with a graph per Home page value
extern GraphConfig graph_config;                                  ///< Settings of the graph shown
extern uint32_t graph_last_slot;                                  ///< Start (epoch) of the newest slot, 0 if none

/// @brief Values of the graph shown, oldest first, at most GRAPH_POINTS. NAN marks a period without data.
extern std::vector<float, ExternalRAMAllocator<float>> graph_values;

/**
 * @brief Forget the graph shown, so a page opened for another entity never shows stale data.
 */
void graph_reset();

/**
 * @brief Apply the graph settings sent by the Blueprint.
 *
 * @param txt_list `[title, icon, unit, min, max, decimals]`; empty min/max mean an automatic scale.
 * @return true when the list was valid.
 */
bool graph_set_config(const std::vector<std::string> &txt_list);

/**
 * @brief Replace the graph values with the history sent by the Blueprint.
 *
 * @param txt_list `[start, v0, v1, ...]`: epoch start of the first slot, then one value per slot,
 *                 oldest first, with an empty string for a period without data.
 * @return true when the list was valid.
 */
bool graph_set_history(const std::vector<std::string> &txt_list);

/**
 * @brief Append the newest values sent by the Blueprint, filling missed slots with NAN.
 *
 * Values older than the newest slot already shown are ignored, so a late or repeated
 * answer cannot duplicate points. A gap longer than the graph starts the graph over.
 *
 * @param txt_list `[start, v0, v1, ...]`, same format as graph_set_history().
 * @return Number of slots appended (0 when nothing new or the list was invalid).
 */
size_t graph_append(const std::vector<std::string> &txt_list);

/**
 * @brief Compute the scale of the graph: the configured bounds, or the range of the values.
 *
 * A flat or empty range is widened on its automatic side(s), so the line is not drawn on the bottom edge
 * and a configured bound is always kept.
 *
 * @param[out] low Lower bound of the scale.
 * @param[out] high Upper bound of the scale.
 */
void graph_range(float &low, float &high);

/**
 * @brief Scale a value to the waveform height.
 *
 * @param value Value to scale; values outside the scale are clamped.
 * @param low Lower bound of the scale.
 * @param high Upper bound of the scale.
 * @return Pixel value in 0..GRAPH_HEIGHT-1.
 */
uint8_t graph_scale(float value, float low, float high);

/**
 * @brief Pixel values of every point to draw, oldest first.
 *
 * The waveform cannot skip points: a period without data repeats the previous value,
 * and periods before the first value are drawn at the bottom.
 *
 * @param low Lower bound of the scale.
 * @param high Upper bound of the scale.
 * @return One pixel value per point in graph_values.
 */
std::vector<uint8_t> graph_pixels(float low, float high);

/**
 * @brief Format a value for the min/max labels, with the configured decimals and unit.
 *
 * @param value Value to format.
 * @return Formatted text, e.g. "1250 W".
 */
std::string graph_format(float value);

namespace hmi::graph {

/**
 * @namespace graph
 * @brief Components for the Graph page.
 *
 * Component ID mapping for the Graph page (page id from `get_page_id("graph")`).
 * Based on the Nextion HMI design file; the IDs are the same on every model.
 * Note: All components are local scope, so names don't include page prefix.
 */

// Header components, as on page canvas
constexpr HMIComponent PAGE_LABEL = {"page_label", 1};    ///< Graph title (100 chars max)
constexpr HMIComponent ICON_STATE = {"icon_state", 2};    ///< Graph icon (10 chars max)
constexpr HMIComponent BUTTON_BACK = {"button_back", 3};  ///< Back button (3 chars max)

// Graph components
constexpr HMIComponent WAVE = {"wave", 4};            ///< Waveform, GRAPH_POINTS x GRAPH_HEIGHT px, 1 channel
constexpr HMIComponent VALUE_NOW = {"value_now", 5};  ///< Current value (30 chars max)
constexpr HMIComponent VALUE_MIN = {"value_min", 6};  ///< Lower bound of the scale (30 chars max)
constexpr HMIComponent VALUE_MAX = {"value_max", 7};  ///< Upper bound of the scale (30 chars max)

// Axis drawing: hidden hotspots and variables, drawn by the TFT relative to the waveform
constexpr HMIComponent DRAW_FRAME = {"draw_frame", 8};  ///< Hotspot: clears the axis area and draws the bound lines
constexpr HMIComponent TICK_X = {"tick_x", 9};          ///< Variable: tick offset from the waveform's left edge, in px
constexpr HMIComponent TICK_TEXT = {"tick_text", 10};   ///< Variable: label of the next tick (2 chars max)
constexpr HMIComponent DRAW_TICK = {"draw_tick", 11};   ///< Hotspot: draws a tick at tick_x, labelled tick_text

// All visual components for iteration
constexpr HMIComponent ALL[] = {PAGE_LABEL, ICON_STATE, BUTTON_BACK, WAVE, VALUE_NOW, VALUE_MIN, VALUE_MAX};

constexpr size_t COMPONENT_COUNT = sizeof(ALL) / sizeof(ALL[0]);

}  // namespace hmi::graph
}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_GRAPH
