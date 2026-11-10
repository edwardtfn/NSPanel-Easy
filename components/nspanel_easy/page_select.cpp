// page_select.cpp

#ifdef NSPANEL_EASY_PAGE_SELECT

#include "page_select.h"

#include "page_popup_select.h"

namespace esphome::nspanel_easy {

std::string select_current_option(const std::string &state) {
  if (state.empty() || state == "unknown" || state == "unavailable") {
    return "";
  }  // if (no option selected)
  const char *whitespace = " \t\n\r\f\v";
  const size_t first = state.find_first_not_of(whitespace);
  if (first == std::string::npos) {
    return "";
  }  // if (only whitespace)
  const size_t last = state.find_last_not_of(whitespace);
  return state.substr(first, last - first + 1);
}

std::string select_picker_title(const SnapshotEntity &entity, const std::string &overlap_entity,
                                const std::string &overlap_title) {
  if (!entity.exists) {
    return entity.entity_id;
  }  // if (entity missing)
  if (!overlap_title.empty() && overlap_entity == entity.entity_id) {
    return overlap_title;
  }  // if (configured title for this entity)
  return entity.name.empty() ? entity.entity_id : entity.name;
}

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_SELECT
