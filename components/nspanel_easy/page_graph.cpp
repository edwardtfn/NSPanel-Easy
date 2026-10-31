// page_graph.cpp

#ifdef NSPANEL_EASY_PAGE_GRAPH

#include "page_graph.h"
#include <algorithm>  // For std::min
#include <cinttypes>
#include <cmath>   // For NAN, std::isfinite, std::fabs, std::lround
#include <cstdio>  // For snprintf
#include "esphome/core/log.h"
#include "text.h"  // For adjustDecimalSeparator

namespace esphome::nspanel_easy {

static const char *const TAG = "nspanel.page.graph";

std::array<std::string, HOME_VALUE_COUNT> graph_entities{};
GraphConfig graph_config{};
uint32_t graph_last_slot = 0;
std::vector<float, ExternalRAMAllocator<float>> graph_values;

/**
 * @brief Parse a graph value: an empty or non-numeric string is a period without data.
 *
 * @param txt Value as sent by the Blueprint.
 * @return The value, or NAN.
 */
static float graph_parse_value(const std::string &txt) {
  const optional<float> value = parse_number<float>(txt);
  return (value.has_value() && std::isfinite(*value)) ? *value : NAN;
}

/**
 * @brief Parse the epoch start of the first slot of a history or append list.
 *
 * @param txt_list List sent by the Blueprint, starting with the epoch start.
 * @param[out] start Epoch start of the first slot, aligned to GRAPH_SLOT_SECONDS.
 * @return true when the start is valid.
 */
static bool graph_parse_start(const std::vector<std::string> &txt_list, uint32_t &start) {
  if (txt_list.empty())
    return false;
  const optional<uint32_t> value = parse_number<uint32_t>(txt_list.front());
  if (!value.has_value() || *value == 0) {
    ESP_LOGE(TAG, "Invalid graph start: '%s'", txt_list.front().c_str());
    return false;
  }  // if (invalid start)
  start = *value - (*value % GRAPH_SLOT_SECONDS);
  return true;
}

void graph_reset() {
  graph_config = GraphConfig{};
  graph_values.clear();
  graph_last_slot = 0;
}

bool graph_set_config(const std::vector<std::string> &txt_list) {
  if (txt_list.size() != 6) {
    ESP_LOGE(TAG, "Expected 6 graph settings, got %zu", txt_list.size());
    return false;
  }  // if (wrong size)
  graph_config.title = txt_list[0];
  graph_config.icon = txt_list[1];
  graph_config.unit = txt_list[2];
  graph_config.min = parse_number<float>(txt_list[3]);
  graph_config.max = parse_number<float>(txt_list[4]);
  const optional<uint8_t> decimals = parse_number<uint8_t>(txt_list[5]);
  graph_config.decimals = decimals.has_value() ? std::min<uint8_t>(*decimals, 3) : 1;
  return true;
}

bool graph_set_history(const std::vector<std::string> &txt_list) {
  uint32_t start = 0;
  if (!graph_parse_start(txt_list, start))
    return false;
  graph_values.clear();
  // Keep only the newest GRAPH_POINTS values, in case the Blueprint sent more
  const size_t count = txt_list.size() - 1;
  const size_t skip = (count > GRAPH_POINTS) ? count - GRAPH_POINTS : 0;
  graph_values.reserve(count - skip);
  for (size_t idx = 1 + skip; idx < txt_list.size(); ++idx) {
    graph_values.push_back(graph_parse_value(txt_list[idx]));
  }  // for (idx)
  graph_last_slot = graph_values.empty() ? 0 : start + (count - 1) * GRAPH_SLOT_SECONDS;
  ESP_LOGD(TAG, "History: %zu values, newest slot %" PRIu32, graph_values.size(), graph_last_slot);
  return true;
}

size_t graph_append(const std::vector<std::string> &txt_list) {
  uint32_t start = 0;
  if (!graph_parse_start(txt_list, start))
    return 0;
  size_t appended = 0;
  for (size_t idx = 1; idx < txt_list.size(); ++idx) {
    const uint32_t slot = start + (idx - 1) * GRAPH_SLOT_SECONDS;
    if (graph_last_slot != 0 && slot <= graph_last_slot)
      continue;  // Already shown
    if (graph_last_slot != 0 && slot - graph_last_slot > GRAPH_POINTS * GRAPH_SLOT_SECONDS) {
      // Gap longer than the graph: nothing shown is kept, so start over instead of filling it
      graph_values.clear();
      graph_last_slot = 0;
    }  // if (gap longer than the graph)
    // Fill slots missed between the newest point shown and this one
    if (graph_last_slot != 0) {
      for (uint32_t missed = graph_last_slot + GRAPH_SLOT_SECONDS; missed < slot; missed += GRAPH_SLOT_SECONDS) {
        graph_values.push_back(NAN);
        ++appended;
      }  // for (missed)
    }  // if (graph_last_slot != 0)
    graph_values.push_back(graph_parse_value(txt_list[idx]));
    graph_last_slot = slot;
    ++appended;
  }  // for (idx)
  if (graph_values.size() > GRAPH_POINTS) {
    graph_values.erase(graph_values.begin(), graph_values.end() - GRAPH_POINTS);
  }  // if (too many values)
  if (appended > 0)
    ESP_LOGD(TAG, "Appended %zu values, newest slot %" PRIu32, appended, graph_last_slot);
  return appended;
}

void graph_range(float &low, float &high) {
  float data_low = NAN;
  float data_high = NAN;
  for (const float value : graph_values) {
    if (!std::isfinite(value))
      continue;
    if (!std::isfinite(data_low) || value < data_low)
      data_low = value;
    if (!std::isfinite(data_high) || value > data_high)
      data_high = value;
  }  // for (value)
  low = graph_config.min.has_value() ? *graph_config.min : data_low;
  high = graph_config.max.has_value() ? *graph_config.max : data_high;
  if (!std::isfinite(low) && !std::isfinite(high)) {
    low = 0.0f;
    high = 1.0f;
  } else if (!std::isfinite(low)) {
    low = high - 1.0f;
  } else if (!std::isfinite(high)) {
    high = low + 1.0f;
  }  // if (bounds missing)
  if (high <= low) {
    // Flat line: widen the automatic bounds only, so a configured bound is always honoured
    const float margin = (std::fabs(low) > 1.0f) ? std::fabs(low) * 0.1f : 1.0f;
    const bool fixed_low = graph_config.min.has_value();
    const bool fixed_high = graph_config.max.has_value();
    if (fixed_low && !fixed_high) {
      high = low + margin;
    } else if (fixed_high && !fixed_low) {
      low = high - margin;
    } else {  // Both automatic (flat data), or both configured the wrong way round
      low -= margin;
      high += margin;
    }  // if (fixed bounds)
  }  // if (flat range)
}

uint8_t graph_scale(float value, float low, float high) {
  if (!std::isfinite(value) || high <= low)
    return 0;
  const float ratio = (value - low) / (high - low);
  const long pixel = std::lround(ratio * static_cast<float>(GRAPH_HEIGHT - 1));
  if (pixel < 0)
    return 0;
  if (pixel > GRAPH_HEIGHT - 1)
    return GRAPH_HEIGHT - 1;
  return static_cast<uint8_t>(pixel);
}

std::vector<uint8_t> graph_pixels(float low, float high) {
  std::vector<uint8_t> pixels;
  pixels.reserve(graph_values.size());
  float previous = NAN;
  for (const float value : graph_values) {
    if (std::isfinite(value))
      previous = value;
    pixels.push_back(graph_scale(previous, low, high));
  }  // for (value)
  return pixels;
}

std::string graph_format(float value) {
  char buffer[32];
  const char *separator = (units_separator_str != nullptr) ? units_separator_str->c_str() : "";
  if (graph_config.unit.empty()) {
    snprintf(buffer, sizeof(buffer), "%.*f", graph_config.decimals, value);
  } else {
    snprintf(buffer, sizeof(buffer), "%.*f%s%s", graph_config.decimals, value, separator, graph_config.unit.c_str());
  }  // if (unit)
  // Guarded indexing: an empty separator would be undefined behaviour here.
  const char decimal =
      (decimal_separator_str == nullptr || decimal_separator_str->empty()) ? '.' : (*decimal_separator_str)[0];
  return adjustDecimalSeparator(buffer, decimal);
}

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_GRAPH
