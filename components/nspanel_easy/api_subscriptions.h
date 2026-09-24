// api_subscriptions.h

#pragma once

#ifdef NSPANEL_EASY_SUBSCRIBE

#include <cstdint>
#include <cstring>
#include <string>

#include "all_icons.h"
#include "nextion_constants.h"

/**
 * @file api_subscriptions.h
 * @brief Direct Home Assistant state subscriptions for NSPanel components.
 *
 * The blueprint pushes a set of bindings (which entity drives which component)
 * once per session. ESPHome persists them, re-subscribes on every boot, and
 * renders state changes locally, so no automation runs per state change.
 *
 * Why persistence is load-bearing:
 * APIConnection::process_state_subscriptions_() latches state_subs_at_ to -1
 * once the burst that follows SubscribeHomeAssistantStatesRequest drains, so a
 * subscription registered later is stored and never sent. The blueprint can
 * only push after the API is connected, which is always after that burst.
 * Bindings therefore take effect on the following boot, and a binding change
 * schedules a restart. Should ESPHome gain runtime subscriptions, the
 * persistence layer and the restart can both be removed.
 *
 * Rendering is dispatched through a function pointer resolved from the target
 * page at load time. Classification lives here and never inside a renderer, so
 * on/off semantics cannot drift between component types.
 */

