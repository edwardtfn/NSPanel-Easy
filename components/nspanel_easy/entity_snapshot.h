// entity_snapshot.h

#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace esphome::nspanel_easy {

/// Largest snapshot accepted from the Blueprint, in bytes; larger ones are dropped.
constexpr size_t ENTITY_SNAPSHOT_MAX_SIZE = 8192;

/**
 * @brief One entity of a snapshot: its role on the page, state and the attributes the page needs.
 *
 * Scalar attributes are kept as text (numbers and booleans in their JSON form); list attributes
 * as lists of texts. Other JSON values (objects, lists of objects) are not kept.
 */
struct SnapshotEntity {
  std::string role;       ///< Role on the page, e.g. "main" or "battery"
  std::string entity_id;  ///< Entity id as sent by the Blueprint
  std::string state;      ///< State, as Home Assistant reports it ("unknown" when the entity is missing)
  std::string name;       ///< Home Assistant's entity name (friendly name); empty when the entity is missing
  bool exists{false};     ///< Whether the entity exists in Home Assistant
  std::map<std::string, std::string> attributes;          ///< Scalar attributes, as text
  std::map<std::string, std::vector<std::string>> lists;  ///< List attributes, as texts

  /**
   * @brief Scalar attribute as text.
   * @param key Attribute name.
   * @param fallback Text returned when the attribute was not sent.
   * @return The attribute, or @p fallback.
   */
  std::string attribute(const std::string &key, const std::string &fallback = "") const;

  /**
   * @brief List attribute.
   * @param key Attribute name.
   * @return The list, or nullptr when the attribute was not sent as a list.
   */
  const std::vector<std::string> *list(const std::string &key) const;
};

/**
 * @brief State and attributes of the entities shown on an entity details page, sent by the Blueprint.
 *
 * The Blueprint sends the raw data (`entity_snapshot` action) and the firmware renders the page,
 * instead of the Blueprint computing every text and value with its own templates (issue #636).
 */
struct EntitySnapshot {
  std::string page;                      ///< Page the snapshot is for, e.g. "select"
  bool constructor{false};               ///< True for the first snapshot after the page opens
  bool compact_unit_display{false};      ///< Units follow values without a space (Blueprint setting)
  std::vector<SnapshotEntity> entities;  ///< Entities shown on the page, each with its role

  /**
   * @brief Entity with a given role.
   * @param role Role on the page, e.g. "main".
   * @return The entity, or nullptr when the snapshot has no entity with that role.
   */
  const SnapshotEntity *find(const std::string &role) const;
};

/// Last snapshot received from the Blueprint; valid while `entity_snapshot_received` runs.
extern EntitySnapshot entity_snapshot;

/**
 * @brief Parse a snapshot sent by the Blueprint.
 * @param json Snapshot as JSON: {"page", "constructor", "compact_unit_display", "entities": [{"role", "entity_id",
 * "exists", "state", "name", "attributes": {...}}]}.
 * @param snapshot Filled with the parsed snapshot; cleared first.
 * @return true when the JSON was parsed and has a page; false otherwise (snapshot left empty).
 */
bool parse_entity_snapshot(const std::string &json, EntitySnapshot &snapshot);

/**
 * @brief Write the title and the title icon of an entity details page, from a snapshot entity.
 *
 * Writes `page_label` (the title of the button that opened the page, else the
 * entity's name) and `icon_state` (the icon of that button, else the entity's
 * icon, resolved on the panel), as the Blueprint did. Nothing is written for a
 * missing entity.
 *
 * @param entity The page's main entity.
 * @return The icon written, for pages that show it elsewhere too; empty when nothing was written.
 */
std::string details_render_title(const SnapshotEntity &entity);

}  // namespace esphome::nspanel_easy
