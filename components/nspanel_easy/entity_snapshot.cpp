// entity_snapshot.cpp

#include "entity_snapshot.h"

#include <cstdio>

#include "esphome/components/json/json_util.h"
#include "esphome/components/nextion/nextion.h"

#include "nextion_components.h"

#ifdef NSPANEL_EASY_HW_DISPLAY
#include "hw_display.h"
#endif  // NSPANEL_EASY_HW_DISPLAY

#ifdef NSPANEL_EASY_SUBSCRIBE
#include "api_subscriptions.h"
#endif  // NSPANEL_EASY_SUBSCRIBE

namespace esphome::nspanel_easy {

EntitySnapshot entity_snapshot;

std::string SnapshotEntity::attribute(const std::string &key, const std::string &fallback) const {
  const auto it = this->attributes.find(key);
  return (it == this->attributes.end()) ? fallback : it->second;
}

const std::vector<std::string> *SnapshotEntity::list(const std::string &key) const {
  const auto it = this->lists.find(key);
  return (it == this->lists.end()) ? nullptr : &it->second;
}

const SnapshotEntity *EntitySnapshot::find(const std::string &role) const {
  for (const auto &entity : this->entities) {
    if (entity.role == role) {
      return &entity;
    }  // if (entity.role == role)
  }  // for each entity
  return nullptr;
}

/**
 * @brief JSON value as text: strings as they are, numbers and booleans in their JSON form.
 * @param value Value to convert.
 * @param text Receives the text.
 * @return false for null, objects and lists, which have no text form here.
 */
static bool json_to_text(JsonVariantConst value, std::string &text) {
  if (value.isNull() || value.is<JsonObjectConst>() || value.is<JsonArrayConst>()) {
    return false;
  }  // if (no text form)
  if (value.is<const char *>()) {
    text = value.as<const char *>();
    return true;
  }  // if (string)
  text.clear();
  serializeJson(value, text);
  return true;
}

bool parse_entity_snapshot(const std::string &json, EntitySnapshot &snapshot) {
  snapshot = EntitySnapshot{};
  const bool parsed = json::parse_json(json, [&snapshot](JsonObject root) -> bool {
    snapshot.page = root["page"] | "";
    snapshot.constructor = root["constructor"] | false;
    snapshot.compact_unit_display = root["compact_unit_display"] | false;
    snapshot.settled = root["settled"] | false;
    // Ranges are kept in locals: iterating over the result of as<>() directly makes GCC warn about a
    // dangling reference to the temporary proxy (-Wdangling-reference).
    const JsonArrayConst items = root["entities"].as<JsonArrayConst>();
    for (JsonObjectConst item : items) {
      SnapshotEntity entity;
      entity.role = item["role"] | "";
      entity.entity_id = item["entity_id"] | "";
      entity.state = item["state"] | "unknown";
      entity.name = item["name"] | "";
      entity.exists = item["exists"] | false;
      const JsonObjectConst attributes = item["attributes"].as<JsonObjectConst>();
      for (JsonPairConst attribute : attributes) {
        const std::string key = attribute.key().c_str();
        if (attribute.value().is<JsonArrayConst>()) {
          std::vector<std::string> texts;
          const JsonArrayConst elements = attribute.value().as<JsonArrayConst>();
          for (JsonVariantConst element : elements) {
            std::string text;
            if (json_to_text(element, text)) {
              texts.push_back(std::move(text));
            }  // if (element has a text form)
          }  // for each list element
          entity.lists[key] = std::move(texts);
        } else {
          std::string text;
          if (json_to_text(attribute.value(), text)) {
            entity.attributes[key] = std::move(text);
          }  // if (attribute has a text form)
        }  // if (list attribute)
      }  // for each attribute
      snapshot.entities.push_back(std::move(entity));
    }  // for each entity
    return true;
  });
  if (!parsed || snapshot.page.empty()) {
    snapshot = EntitySnapshot{};
    return false;
  }  // if (not parsed or no page)
  return true;
}

/**
 * @brief Days from 1970-01-01 to a civil date (proleptic Gregorian calendar).
 *
 * @param year Year.
 * @param month Month, 1 to 12.
 * @param day Day, 1 to 31.
 * @return Days since the Unix epoch.
 */
static int64_t days_from_civil(int64_t year, unsigned month, unsigned day) {
  year -= month <= 2 ? 1 : 0;
  const int64_t era = (year >= 0 ? year : year - 399) / 400;
  const unsigned yoe = static_cast<unsigned>(year - era * 400);
  const unsigned doy = (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
  return era * 146097 + static_cast<int64_t>(doe) - 719468;
}

bool parse_iso_datetime(const std::string &text, int64_t &epoch) {
  int year = 0;
  unsigned month = 0;
  unsigned day = 0;
  unsigned hour = 0;
  unsigned minute = 0;
  unsigned second = 0;
  int consumed = 0;
  if (sscanf(text.c_str(), "%d-%u-%uT%u:%u:%u%n", &year, &month, &day, &hour, &minute, &second, &consumed) != 6 ||
      month < 1 || month > 12 || day < 1 || day > 31) {
    return false;
  }
  const char *rest = text.c_str() + consumed;
  if (*rest == '.') {  // Fraction of a second
    ++rest;
    while (*rest >= '0' && *rest <= '9') {
      ++rest;
    }
  }
  int offset = 0;
  if (*rest == 'Z') {
    offset = 0;
  } else if (*rest == '+' || *rest == '-') {
    unsigned offset_hours = 0;
    unsigned offset_minutes = 0;
    if (sscanf(rest + 1, "%u:%u", &offset_hours, &offset_minutes) != 2) {
      return false;
    }
    offset = static_cast<int>((offset_hours * 3600) + (offset_minutes * 60)) * (*rest == '-' ? -1 : 1);
  } else {
    return false;  // A time without an offset cannot be placed
  }
  epoch = (days_from_civil(year, month, day) * 86400) + (hour * 3600) + (minute * 60) + second - offset;
  return true;
}

std::string details_render_title(const SnapshotEntity &entity) {
  if (nextion_display == nullptr || !entity.exists) {
    return "";
  }
#ifdef NSPANEL_EASY_HW_DISPLAY
  // The title and icon of the button that opened the page, while the page shows its entity
  const bool overlap = (details_overlap.entity == entity.entity_id);
  const std::string &overlap_title = overlap ? details_overlap.title : std::string();
  const std::string &overlap_icon = overlap ? details_overlap.icon : std::string();
#else   // NSPANEL_EASY_HW_DISPLAY
  const std::string overlap_title;
  const std::string overlap_icon;
#endif  // NSPANEL_EASY_HW_DISPLAY
  const std::string &title = overlap_title.empty() ? entity.name : overlap_title;
  if (!title.empty()) {
    nextion_display->set_component_text("page_label", title.c_str());
  }
#ifdef NSPANEL_EASY_SUBSCRIBE
  const std::string icon =
      resolve_entity_icon(entity.entity_id.c_str(), entity.attribute("device_class").c_str(), entity.state.c_str(),
                          overlap_icon.c_str(), entity.attribute("icon").c_str());
  nextion_display->set_component_text("icon_state", icon.c_str());
  return icon;
#else   // NSPANEL_EASY_SUBSCRIBE
  return "";
#endif  // NSPANEL_EASY_SUBSCRIBE
}

}  // namespace esphome::nspanel_easy
