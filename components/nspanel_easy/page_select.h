// page_select.h

#pragma once

#ifdef NSPANEL_EASY_PAGE_SELECT

#include <string>
#include <vector>

#include "entity_snapshot.h"

namespace esphome::nspanel_easy {

/**
 * @brief Option currently selected, from the entity state.
 * @param state Entity state.
 * @return The state without surrounding whitespace, or "" when it is empty, "unknown" or "unavailable".
 */
std::string select_current_option(const std::string &state);

/**
 * @brief Title of the picker.
 * @param entity Main entity of the snapshot.
 * @param overlap_entity Entity the configured title belongs to (details overlap).
 * @param overlap_title Title configured for the entity details page (details overlap).
 * @return The configured title when it belongs to this entity, else the entity name, else the entity id.
 */
std::string select_picker_title(const SnapshotEntity &entity, const std::string &overlap_entity,
                                const std::string &overlap_title);

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_PAGE_SELECT