namespace esphome::nspanel_easy {

/// @brief Tracked visibility of a component whose initial state is not known.
enum class Visibility : uint8_t {
  UNKNOWN,  ///< Never resolved; the next update must write, whatever it decides
  HIDDEN,
  SHOWN,
};

/// @brief Persisted format version. Bump whenever SubBinding's layout changes.
static constexpr uint8_t SUB_FORMAT_VERSION = 4;  // 4: SubHeader gained entity_len

/// @brief Upper bound on bindings; storage is allocated to the configured count, not this.
#ifndef NSPANEL_EASY_SUB_MAX
#define NSPANEL_EASY_SUB_MAX 128
#endif  // NSPANEL_EASY_SUB_MAX
static constexpr uint16_t SUB_MAX = NSPANEL_EASY_SUB_MAX;

/**
 * @brief Bindings per persisted NVS chunk.
 *
 * Deliberately small. NVS stores each preference as a single blob and needs
 * room for the new copy before releasing the old, so a large blob can fail with
 * ESP_ERR_NVS_NOT_ENOUGH_SPACE on a partition that is merely fragmented rather
 * than full. At 4 bindings a chunk is under 512 bytes, which fits comfortably
 * inside one NVS page; at 16 it was roughly 1.8 KB.
 */
static constexpr uint16_t SUB_CHUNK_SIZE = 4;

/// @brief Number of NVS chunks needed to hold SUB_MAX bindings.
static constexpr uint16_t SUB_CHUNK_COUNT = (SUB_MAX + SUB_CHUNK_SIZE - 1) / SUB_CHUNK_SIZE;

/**
 * @brief Size of the stored entity_id buffer, including the null terminator.
 *
 * Configurable because it is the one field whose natural length is set by the
 * user's Home Assistant rather than by this project: integrations that derive
 * an entity_id from a location or a device name can exceed any fixed default.
 * A truncated entity_id subscribes to an entity that does not exist, so the
 * binding is silently dead. Raising this costs SUB_ENTITY_LEN bytes per live
 * binding in RAM and widens every persisted chunk by four times the increase,
 * which is why it is a per-installation knob rather than a generous constant.
 *
 * The value takes part in the persistence contract; see SubHeader::entity_len.
 */
#ifndef NSPANEL_EASY_SUB_ENTITY_LEN
#define NSPANEL_EASY_SUB_ENTITY_LEN 96
#endif  // NSPANEL_EASY_SUB_ENTITY_LEN

static constexpr uint8_t SUB_ATTR_LEN = 24;                             ///< Longest stored attribute name
static constexpr uint8_t SUB_COMPONENT_LEN = 16;                        ///< Longest stored component name
static constexpr uint8_t SUB_DEVICE_CLASS_LEN = 16;                     ///< Longest device_class in use ("garage_door")
static constexpr uint8_t SUB_ENTITY_LEN = NSPANEL_EASY_SUB_ENTITY_LEN;  ///< entity_id buffer, null included
static constexpr uint8_t SUB_PAGE_LEN = 12;                             ///< Longest stored page name ("screensaver")
static constexpr uint8_t SUB_STATE_LEN = 24;                            ///< Longest state ("armed_custom_bypass" is 19)

// Held in a uint8_t in SubHeader, so the contract check cannot represent more.
// The lower bound is the shortest buffer that still fits a realistic entity_id.
static_assert(NSPANEL_EASY_SUB_ENTITY_LEN >= 32, "api_subscribe_entity_len must be at least 32");
static_assert(NSPANEL_EASY_SUB_ENTITY_LEN <= 255, "api_subscribe_entity_len must not exceed 255");

/// @brief Attribute subscribed alongside the state for climate bindings, unlike
///        SubBinding::attribute which is subscribed instead of the state.
static constexpr const char SUB_HVAC_ACTION[] = "hvac_action";

/// @brief Consecutive unverified commits tolerated before refusing to commit again.
static constexpr uint8_t SUB_MAX_UNVERIFIED_COMMITS = 3;

/**
 * @brief Classification of a Home Assistant state string.
 *
 * Tri-state plus a transitional case. A state belonging to no known set hides
 * the component in both polarities, so an unrecognised value never lights an
 * inverted component by accident. A transitional state shows it in both, so
 * movement is surfaced regardless of how the binding is configured.
 */
enum SubEntityState : uint8_t {
  SUB_STATE_NEITHER = 0,   ///< Unusable or unrecognised; hidden either way
  SUB_STATE_OFF,           ///< Inactive for this domain
  SUB_STATE_ON,            ///< Active for this domain
  SUB_STATE_TRANSITIONAL,  ///< In motion; shown in both polarities
  SUB_STATE_UNAVAILABLE,   ///< Entity reported unavailable; see UnavailableBehavior
};

/**
 * @brief Domains whose visible state set spans more than one state.
 *
 * Only these need on-device icon and colour resolution. Every other domain has
 * a single visible state per polarity, so the blueprint resolves the appearance
 * once and pushes it, and ESPHome only toggles visibility.
 */
enum SubDomain : uint8_t {
  SUB_DOMAIN_GENERIC = 0,   ///< Appearance supplied by the blueprint
  SUB_DOMAIN_ALARM,         ///< alarm_control_panel
  SUB_DOMAIN_CLIMATE,       ///< climate; also subscribes to the hvac_action attribute
  SUB_DOMAIN_COVER,         ///< cover; device_class dependent
  SUB_DOMAIN_LOCK,          ///< lock
  SUB_DOMAIN_WATER_HEATER,  ///< water_heater
};

/// @brief Icon and colour for a component in a given state.
struct SubAppearance {
  const char *icon;  ///< UTF-8 MDI codepoint, or nullptr when the blueprint supplies it
  uint16_t color;    ///< RGB565 foreground colour
};

/**
 * @brief Persisted binding between a Home Assistant entity and a component.
 *
 * The entity field is the storage the API points at: the const char* overload
 * of subscribe_home_assistant_state() keeps the pointer without copying. The
 * binding array is allocated once at boot and never resized, so the pointer
 * stays valid for the lifetime of the process.
 */
struct SubBinding {
  char entity[SUB_ENTITY_LEN];              ///< entity_id; empty means the slot is unused
  char page[SUB_PAGE_LEN];                  ///< Target page, e.g. "chips"
  char component[SUB_COMPONENT_LEN];        ///< Target component, e.g. "chip01"
  char device_class[SUB_DEVICE_CLASS_LEN];  ///< HA device_class; cover only, empty when unset
  char attribute[SUB_ATTR_LEN];             ///< Attribute to read instead of the state; empty to use the state
  uint8_t domain;                           ///< SubDomain, derived from entity at bind time
  bool inverted;                            ///< Show while the entity is inactive
};

/// @brief One persisted NVS chunk.
struct SubChunk {
  SubBinding bindings[SUB_CHUNK_SIZE];
};

/**
 * @brief Runtime state for a binding. Never persisted.
 *
 * Appearance arrives with the binding push and is not stored, so a component
 * stays hidden after a boot until the blueprint reconnects and pushes again.
 */
struct SubRuntime {
  char state[SUB_STATE_LEN];   ///< Last raw state string
  char action[SUB_STATE_LEN];  ///< Last hvac_action value; climate bindings only
  char icon_on[4];             ///< Blueprint-supplied icon for the active state
  char icon_off[4];            ///< Blueprint-supplied icon for the inactive state
  uint16_t color_on;           ///< Blueprint-supplied colour for the active state
  uint16_t color_off;          ///< Blueprint-supplied colour for the inactive state
  SubEntityState last_state;   ///< Classification of the effective state
  bool has_appearance;         ///< Blueprint has pushed appearance in this session
};

/**
 * @brief Renders a binding onto its target component.
 *
 * Receives an already-classified state so that on/off semantics stay in one
 * place. A renderer decides only how to draw, never whether the entity counts
 * as active.
 *
 * @param binding The binding being rendered.
 * @param rt Runtime state, including blueprint-supplied appearance.
 * @param state Effective state string; hvac_action for climate when usable.
 * @param visible Whether the component should be shown.
 */
using SubRenderFn = void (*)(const SubBinding &binding, const SubRuntime &rt, const char *state, bool visible);

/// @brief Live bindings, allocated once at boot to the persisted count.
extern SubBinding *sub_bindings;

/// @brief Runtime state, parallel to sub_bindings.
extern SubRuntime *sub_runtime;

/// @brief Render function per binding, resolved from the target page at load time.
extern SubRenderFn *sub_renderers;

/// @brief Number of live bindings.
extern uint16_t sub_count;

/**
 * @brief Test whether a state string appears in a literal state list.
 *
 * @tparam N Number of entries, deduced from the array.
 * @param state State string to test.
 * @param list State literals.
 * @return true when the state matches one of the entries.
 */
template<size_t N> inline bool sub_state_in(const char *state, const char *const (&list)[N]) {
  for (size_t i = 0; i < N; ++i) {
    if (strcmp(state, list[i]) == 0) {
      return true;
    }
  }
  return false;
}

/// @brief States that carry no usable value, for any domain.
static constexpr const char *SUB_UNUSABLE_STATES[] = {"unknown", "unavailable", "none", "None"};

/// @brief States that mean the entity itself is gone, as opposed to merely
///        having no usable value yet. "unknown" is deliberately absent: a
///        `button` or `script` entity that has never run reports "unknown"
///        while remaining perfectly actionable. "none" and "None" are also
///        absent: they are not availability states in Home Assistant and can be
///        legitimate values (e.g. a `select` option). Only "unavailable" is
///        used, matching the Blueprint render and tap guard, so every surface
///        agrees on which buttons are unavailable.
static constexpr const char *SUB_UNAVAILABLE_STATES[] = {"unavailable"};

/**
 * @brief Derive the domain from an entity_id.
 *
 * @param entity_id Full entity_id, e.g. "cover.garage_door".
 * @return Matching SubDomain, or SUB_DOMAIN_GENERIC when no on-device
 *         appearance resolution is needed.
 */
inline SubDomain parse_sub_domain(const char *entity_id) {
  const char *dot = strchr(entity_id, '.');
  if (dot == nullptr) {
    return SUB_DOMAIN_GENERIC;
  }
  const size_t len = static_cast<size_t>(dot - entity_id);

  struct DomainEntry {
    const char *name;
    SubDomain domain;
  };
  static constexpr DomainEntry DOMAINS[] = {
      {"alarm_control_panel", SUB_DOMAIN_ALARM},
      {"climate", SUB_DOMAIN_CLIMATE},
      {"cover", SUB_DOMAIN_COVER},
      {"lock", SUB_DOMAIN_LOCK},
      {"water_heater", SUB_DOMAIN_WATER_HEATER},
  };
  for (const DomainEntry &entry : DOMAINS) {
    if (strncmp(entity_id, entry.name, len) == 0 && entry.name[len] == '\0') {
      return entry.domain;
    }
  }
  return SUB_DOMAIN_GENERIC;
}

/**
 * @brief Classify a raw Home Assistant state string for a given domain.
 *
 * Deliberately not a port of the blueprint's enum.states, which omits
 * armed_night, disagrees with itself over armed_bypass vs armed_custom_bypass,
 * never resolves water_heater operation modes, and lacks "cool".
 *
 * @param domain Domain of the bound entity.
 * @param state Raw state string, or the tracked attribute value.
 * @return Classification, or SUB_STATE_TRANSITIONAL while moving.
 */
inline SubEntityState evaluate_sub_state(SubDomain domain, const char *state) {
  if (state == nullptr || state[0] == '\0') {
    return SUB_STATE_NEITHER;  // Nothing received yet, which is not the same as unavailable
  }
  if (sub_state_in(state, SUB_UNAVAILABLE_STATES)) {
    return SUB_STATE_UNAVAILABLE;
  }
  if (sub_state_in(state, SUB_UNUSABLE_STATES)) {
    return SUB_STATE_NEITHER;
  }

  switch (domain) {
    case SUB_DOMAIN_ALARM: {
      static constexpr const char *ON_STATES[] = {
          "armed_home",          "armed_away",   "armed_night", "armed_vacation",
          "armed_custom_bypass", "armed_bypass", "triggered",
      };
      static constexpr const char *TRANSITIONAL_STATES[] = {"arming", "pending", "disarming"};
      if (sub_state_in(state, ON_STATES)) {
        return SUB_STATE_ON;
      }
      if (sub_state_in(state, TRANSITIONAL_STATES)) {
        return SUB_STATE_TRANSITIONAL;
      }
      return (strcmp(state, "disarmed") == 0) ? SUB_STATE_OFF : SUB_STATE_NEITHER;
    }  // case SUB_DOMAIN_ALARM

    case SUB_DOMAIN_CLIMATE: {
      // Receives hvac_action when that attribute holds a usable value, and the
      // hvac mode otherwise, so both vocabularies are covered here.
      static constexpr const char *ON_STATES[] = {
          "heat",   "heating", "cool",     "cooling", "heat_cool",  "dry",
          "drying", "fan",     "fan_only", "auto",    "preheating", "defrosting",
      };
      static constexpr const char *OFF_STATES[] = {"off", "idle"};
      if (sub_state_in(state, ON_STATES)) {
        return SUB_STATE_ON;
      }
      return sub_state_in(state, OFF_STATES) ? SUB_STATE_OFF : SUB_STATE_NEITHER;
    }  // case SUB_DOMAIN_CLIMATE

    case SUB_DOMAIN_COVER: {
      static constexpr const char *TRANSITIONAL_STATES[] = {"opening", "closing"};
      if (sub_state_in(state, TRANSITIONAL_STATES)) {
        return SUB_STATE_TRANSITIONAL;
      }
      if (strcmp(state, "open") == 0) {
        return SUB_STATE_ON;
      }
      return (strcmp(state, "closed") == 0) ? SUB_STATE_OFF : SUB_STATE_NEITHER;
    }  // case SUB_DOMAIN_COVER

    case SUB_DOMAIN_LOCK: {
      // "jammed" counts as active: the lock is not securing anything, which is
      // precisely what a lock indicator exists to surface.
      static constexpr const char *ON_STATES[] = {"unlocked", "open", "jammed"};
      static constexpr const char *TRANSITIONAL_STATES[] = {"locking", "unlocking", "opening"};
      if (sub_state_in(state, ON_STATES)) {
        return SUB_STATE_ON;
      }
      if (sub_state_in(state, TRANSITIONAL_STATES)) {
        return SUB_STATE_TRANSITIONAL;
      }
      return (strcmp(state, "locked") == 0) ? SUB_STATE_OFF : SUB_STATE_NEITHER;
    }  // case SUB_DOMAIN_LOCK

    case SUB_DOMAIN_WATER_HEATER: {
      // The state of a water_heater is its current operation mode.
      static constexpr const char *ON_STATES[] = {
          "on", "eco", "electric", "gas", "heat_pump", "high_demand", "performance",
      };
      if (sub_state_in(state, ON_STATES)) {
        return SUB_STATE_ON;
      }
      return (strcmp(state, "off") == 0) ? SUB_STATE_OFF : SUB_STATE_NEITHER;
    }  // case SUB_DOMAIN_WATER_HEATER

    case SUB_DOMAIN_GENERIC:
    default: {
      static constexpr const char *ON_STATES[] = {"on", "true", "True", "1", "active", "home", "playing"};
      static constexpr const char *OFF_STATES[] = {
          "off", "false", "False", "0", "not_home", "idle", "standby", "paused",
      };
      if (sub_state_in(state, ON_STATES)) {
        return SUB_STATE_ON;
      }
      return sub_state_in(state, OFF_STATES) ? SUB_STATE_OFF : SUB_STATE_NEITHER;
    }  // case SUB_DOMAIN_GENERIC
  }  // switch domain
}

/**
 * @brief Resolve visibility from a classification and the inversion flag.
 *
 * @param eval Classification returned by evaluate_sub_state().
 * @param inverted Whether the binding is configured as inverted.
 * @return true when the component should be shown.
 */
inline bool sub_visible(SubEntityState eval, bool inverted) {
  if (eval == SUB_STATE_TRANSITIONAL) {
    return true;  // Movement is surfaced regardless of the configured polarity
  }
  return inverted ? (eval == SUB_STATE_OFF) : (eval == SUB_STATE_ON);
}

/**
 * @brief Per-device_class cover icons, one per state.
 *
 * Mirrors device_class_icons.cover in the blueprint. Device classes sharing the
 * same artwork are listed separately rather than aliased, so a future
 * divergence is a one-line edit. The final row, with a null device_class, is the
 * fallback for covers with no device class or an unrecognised one, and matches
 * nextion.icon.domain.cover.
 */
struct CoverIcons {
  const char *device_class;  ///< HA cover device_class, or nullptr for the fallback row
  const char *open;          ///< Icon while fully open
  const char *opening;       ///< Icon while opening
  const char *closed;        ///< Icon while fully closed
  const char *closing;       ///< Icon while closing
};

static constexpr CoverIcons COVER_ICONS[] = {
    {"awning", Icons::MDI_WINDOW_SHUTTER_OPEN, Icons::MDI_ARROW_UP_BOX, Icons::MDI_WINDOW_SHUTTER,
     Icons::MDI_ARROW_DOWN_BOX},
    {"blind", Icons::MDI_BLINDS_HORIZONTAL, Icons::MDI_ARROW_UP_BOX, Icons::MDI_BLINDS_HORIZONTAL_CLOSED,
     Icons::MDI_ARROW_DOWN_BOX},
    {"curtain", Icons::MDI_CURTAINS, Icons::MDI_ARROW_SPLIT_VERTICAL, Icons::MDI_CURTAINS_CLOSED,
     Icons::MDI_ARROW_COLLAPSE_HORIZONTAL},
    {"curtains", Icons::MDI_CURTAINS, Icons::MDI_ARROW_SPLIT_VERTICAL, Icons::MDI_CURTAINS_CLOSED,
     Icons::MDI_ARROW_COLLAPSE_HORIZONTAL},
    {"damper", Icons::MDI_CIRCLE, Icons::MDI_CIRCLE, Icons::MDI_CIRCLE_SLICE_8, Icons::MDI_CIRCLE},
    {"door", Icons::MDI_DOOR_OPEN, Icons::MDI_DOOR_OPEN, Icons::MDI_DOOR_CLOSED, Icons::MDI_DOOR_OPEN},
    {"garage", Icons::MDI_GARAGE_OPEN, Icons::MDI_ARROW_UP_BOX, Icons::MDI_GARAGE, Icons::MDI_ARROW_DOWN_BOX},
    {"garage_door", Icons::MDI_GARAGE_OPEN, Icons::MDI_ARROW_UP_BOX, Icons::MDI_GARAGE, Icons::MDI_ARROW_DOWN_BOX},
    {"gate", Icons::MDI_GATE_OPEN, Icons::MDI_GATE_ARROW_LEFT, Icons::MDI_GATE, Icons::MDI_GATE_ARROW_RIGHT},
    {"shade", Icons::MDI_ROLLER_SHADE, Icons::MDI_ARROW_UP_BOX, Icons::MDI_ROLLER_SHADE_CLOSED,
     Icons::MDI_ARROW_DOWN_BOX},
    {"shutter", Icons::MDI_WINDOW_SHUTTER_OPEN, Icons::MDI_ARROW_UP_BOX, Icons::MDI_WINDOW_SHUTTER,
     Icons::MDI_ARROW_DOWN_BOX},
    {"window", Icons::MDI_WINDOW_OPEN, Icons::MDI_ARROW_UP_BOX, Icons::MDI_WINDOW_CLOSED, Icons::MDI_ARROW_DOWN_BOX},
    {nullptr, Icons::MDI_BLINDS, Icons::MDI_BLINDS, Icons::MDI_BLINDS, Icons::MDI_BLINDS},
};

/// @brief Index of the fallback row in COVER_ICONS (the one with a null device_class).
static constexpr size_t COVER_ICONS_FALLBACK = (sizeof(COVER_ICONS) / sizeof(COVER_ICONS[0])) - 1;

/**
 * @brief Resolve the icon and colour for a component in its current state.
 *
 * Only meaningful for domains whose appearance varies across the visible state
 * set. SUB_DOMAIN_GENERIC returns a null icon, signalling the caller to use the
 * appearance the blueprint pushed.
 *
 * @param domain Domain of the bound entity.
 * @param device_class HA device_class, or an empty string when unset. Cover only.
 * @param state Effective state string; hvac_action for climate when usable.
 * @param color_on Fallback colour for active states.
 * @param color_off Fallback colour for inactive states.
 * @return Icon codepoint and RGB565 colour.
 */
inline SubAppearance resolve_sub_appearance(SubDomain domain, const char *device_class, const char *state,
                                            uint16_t color_on, uint16_t color_off) {
  switch (domain) {
    case SUB_DOMAIN_ALARM: {
      if (strcmp(state, "armed_home") == 0)
        return {Icons::MDI_SHIELD_HOME_OUTLINE, Colors::RGB565_GREEN};
      if (strcmp(state, "armed_away") == 0)
        return {Icons::MDI_SHIELD_LOCK_OUTLINE, Colors::RGB565_GREEN};
      if (strcmp(state, "armed_night") == 0)
        return {Icons::MDI_SHIELD_MOON_OUTLINE, Colors::RGB565_GREEN};
      if (strcmp(state, "armed_vacation") == 0)
        return {Icons::MDI_SHIELD_AIRPLANE_OUTLINE, Colors::RGB565_GREEN};
      if (strcmp(state, "armed_custom_bypass") == 0 || strcmp(state, "armed_bypass") == 0)
        return {Icons::MDI_SHIELD_HALF_FULL, Colors::RGB565_GREEN};
      if (strcmp(state, "triggered") == 0)
        return {Icons::MDI_SHIELD_ALERT_OUTLINE, Colors::RGB565_RED};
      if (strcmp(state, "arming") == 0 || strcmp(state, "pending") == 0)
        return {Icons::MDI_SHIELD_OUTLINE, Colors::RGB565_YELLOW};
      if (strcmp(state, "disarming") == 0)
        return {Icons::MDI_SHIELD_OFF_OUTLINE, Colors::RGB565_YELLOW};
      if (strcmp(state, "disarmed") == 0)
        return {Icons::MDI_SHIELD_OFF_OUTLINE, color_off};
      return {Icons::MDI_SHIELD, color_on};
    }  // case SUB_DOMAIN_ALARM

    case SUB_DOMAIN_CLIMATE: {
      // heat_cool is tested before heat, and fan_only before fan, so that the
      // more specific mode wins — the blueprint relies on substring order here.
      if (strcmp(state, "off") == 0)
        return {Icons::MDI_THERMOSTAT, color_off};
      if (strcmp(state, "heat_cool") == 0)
        return {Icons::MDI_AUTORENEW, Colors::RGB565_YELLOW};
      if (strcmp(state, "heating") == 0 || strcmp(state, "heat") == 0)
        return {Icons::MDI_THERMOMETER_LINES, Colors::RGB565_DEEP_ORANGE};
      if (strcmp(state, "cooling") == 0 || strcmp(state, "cool") == 0)
        return {Icons::MDI_SNOWFLAKE, Colors::RGB565_BLUE};
      if (strcmp(state, "drying") == 0 || strcmp(state, "dry") == 0)
        return {Icons::MDI_WATER_PERCENT, Colors::RGB565_ORANGE};
      if (strcmp(state, "fan_only") == 0 || strcmp(state, "fan") == 0)
        return {Icons::MDI_FAN, Colors::RGB565_CYAN};
      if (strcmp(state, "preheating") == 0)
        return {Icons::MDI_HEAT_WAVE, Colors::RGB565_DEEP_ORANGE};
      if (strcmp(state, "defrosting") == 0)
        return {Icons::MDI_SNOWFLAKE_MELT, Colors::RGB565_BLUE};
      if (strcmp(state, "auto") == 0)
        return {Icons::MDI_REFRESH_AUTO, Colors::RGB565_GREEN};
      if (strcmp(state, "idle") == 0)
        return {Icons::MDI_THERMOMETER, color_off};
      return {Icons::MDI_THERMOSTAT, color_on};
    }  // case SUB_DOMAIN_CLIMATE

    case SUB_DOMAIN_COVER: {
      const CoverIcons *row = &COVER_ICONS[COVER_ICONS_FALLBACK];
      if (device_class != nullptr && device_class[0] != '\0') {
        for (const CoverIcons &candidate : COVER_ICONS) {
          if (candidate.device_class != nullptr && strcmp(device_class, candidate.device_class) == 0) {
            row = &candidate;
            break;
          }
        }
      }
      if (strcmp(state, "opening") == 0)
        return {row->opening, Colors::RGB565_YELLOW};
      if (strcmp(state, "closing") == 0)
        return {row->closing, Colors::RGB565_YELLOW};
      if (strcmp(state, "closed") == 0)
        return {row->closed, color_off};
      return {row->open, color_on};
    }  // case SUB_DOMAIN_COVER

    case SUB_DOMAIN_LOCK: {
      if (strcmp(state, "locked") == 0)
        return {Icons::MDI_LOCK, Colors::RGB565_GREEN};
      if (strcmp(state, "unlocked") == 0 || strcmp(state, "open") == 0)
        return {Icons::MDI_LOCK_OPEN_VARIANT, Colors::RGB565_RED};
      if (strcmp(state, "locking") == 0 || strcmp(state, "unlocking") == 0 || strcmp(state, "opening") == 0)
        return {Icons::MDI_LOCK_CLOCK, Colors::RGB565_YELLOW};
      return {Icons::MDI_LOCK_ALERT, Colors::RGB565_RED};  // jammed, or anything unrecognised
    }  // case SUB_DOMAIN_LOCK

    case SUB_DOMAIN_WATER_HEATER: {
      if (strcmp(state, "off") == 0)
        return {Icons::MDI_WATER_BOILER_OFF, color_off};
      if (strcmp(state, "on") == 0)
        return {Icons::MDI_WATER_BOILER, Colors::RGB565_ORANGE};
      if (strcmp(state, "eco") == 0)
        return {Icons::MDI_LEAF, Colors::RGB565_GREEN};
      if (strcmp(state, "electric") == 0)
        return {Icons::MDI_LIGHTNING_BOLT, Colors::RGB565_YELLOW};
      if (strcmp(state, "gas") == 0)
        return {Icons::MDI_FIRE, Colors::RGB565_DEEP_ORANGE};
      if (strcmp(state, "heat_pump") == 0)
        return {Icons::MDI_HEAT_PUMP, Colors::RGB565_BLUE};
      if (strcmp(state, "performance") == 0)
        return {Icons::MDI_HEAT_WAVE, Colors::RGB565_RED};
      if (strcmp(state, "high_demand") == 0)
        return {Icons::MDI_SPEEDOMETER, Colors::RGB565_RED};
      return {Icons::MDI_WATER_BOILER, color_on};
    }  // case SUB_DOMAIN_WATER_HEATER

    case SUB_DOMAIN_GENERIC:
    default:
      return {nullptr, color_on};  // Caller uses the blueprint-supplied appearance
  }  // switch domain
}

//
// Engine API. Implemented in api_subscriptions.cpp.
//

/**
 * @brief Resolve the render function for a target page.
 *
 * @param page Target page name as pushed by the blueprint.
 * @return Render function, or nullptr when the page has no renderer.
 */
SubRenderFn sub_resolve_renderer(const char *page);

/**
 * @brief Load persisted bindings, allocate storage, and resolve renderers.
 *
 * Call once from boot_early_routines, before the API connects.
 *
 * @return Number of bindings loaded.
 */
uint16_t sub_load();

/**
 * @brief Register a Home Assistant state subscription for every loaded binding.
 *
 * Must run before the API connects: subscriptions added after the initial burst
 * are stored by ESPHome and never sent.
 */
void sub_subscribe_all();

/**
 * @brief Accept one binding from the blueprint's push.
 *
 * The first call after a completed or abandoned push implicitly starts a new
 * one. Appearance is applied to the live binding immediately when the target
 * matches; the entity is staged and only takes effect after a restart.
 *
 * @param page Target page.
 * @param component Target component.
 * @param entity Home Assistant entity_id.
 * @param device_class HA device_class, or empty.
 * @param attribute HA attribute to subscribe to.
 * @param icon_on Icon for the active state, or empty to resolve on-device.
 * @param icon_off Icon for the inactive state, or empty.
 * @param color_on RGB565 colour for the active state.
 * @param color_off RGB565 colour for the inactive state.
 * @param inverted Show while the entity is inactive.
 */
void sub_push_binding(const char *page, const char *component, const char *entity, const char *device_class,
                      const char *attribute, const char *icon_on, const char *icon_off, uint16_t color_on,
                      uint16_t color_off, bool inverted);

/**
 * @brief Close the blueprint's push.
 *
 * @param count Number of bindings the blueprint sent. A mismatch discards the
 *              push and leaves the persisted set untouched.
 * @return true when the staged set differs from the persisted one, was saved
 *         successfully, and a restart is therefore required. false when nothing
 *         changed, the push was rejected, or the save failed -- restarting on an
 *         unpersisted set would reload the old bindings and loop forever.
 */
bool sub_push_end(uint16_t count);

/**
 * @brief Commit a push that never received its end marker.
 *
 * Called from the watchdog timeout. Refuses to commit after
 * SUB_MAX_UNVERIFIED_COMMITS consecutive unverified commits, so a systematically
 * failing push cannot produce a boot loop.
 *
 * @return true when the set was saved and a restart is required. See
 *         sub_push_end() for why a failed save must not restart.
 */
bool sub_push_timeout();

/**
 * @brief Persist the staged set.
 *
 * Written in three phases -- empty header, chunks, real header -- so that a
 * failure part-way through leaves the stored set empty rather than a mixture of
 * old and new chunks. The unverified-commit counter only advances once the
 * header has reached NVS.
 *
 * @param verified Whether the push was confirmed by a matching end marker.
 * @return true when every chunk and the header reached NVS.
 */
bool sub_persist(bool verified);

/// @brief Re-render every binding from its last known state.
void sub_render_all();

/// @brief Log the loaded bindings during dump_config.
void sub_dump_config();

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_SUBSCRIBE
