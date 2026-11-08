// page_select.cpp

#ifdef NSPANEL_EASY_PAGE_SELECT

#include "page_select.h"

#include <algorithm>

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

std::vector<std::string> select_picker_options(const std::vector<std::string> &options, const std::string &current) {
  const size_t limit = POPUP_SELECT_MAX_OPTIONS;
  if (options.size() <= limit) {
    return options;
  }  // if (all options fit)
  const auto shown_end = options.begin() + limit;
  if (current.empty() || std::find(options.begin(), shown_end, current) != shown_end) {
    return std::vector<std::string>(options.begin(), shown_end);
  }  // if (no current option, or already among the first ones)
  // The current option first, then the first options but one (without the current option)
  std::vector<std::string> shown{current};
  for (auto it = options.begin(); it != options.begin() + (limit - 1); ++it) {
    if (*it != current) {
      shown.push_back(*it);
    }  // if (not the current option)
  }  // for each of the first options but one
  return shown;
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
