// api_subscriptions.cpp

#ifdef NSPANEL_EASY_SUBSCRIBE

#include "api_subscriptions.h"
#include "base.h"
#include "icon_rules.h"
#include "nextion_components.h"

#include <algorithm>
#include <cinttypes>
#include <cstdio>
#include <cstring>

#include <esp_attr.h>
#include <esp_heap_caps.h>
#include <nvs.h>

#include "esphome/components/api/api_server.h"
#include "esphome/core/application.h"
#include "esphome/core/helpers.h"
#include "esphome/core/log.h"
#include "esphome/core/preferences.h"

#ifdef NSPANEL_EASY_CHIPS
#include "chips.h"
#endif  // NSPANEL_EASY_CHIPS
#ifdef NSPANEL_EASY_PAGE_HOME
#include "page_home.h"
#endif  // NSPANEL_EASY_PAGE_HOME
#ifdef NSPANEL_EASY_HW_DISPLAY
#include "hw_display.h"
#endif  // NSPANEL_EASY_HW_DISPLAY
#ifdef NSPANEL_EASY_HW_TEMPERATURE
#include "hw_temperature.h"
#endif  // NSPANEL_EASY_HW_TEMPERATURE
#ifdef NSPANEL_EASY_PAGE_BUTTONS
#include "page_buttons.h"
#endif  // NSPANEL_EASY_PAGE_BUTTONS
#ifdef NSPANEL_EASY_PAGE_CLIMATE
#include "page_climate.h"
#endif  // NSPANEL_EASY_PAGE_CLIMATE

namespace esphome::nspanel_easy {

static const char *const TAG = "nspanel.api.sub";

SubBinding *sub_bindings = nullptr;
SubRuntime *sub_runtime = nullptr;
SubRenderFn *sub_renderers = nullptr;
uint16_t sub_count = 0;

/**
 * @brief Persisted header of the bindings.
 *
 * Describes the packed stream stored in the chunks. Written last, after every chunk
 * reached NVS, so it never describes chunks that are not there (#374).
 */
struct SubHeader {
  uint8_t version;     ///< SUB_FORMAT_VERSION at the time of writing
  uint8_t unverified;  ///< Consecutive commits that never received an end marker
  uint16_t count;      ///< Number of persisted bindings
  uint32_t length;     ///< Bytes of the packed stream
  uint32_t checksum;   ///< sub_checksum() of the packed stream
};
static_assert(sizeof(SubHeader) == 12, "SubHeader must have no implicit padding");

/// @brief Starting value of sub_checksum() (FNV-1a offset basis).
static constexpr uint32_t SUB_CHECKSUM_INIT = 2166136261U;

/**
 * @brief Extend a checksum (FNV-1a, 32 bits) with bytes of the packed stream.
 *
 * Detects chunks that do not belong to the set described by the header: left from an
 * older set, missing, or damaged. It is not a protection against deliberate changes.
 *
 * @param checksum Checksum so far; SUB_CHECKSUM_INIT for the first bytes.
 * @param bytes Bytes to add.
 * @param size Number of bytes.
 * @return The extended checksum.
 */
static uint32_t sub_checksum(uint32_t checksum, const uint8_t *bytes, size_t size) {
  for (size_t i = 0; i < size; ++i) {
    checksum = (checksum ^ bytes[i]) * 16777619U;  // FNV-1a prime
  }  // for each byte
  return checksum;
}

/// @brief Bit of the flags byte of a packed binding: show while the entity is inactive.
static constexpr uint8_t SUB_PACK_FLAG_INVERTED = 0x01;

/**
 * @brief Pack bindings: for each one, the flags byte and its five strings, null-terminated.
 *
 * @param bindings Bindings to pack.
 * @param count Number of bindings.
 * @param out Destination, of at least count * SUB_PACK_BINDING_MAX bytes.
 * @return Bytes written.
 */
static uint32_t sub_pack(const SubBinding *bindings, uint16_t count, uint8_t *out) {
  uint32_t length = 0;
  const auto put = [&](const char *text, size_t size) {
    const size_t used = strnlen(text, size - 1);  // The fields are always terminated; this is a bound only
    memcpy(out + length, text, used);
    length += used;
    out[length++] = '\0';
  };
  for (uint16_t idx = 0; idx < count; ++idx) {
    const SubBinding &binding = bindings[idx];
    out[length++] = binding.inverted ? SUB_PACK_FLAG_INVERTED : 0;
    put(binding.entity, sizeof(binding.entity));
    put(binding.page, sizeof(binding.page));
    put(binding.component, sizeof(binding.component));
    put(binding.device_class, sizeof(binding.device_class));
    put(binding.attribute, sizeof(binding.attribute));
  }  // for each binding
  return length;
}

/**
 * @brief Unpack bindings packed by sub_pack().
 *
 * Every string must be terminated inside the stream and fit its field, as the stream
 * may be damaged; the domain is left at zero, as it is derived from the entity_id.
 *
 * @param in Packed stream.
 * @param length Bytes of the stream.
 * @param bindings Destination, of count bindings.
 * @param count Number of bindings the stream must hold, exactly.
 * @return false when the stream is not count well-formed bindings.
 */
static bool sub_unpack(const uint8_t *in, uint32_t length, SubBinding *bindings, uint16_t count) {
  uint32_t pos = 0;
  const auto get = [&](char *field, size_t size) {
    const uint8_t *start = in + pos;
    const void *end = memchr(start, '\0', length - pos);
    if (end == nullptr) {
      return false;
    }  // if (not terminated)
    const size_t used = static_cast<const uint8_t *>(end) - start;
    if (used >= size) {
      return false;
    }  // if (does not fit)
    memcpy(field, start, used);
    field[used] = '\0';
    pos += used + 1;
    return true;
  };
  for (uint16_t idx = 0; idx < count; ++idx) {
    SubBinding &binding = bindings[idx];
    memset(&binding, 0, sizeof(binding));
    if (pos >= length) {
      return false;
    }  // if (stream too short)
    const uint8_t flags = in[pos++];
    binding.inverted = (flags & SUB_PACK_FLAG_INVERTED) != 0;
    if (!get(binding.entity, sizeof(binding.entity)) || !get(binding.page, sizeof(binding.page)) ||
        !get(binding.component, sizeof(binding.component)) ||
        !get(binding.device_class, sizeof(binding.device_class)) ||
        !get(binding.attribute, sizeof(binding.attribute))) {
      return false;
    }  // if (a string is damaged)
  }  // for each binding
  return pos == length;
}

/// @brief Staging buffer, allocated for the duration of a push only.
static SubBinding *sub_staging = nullptr;

/// @brief Bindings received in the push currently in progress.
static uint16_t sub_staged = 0;

/// @brief Bindings of the push in progress dropped for exceeding SUB_MAX. Counted
///        against the end marker, so the bindings that fit are still committed.
static uint16_t sub_dropped = 0;

/// @brief Whether a push is currently in progress.
static bool sub_pushing = false;

/// @brief Identifier of the push in progress.
static char sub_push_id[SUB_PUSH_ID_LEN] = "";

/// @brief Identifier of the last push that ended, so a late call from an older push cannot start a new one.
static char sub_last_push_id[SUB_PUSH_ID_LEN] = "";

/// @brief When the last push ended, to limit how long sub_last_push_id is enforced.
static uint32_t sub_last_push_end_ms = 0;

/**
 * @brief Whether a call belongs to an older push than the newest one seen.
 *
 * While a push is in progress, the call is compared with that push. Otherwise,
 * it is compared with the last push that ended, but only for
 * SUB_LATE_CALL_WINDOW_MS: a late call from an older push arrives within
 * seconds, and the limit keeps a Home Assistant clock that moved backwards
 * from blocking every later push.
 *
 * An identifier too long to be stored is ignored as well, so it can never be
 * truncated and then mismatch its own end marker.
 *
 * @param push_id Identifier of the call's push.
 * @return true when the call must be ignored.
 */
static bool sub_push_superseded(const char *push_id) {
  if (strlen(push_id) >= SUB_PUSH_ID_LEN) {
    ESP_LOGW(TAG, "Push identifier longer than %" PRIu8 " characters ignored", SUB_PUSH_ID_LEN - 1);
    return true;
  }  // if identifier too long
  if (sub_pushing)
    return strcmp(push_id, sub_push_id) < 0;
  if ((uint32_t) (App.get_loop_component_start_time() - sub_last_push_end_ms) >= SUB_LATE_CALL_WINDOW_MS)
    return false;
  return strcmp(push_id, sub_last_push_id) < 0;
}

/// @brief Updates received from Home Assistant in the current statistics window.
static uint32_t sub_stats_received = 0;

/// @brief Updates in the current statistics window that changed a value.
static uint32_t sub_stats_changed = 0;

/**
 * @brief Count one update received for a binding.
 *
 * @param idx Binding index.
 */
static void sub_stats_count(uint16_t idx) {
  ++sub_stats_received;
  SubRuntime &rt = sub_runtime[idx];
  if (rt.received < UINT16_MAX) {
    ++rt.received;
  }
}

/// @brief Consecutive commits that never received an end marker.
static uint8_t sub_unverified = 0;

/**
 * @brief Allocate, preferring PSRAM and falling back to internal DRAM.
 *
 * Every NSPanel carries PSRAM, so allocations normally land there. The internal
 * DRAM fallback is defensive, covering a failed PSRAM initialization (e.g. a
 * hardware defect). Allocations here are made once and never resized, which is
 * what lets the API hold raw pointers into the binding array.
 *
 * @param bytes Size to allocate.
 * @return Zeroed allocation, or nullptr on failure.
 */
static void *sub_alloc(size_t bytes) {
  void *ptr = heap_caps_calloc(1, bytes, MALLOC_CAP_SPIRAM);
  if (ptr == nullptr) {
    ptr = heap_caps_calloc(1, bytes, MALLOC_CAP_INTERNAL);
  }
  return ptr;
}

/**
 * @brief Preference handle for the persisted header.
 *
 * @return Preference object for SubHeader.
 */
static ESPPreferenceObject sub_header_pref() {
  return global_preferences->make_preference<SubHeader>(fnv1_hash("nspanel_easy_sub_header"));
}

// Opt-in erase of the stored preferences when the bindings cannot be stored
// (api_subscribe_wipe_nvs_on_failure); off unless the build sets it.
#ifndef NSPANEL_EASY_SUB_WIPE_NVS_ON_FAILURE
#define NSPANEL_EASY_SUB_WIPE_NVS_ON_FAILURE false
#endif  // NSPANEL_EASY_SUB_WIPE_NVS_ON_FAILURE

/// @brief Whether the stored preferences are erased, once, when the bindings cannot be stored.
static constexpr bool SUB_WIPE_NVS_ON_FAILURE = NSPANEL_EASY_SUB_WIPE_NVS_ON_FAILURE;

/// @brief Value of sub_wipe_marker after the preferences were erased ("NVSW").
static constexpr uint32_t SUB_WIPE_MAGIC = 0x4E565357;

/**
 * @brief Tells that the preferences were already erased since the panel was powered on.
 *
 * Kept in RTC memory, which survives a restart but not a power cycle, as it cannot be kept in the
 * storage that was just erased. Holds SUB_WIPE_MAGIC after an erase, and anything else otherwise
 * (it is not initialized at power on).
 */
static RTC_NOINIT_ATTR uint32_t sub_wipe_marker;

/**
 * @brief Handle a failure to store the bindings: report the storage usage and, when enabled, erase it.
 *
 * With `api_subscribe_wipe_nvs_on_failure`, every stored preference is erased, as a factory reset
 * does, and the panel restarts, so the bindings can be stored in the space that is freed. This is
 * done only once since the panel was powered on: when they still do not fit, erasing again would
 * be a restart loop.
 *
 * @param count Number of bindings that could not be stored.
 * @return true when the preferences were erased and the panel is restarting.
 */
static bool sub_persist_failed(uint16_t count) {
  // How full the storage is, as the usual cause is a small or fragmented NVS partition
  nvs_stats_t nvs{};
  if (nvs_get_stats(nullptr, &nvs) == ESP_OK) {
    ESP_LOGE(TAG, "NVS entries: %zu used, %zu free of %zu", nvs.used_entries, nvs.free_entries, nvs.total_entries);
  }  // if (NVS statistics available)
  if (!SUB_WIPE_NVS_ON_FAILURE) {
    return false;
  }  // if (not enabled)
  if (sub_wipe_marker == SUB_WIPE_MAGIC) {
    ESP_LOGE(TAG, "The preferences were already erased since the panel was powered on; not erasing them again");
    return false;
  }  // if (already erased)
  ESP_LOGE(TAG,
           "Could not persist %" PRIu16 " binding(s); erasing every stored preference and restarting, "
           "as api_subscribe_wipe_nvs_on_failure is enabled",
           count);
  sub_wipe_marker = SUB_WIPE_MAGIC;
  global_preferences->reset();
  App.safe_reboot();
  return true;
}

/**
 * @brief Preference handle for one chunk of the packed stream.
 *
 * A key of its own, not the one of the chunks of format 7 and older, which
 * held whole bindings; those are erased by sub_erase_legacy_chunks().
 *
 * @param chunk Chunk index.
 * @return Preference object for SubPackChunk.
 */
static ESPPreferenceObject sub_chunk_pref(uint16_t chunk) {
  return global_preferences->make_preference<SubPackChunk>(fnv1_hash("nspanel_easy_sub_pack") + chunk);
}

/**
 * @brief Erase stored preferences by key, straight from NVS.
 *
 * ESPHome's preferences have no way to delete a key, and a key that is never
 * written again keeps its space. ESPHome stores a preference under the decimal
 * text of its key, in the "esphome" namespace.
 *
 * @param base Key of the first preference.
 * @param first Offset of the first key to erase.
 * @param last Offset past the last key to erase.
 * @return Number of keys that were found and erased.
 */
static uint16_t sub_erase_keys(uint32_t base, uint16_t first, uint16_t last) {
  nvs_handle_t handle;
  if (nvs_open("esphome", NVS_READWRITE, &handle) != ESP_OK) {
    return 0;
  }  // if (NVS not available)
  uint16_t erased = 0;
  for (uint16_t idx = first; idx < last; ++idx) {
    char key[11];  // A uint32_t in decimal, null included
    snprintf(key, sizeof(key), "%" PRIu32, static_cast<uint32_t>(base + idx));
    if (nvs_erase_key(handle, key) == ESP_OK) {
      ++erased;
    }  // if (erased)
  }  // for each key
  if (erased > 0) {
    nvs_commit(handle);
  }  // if (anything erased)
  nvs_close(handle);
  return erased;
}

/**
 * @brief Erase the chunks of format 7 and older, which held whole bindings.
 *
 * They are left in NVS by an update, as their keys are no longer written, and
 * on a small partition they take the space the packed chunks need (#790).
 * Erasing a key that is not there costs a lookup only.
 */
static void sub_erase_legacy_chunks() {
  static constexpr uint16_t LEGACY_CHUNK_COUNT = (SUB_MAX + 3) / 4;  // 4 bindings per chunk in format 7
  const uint16_t erased = sub_erase_keys(fnv1_hash("nspanel_easy_sub_chunk"), 0, LEGACY_CHUNK_COUNT);
  if (erased > 0) {
    ESP_LOGI(TAG, "Erased %" PRIu16 " chunk(s) of bindings stored by an older firmware", erased);
  }  // if (anything erased)
}

/**
 * @brief Whether the header stored in NVS is the given one.
 *
 * Read back after a sync, as its result covers every queued preference: another
 * component's failed write does not mean that this header was not written.
 *
 * @param expected Header that should be stored.
 * @return true when the stored header matches.
 */
static bool sub_header_stored(const SubHeader &expected) {
  SubHeader stored{};
  return sub_header_pref().load(&stored) && memcmp(&stored, &expected, sizeof(SubHeader)) == 0;
}

/**
 * @brief Whether a chunk stored in NVS is the given one.
 *
 * @param chunk Chunk index.
 * @param expected Chunk that should be stored.
 * @return true when the stored chunk matches.
 * @see sub_header_stored()
 */
static bool sub_chunk_stored(uint16_t chunk, const SubPackChunk &expected) {
  SubPackChunk stored{};
  return sub_chunk_pref(chunk).load(&stored) && memcmp(&stored, &expected, sizeof(SubPackChunk)) == 0;
}

/**
 * @brief Copy a string into a fixed buffer, always null-terminating.
 *
 * @param dest Destination buffer.
 * @param size Size of the destination buffer.
 * @param src Source string, may be nullptr.
 * @return true when the whole source fitted; false when it was truncated.
 */
static bool sub_copy(char *dest, size_t size, const char *src) {
  if (src == nullptr) {
    dest[0] = '\0';
    return true;
  }
  strncpy(dest, src, size - 1);
  dest[size - 1] = '\0';
  return strlen(src) < size;
}

/**
 * @brief Test whether two bindings would produce the same subscription.
 *
 * Only the entity and its target participate. device_class, inverted and
 * appearance are applied live during a push, so a change to any of them must
 * never trigger a restart.
 *
 * @param a First binding.
 * @param b Second binding.
 * @return true when both would subscribe identically.
 */
static bool sub_binding_equal(const SubBinding &a, const SubBinding &b) {
  return strcmp(a.entity, b.entity) == 0 && strcmp(a.page, b.page) == 0 && strcmp(a.component, b.component) == 0 &&
         strcmp(a.attribute, b.attribute) == 0;
}

/**
 * @brief Find the binding that drives a given target.
 *
 * A target is a page and component pair. The blueprint sends each target at
 * most once per push, so a target is unique within any well-formed set.
 *
 * @param set Bindings to search.
 * @param count Number of bindings in set.
 * @param page Target page.
 * @param component Target component.
 * @return Index of the matching binding, or count when the target is absent.
 */
static uint16_t sub_find_target(const SubBinding *set, uint16_t count, const char *page, const char *component) {
  for (uint16_t idx = 0; idx < count; ++idx) {
    if (strcmp(set[idx].page, page) == 0 && strcmp(set[idx].component, component) == 0) {
      return idx;
    }
  }  // for each binding
  return count;
}

/**
 * @brief Recompute and render one binding from its stored state.
 *
 * Classification happens here so that renderers never interpret a raw state.
 *
 * @param idx Binding index.
 */
static void sub_apply(uint16_t idx) {
  const SubBinding &binding = sub_bindings[idx];
  SubRuntime &rt = sub_runtime[idx];
  const SubDomain domain = static_cast<SubDomain>(binding.domain);

  // Climate prefers hvac_action over the hvac mode whenever the attribute holds
  // a usable value, matching the blueprint's precedence.
  const char *effective = rt.state;
  if (domain == SUB_DOMAIN_CLIMATE && rt.action[0] != '\0' && !sub_state_in(rt.action, SUB_UNUSABLE_STATES)) {
    effective = rt.action;
  }

  rt.last_state = evaluate_sub_state(domain, effective);
  const bool visible = sub_visible(rt.last_state, binding.inverted);

  ESP_LOGV(TAG, "%s.%s%s%s: '%s' => vis=%s", binding.page, binding.component, binding.attribute[0] != '\0' ? "." : "",
           binding.attribute, effective, YESNO(visible));

  if (sub_renderers[idx] != nullptr) {
    sub_renderers[idx](binding, rt, effective, visible);
  }
}

SubRenderFn sub_resolve_renderer(const char *page) {
  struct PageEntry {
    const char *page;
    SubRenderFn render;
  };
  static constexpr PageEntry PAGES[] = {
#ifdef NSPANEL_EASY_CHIPS
      {"chips", &chip_sub_render},
#endif  // NSPANEL_EASY_CHIPS
#ifdef NSPANEL_EASY_PAGE_HOME
      {"home", &home_sub_render},
#endif  // NSPANEL_EASY_PAGE_HOME
#ifdef NSPANEL_EASY_HW_DISPLAY
      {"wakeup", &wakeup_sensor_render},
#endif  // NSPANEL_EASY_HW_DISPLAY
#ifdef NSPANEL_EASY_HW_TEMPERATURE
      {"calibration", &temp_reference_render},
#endif  // NSPANEL_EASY_HW_TEMPERATURE
#ifdef NSPANEL_EASY_PAGE_BUTTONS
      {"buttonpage01", &buttons_sub_render},
      {"buttonpage02", &buttons_sub_render},
      {"buttonpage03", &buttons_sub_render},
      {"buttonpage04", &buttons_sub_render},
#endif  // NSPANEL_EASY_PAGE_BUTTONS
#ifdef NSPANEL_EASY_PAGE_CLIMATE
      {"climate", &climate_sub_render},  // Custom buttons
#endif                                   // NSPANEL_EASY_PAGE_CLIMATE
  };
  for (const PageEntry &entry : PAGES) {
    if (strcmp(page, entry.page) == 0) {
      return entry.render;
    }
  }
  return nullptr;
}

/**
 * @brief Reject targets that are driven locally rather than by Home Assistant.
 *
 * The relay and embedded-thermostat chips keep their own persisted appearance
 * and render without Home Assistant, so a subscription must never claim them.
 * Once those slots become user-assignable this becomes a check against whatever
 * currently owns the slot.
 *
 * @param page Target page.
 * @param component Target component.
 * @return true when the target may be bound.
 */
static bool sub_target_allowed(const char *page, const char *component) {
  if (strcmp(page, "chips") != 0) {
    return true;
  }
  static constexpr const char *LOCAL_CHIPS[] = {"chip_relay1", "chip_relay2", "chip_climate"};
  for (const char *reserved : LOCAL_CHIPS) {
    if (strcmp(component, reserved) == 0) {
      return false;
    }
  }
  return true;
}

uint16_t sub_load() {
  SubHeader header{};
  if (!sub_header_pref().load(&header)) {
    // Also the header of format 7 and older, which had another size: their chunks are erased here
    ESP_LOGD(TAG, "No persisted bindings");
    sub_erase_legacy_chunks();
    return 0;
  }
  if (header.version != SUB_FORMAT_VERSION) {
    ESP_LOGW(TAG, "Persisted bindings use format %" PRIu8 ", expected %" PRIu8 "; discarding", header.version,
             SUB_FORMAT_VERSION);
    sub_erase_legacy_chunks();
    sub_unverified = 0;
    return 0;
  }

  sub_unverified = header.unverified;

  if (header.count == 0 || header.count > SUB_MAX || header.length == 0 || header.length > SUB_PACK_MAX_BYTES) {
    ESP_LOGW(TAG, "Persisted bindings out of range (%" PRIu16 " binding(s), %" PRIu32 " bytes), ignoring", header.count,
             header.length);
    return 0;
  }

  const uint16_t chunks = static_cast<uint16_t>((header.length + SUB_PACK_CHUNK_BYTES - 1) / SUB_PACK_CHUNK_BYTES);
  auto *packed = static_cast<uint8_t *>(sub_alloc(static_cast<size_t>(chunks) * SUB_PACK_CHUNK_BYTES));
  sub_bindings = static_cast<SubBinding *>(sub_alloc(sizeof(SubBinding) * header.count));
  sub_runtime = static_cast<SubRuntime *>(sub_alloc(sizeof(SubRuntime) * header.count));
  sub_renderers = static_cast<SubRenderFn *>(sub_alloc(sizeof(SubRenderFn) * header.count));
  if (packed == nullptr || sub_bindings == nullptr || sub_runtime == nullptr || sub_renderers == nullptr) {
    ESP_LOGE(TAG, "Out of memory loading %" PRIu16 " bindings", header.count);
    free(packed);
    free(sub_bindings);
    free(sub_runtime);
    free(sub_renderers);
    sub_bindings = nullptr;
    sub_runtime = nullptr;
    sub_renderers = nullptr;
    return 0;
  }

  bool complete = true;
  for (uint16_t chunk = 0; chunk < chunks; ++chunk) {
    if (!sub_chunk_pref(chunk).load(reinterpret_cast<SubPackChunk *>(packed + chunk * SUB_PACK_CHUNK_BYTES))) {
      ESP_LOGW(TAG, "Chunk %" PRIu16 " of %" PRIu16 " missing", chunk, chunks);
      complete = false;
      break;
    }
  }  // for each chunk
  const uint32_t checksum = complete ? sub_checksum(SUB_CHECKSUM_INIT, packed, header.length) : 0;

  // The set is used complete or not at all: a missing chunk, chunks that do not match the header
  // (e.g. left from an older set after an interrupted write) or a damaged stream must never give a
  // mixed set (#374). The stream is only read after the checksum matched.
  const bool valid =
      complete && checksum == header.checksum && sub_unpack(packed, header.length, sub_bindings, header.count);
  free(packed);
  if (!valid) {
    ESP_LOGW(TAG,
             "Persisted bindings failed the integrity check (%" PRIu16 " binding(s), checksum %08" PRIX32
             ", expected %08" PRIX32 "); discarding them until the Blueprint sends them again",
             header.count, checksum, header.checksum);
    free(sub_bindings);
    free(sub_runtime);
    free(sub_renderers);
    sub_bindings = nullptr;
    sub_runtime = nullptr;
    sub_renderers = nullptr;
    sub_count = 0;
    // The count of unverified commits belonged to the discarded set. Kept, it could already be at its limit,
    // which would refuse the push that brings the bindings back when it has no end marker.
    sub_unverified = 0;
    return 0;
  }  // if integrity check failed
  sub_count = header.count;

  for (uint16_t idx = 0; idx < sub_count; ++idx) {
    // The domain is a pure function of the entity_id, so re-derive it rather
    // than trusting the persisted value: bindings stored by an older firmware
    // then pick up any domain reclassification without needing a new push.
    sub_bindings[idx].domain = static_cast<uint8_t>(parse_sub_domain(sub_bindings[idx].entity));
    sub_renderers[idx] = sub_resolve_renderer(sub_bindings[idx].page);
    if (sub_renderers[idx] == nullptr) {
      ESP_LOGW(TAG, "No renderer for page '%s'", sub_bindings[idx].page);
    }
  }  // for each verified binding

  ESP_LOGCONFIG(TAG, "Loaded %" PRIu16 " binding(s)", sub_count);
  return sub_count;
}

void sub_subscribe_all() {
#ifdef USE_API_HOMEASSISTANT_STATES
  if (sub_count == 0) {
    return;
  }
  const size_t heap_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);

  for (uint16_t idx = 0; idx < sub_count; ++idx) {
    SubBinding &binding = sub_bindings[idx];
    if (binding.entity[0] == '\0') {
      continue;
    }

    // A binding may name an attribute to read in place of the state; it lands
    // in rt.state and reaches the renderer through the usual path.
    const char *attribute = (binding.attribute[0] != '\0') ? binding.attribute : nullptr;

    ESP_LOGV(TAG, "Subscribing %s.%s to %s attr='%s'", binding.page, binding.component, binding.entity,
             attribute != nullptr ? attribute : "(state)");

    // The const char* overload stores the pointer without copying, so
    // binding.entity must outlive the subscription. It does: sub_bindings is
    // allocated once here and never resized, and a binding change restarts.
    api::global_api_server->subscribe_home_assistant_state(
        binding.entity, attribute, std::function<void(StringRef)>([idx](StringRef state) {
          sub_stats_count(idx);
          SubRuntime &rt = sub_runtime[idx];
          if (strncmp(rt.state, state.c_str(), sizeof(rt.state)) == 0) {
            return;  // Unchanged; nothing to recompute
          }
          ++sub_stats_changed;
          sub_copy(rt.state, sizeof(rt.state), state.c_str());
          sub_apply(idx);
        }));

    // Climate visibility follows hvac_action when available, so the attribute
    // needs a subscription of its own.
    if (binding.domain == SUB_DOMAIN_CLIMATE && attribute == nullptr) {
      api::global_api_server->subscribe_home_assistant_state(
          binding.entity, SUB_HVAC_ACTION, std::function<void(StringRef)>([idx](StringRef action) {
            sub_stats_count(idx);
            SubRuntime &rt = sub_runtime[idx];
            if (strncmp(rt.action, action.c_str(), sizeof(rt.action)) == 0) {
              return;  // Unchanged; nothing to recompute
            }
            ++sub_stats_changed;
            sub_copy(rt.action, sizeof(rt.action), action.c_str());
            sub_apply(idx);
          }));
    }
  }  // for each binding

  ESP_LOGI(TAG, "%" PRIu16 " binding(s) subscribed | heap cost=%d B", sub_count,
           static_cast<int>(heap_before - heap_caps_get_free_size(MALLOC_CAP_INTERNAL)));
#else   // USE_API_HOMEASSISTANT_STATES
  ESP_LOGE(TAG, "api: homeassistant_states is not enabled; no subscription is possible");
#endif  // USE_API_HOMEASSISTANT_STATES
}

void sub_push_binding(const char *push_id, const char *page, const char *component, const char *entity,
                      const char *device_class, const char *attribute, const char *icon_on, const char *icon_off,
                      uint16_t color_on, uint16_t color_off, bool inverted, const char *icon_name) {
  // An icon sent by name is resolved here, once, into the codepoint the renderers use
  char named_icon[4] = "";
  if ((icon_on == nullptr || icon_on[0] == '\0') && icon_name != nullptr && icon_name[0] != '\0') {
    if (sub_icon_lookup(icon_name, named_icon, sizeof(named_icon))) {
      icon_on = named_icon;
      icon_off = named_icon;
    } else {
      ESP_LOGW(TAG, "%s.%s: icon '%s' is not known", page, component, icon_name);
    }
  }  // if (icon by name)

  if (!sub_target_allowed(page, component)) {
    ESP_LOGW(TAG, "%s.%s is driven locally and cannot be bound", page, component);
    return;
  }

  // Overlapping blueprint runs (e.g. a configuration change while another run
  // is still pushing) interleave their calls. Only the newest push is staged:
  // a call from an older push, including a late one after a newer push ended,
  // is ignored, so bindings from different pushes are never committed together.
  if (sub_push_superseded(push_id)) {
    ESP_LOGD(TAG, "%s.%s from superseded push '%s' ignored", page, component, push_id);
    return;
  }
  if (sub_pushing && strcmp(push_id, sub_push_id) > 0) {
    ESP_LOGW(TAG, "Push '%s' superseded by '%s'; discarding %" PRIu16 " staged binding(s)", sub_push_id, push_id,
             sub_staged);
    memset(sub_staging, 0, sizeof(SubBinding) * SUB_MAX);
    sub_staged = 0;
    sub_dropped = 0;
    sub_copy(sub_push_id, sizeof(sub_push_id), push_id);
  }  // if newer push

  // The first binding after a completed or abandoned push starts a new one.
  if (!sub_pushing) {
    if (sub_staging == nullptr) {
      sub_staging = static_cast<SubBinding *>(sub_alloc(sizeof(SubBinding) * SUB_MAX));
      if (sub_staging == nullptr) {
        ESP_LOGE(TAG, "Out of memory staging bindings");
        return;
      }
    }
    memset(sub_staging, 0, sizeof(SubBinding) * SUB_MAX);
    sub_staged = 0;
    sub_dropped = 0;
    sub_pushing = true;
    sub_copy(sub_push_id, sizeof(sub_push_id), push_id);
    ESP_LOGD(TAG, "Binding push '%s' started", sub_push_id);
  }  // if no push in progress

  // Each run sends every target once, so a target that is already staged
  // means another run reused the same push_id (e.g. a caller sending a fixed
  // value). Restart staging with it: whatever was staged is now a mixture of
  // runs and must never be committed. The end markers of the interrupted runs
  // then fail the count check and are discarded.
  if (sub_find_target(sub_staging, sub_staged, page, component) < sub_staged) {
    ESP_LOGW(TAG, "%s.%s pushed twice; a new push started, discarding %" PRIu16 " staged binding(s)", page, component,
             sub_staged);
    memset(sub_staging, 0, sizeof(SubBinding) * SUB_MAX);
    sub_staged = 0;
    sub_dropped = 0;
  }  // if target already staged

  if (sub_staged >= SUB_MAX) {
    // Counted, so the end marker still matches and the bindings that fit are committed
    ++sub_dropped;
    ESP_LOGW(TAG, "More than %" PRIu16 " bindings pushed; %s.%s ('%s') ignored", SUB_MAX, page, component, entity);
    return;
  }

  SubBinding &staged = sub_staging[sub_staged];
  // A truncated entity_id cannot be bound: its prefix may even name another
  // existing entity, which would then drive this component. The entity is
  // cleared so sub_subscribe_all() skips the binding, but the slot is still
  // staged: dropping it would desync the count sub_push_end() validates, which
  // would discard the whole push instead of the one binding that cannot work.
  if (!sub_copy(staged.entity, sizeof(staged.entity), entity)) {
    staged.entity[0] = '\0';
    ESP_LOGW(TAG, "%s.%s: entity_id is %zu chars, over the %" PRIu8 " limit; '%s' will not update", page, component,
             strlen(entity), static_cast<uint8_t>(SUB_ENTITY_LEN - 1), entity);
    ESP_LOGW(TAG, "Raise api_subscribe_entity_len to at least %zu to bind it", strlen(entity) + 1);
  }
  sub_copy(staged.page, sizeof(staged.page), page);
  sub_copy(staged.component, sizeof(staged.component), component);
  sub_copy(staged.attribute, sizeof(staged.attribute), attribute);
  sub_copy(staged.device_class, sizeof(staged.device_class), device_class);
  staged.domain = static_cast<uint8_t>(parse_sub_domain(staged.entity));
  staged.inverted = inverted;
  ++sub_staged;

  // Appearance is not persisted, so apply it live to the matching live binding.
  // A component that is not yet bound picks it up after the restart.
  for (uint16_t idx = 0; idx < sub_count; ++idx) {
    SubBinding &live = sub_bindings[idx];
    if (strcmp(live.page, page) != 0 || strcmp(live.component, component) != 0) {
      continue;
    }

    // Neither of these affects the subscription, so both apply live rather
    // than waiting for the restart that an entity change would schedule.
    sub_copy(live.device_class, sizeof(live.device_class), device_class);
    live.inverted = inverted;

    // Appearance is not persisted, so it arrives with every push.
    SubRuntime &rt = sub_runtime[idx];
    sub_copy(rt.icon_on, sizeof(rt.icon_on), icon_on);
    sub_copy(rt.icon_off, sizeof(rt.icon_off), icon_off);
    rt.color_on = color_on;
    rt.color_off = color_off;
    rt.has_appearance = true;

    sub_apply(idx);
    break;
  }  // for each live binding
}

/**
 * @brief Look up an icon rule key.
 *
 * @param key Rule key.
 * @return The codepoint, or nullptr when there is no rule for the key.
 */
static const char *sub_icon_table_find(const char *key) {
  for (const IconRule &rule : ICON_RULES) {
    if (strcmp(rule.key, key) == 0) {
      return rule.icon;
    }
  }  // for each rule
  return nullptr;
}

const char *sub_icon_rule(const char *key) { return key != nullptr ? sub_icon_table_find(key) : nullptr; }

/**
 * @brief Look up an icon rule key built from parts.
 *
 * A key that does not fit SUB_ICON_KEY_LEN cannot be a rule, so it is not
 * looked up rather than looked up truncated.
 *
 * @param domain Entity domain.
 * @param device_class Device class; empty for a domain default by state; nullptr for the domain default.
 * @param state State suffix, or nullptr for none.
 * @return The codepoint, or nullptr when the key is not in the table.
 */
static const char *sub_icon_table_find(const char *domain, const char *device_class, const char *state) {
  char key[SUB_ICON_KEY_LEN];
  int len = 0;
  if (device_class == nullptr) {
    len = snprintf(key, sizeof(key), "%s", domain);
  } else if (state == nullptr) {
    len = snprintf(key, sizeof(key), "%s.%s", domain, device_class);
  } else if (device_class[0] == '\0') {
    len = snprintf(key, sizeof(key), "%s-%s", domain, state);
  } else {
    len = snprintf(key, sizeof(key), "%s.%s-%s", domain, device_class, state);
  }
  if (len < 0 || static_cast<size_t>(len) >= sizeof(key)) {
    return nullptr;  // Longer than any rule key
  }
  return sub_icon_table_find(key);
}

/**
 * @brief Icon of an entity from the icon rules: the device class icon for the state, then the domain default.
 *
 * @param entity_id Entity id, e.g. "binary_sensor.door".
 * @param device_class Device class; empty when unset.
 * @param state State string.
 * @param active Whether the state classifies as active, for the "on"/"off" fallback.
 * @return The codepoint, or nullptr when no rule matches.
 */
static const char *sub_rule_icon(const char *entity_id, const char *device_class, const char *state, bool active) {
  // Domain of the entity: everything before the dot
  const char *dot = strchr(entity_id, '.');
  if (dot == nullptr || static_cast<size_t>(dot - entity_id) >= SUB_ICON_KEY_LEN) {
    return nullptr;
  }
  char entity_domain[SUB_ICON_KEY_LEN];
  memcpy(entity_domain, entity_id, dot - entity_id);
  entity_domain[dot - entity_id] = '\0';

  const char *icon = nullptr;
  if (device_class != nullptr && device_class[0] != '\0') {
    // As the Blueprint keys them: no state for button, event and sensor; otherwise the
    // exact state first, then "on"/"off" by how the state classifies
    const bool stateless = strcmp(entity_domain, "button") == 0 || strcmp(entity_domain, "event") == 0 ||
                           strcmp(entity_domain, "sensor") == 0;
    if (stateless) {
      icon = sub_icon_table_find(entity_domain, device_class, nullptr);
    } else {
      icon = sub_icon_table_find(entity_domain, device_class, state);
      if (icon == nullptr) {
        icon = sub_icon_table_find(entity_domain, device_class, active ? "on" : "off");
      }
    }  // if (stateless)
  }  // if (device_class)
  if (icon == nullptr) {
    // Domain default, for the state first (e.g. "sun-below_horizon")
    icon = sub_icon_table_find(entity_domain, "", state);
    if (icon == nullptr) {
      icon = sub_icon_table_find(entity_domain, nullptr, nullptr);
    }
  }
  return icon;
}

SubAppearance resolve_binding_appearance(const SubBinding &binding, const char *state, uint16_t color_on,
                                         uint16_t color_off) {
  const SubDomain domain = static_cast<SubDomain>(binding.domain);
  SubAppearance look = resolve_sub_appearance(domain, binding.device_class, state, color_on, color_off);
  if (look.icon != nullptr) {
    return look;  // A multi-state domain
  }
  const bool active = (evaluate_sub_state(domain, state) == SUB_STATE_ON);
  look.color = active ? color_on : color_off;
  look.icon = sub_rule_icon(binding.entity, binding.device_class, state, active);
  return look;
}

std::string resolve_entity_icon(const char *entity_id, const char *device_class, const char *state,
                                const char *icon_override, const char *icon_attribute) {
  char glyph[4] = {};
  // 1. The icon set on the button that opened the page
  if (sub_icon_lookup(icon_override, glyph, sizeof(glyph))) {
    return glyph;
  }
  // 2. The domains whose icon follows the state, except covers (the entity's own icon wins over theirs)
  const SubDomain domain = parse_sub_domain(entity_id);
  const bool state_icon_first = domain == SUB_DOMAIN_ALARM || domain == SUB_DOMAIN_CLIMATE ||
                                domain == SUB_DOMAIN_LOCK || domain == SUB_DOMAIN_WATER_HEATER;
  if (state_icon_first) {
    const SubAppearance look = resolve_sub_appearance(domain, device_class, state, 0, 0);
    if (look.icon != nullptr) {
      return look.icon;
    }
  }  // if (state_icon_first)
  // 3. The entity's own icon
  if (sub_icon_lookup(icon_attribute, glyph, sizeof(glyph))) {
    return glyph;
  }
  // 4. Covers by device class and state, then the icon rules
  if (domain == SUB_DOMAIN_COVER) {
    const SubAppearance look = resolve_sub_appearance(domain, device_class, state, 0, 0);
    if (look.icon != nullptr) {
      return look.icon;
    }
  }  // if (cover)
  const char *icon = sub_rule_icon(entity_id, device_class, state, evaluate_sub_state(domain, state) == SUB_STATE_ON);
  return icon != nullptr ? icon : Icons::MDI_ALERT_CIRCLE;
}

/// @brief MDI names in codepoint order, as the Blueprint's all_icon_names: ",name0,name1,...,". The name at
///        position i (0-based) is U+E000 + i. Allocated on first use (PSRAM when available). Never persisted.
static char *sub_icon_names = nullptr;

/// @brief Characters used in sub_icon_names, null excluded.
static size_t sub_icon_names_len = 0;

/// @brief Names received in this session, so a chunk can be checked against its start index.
static uint16_t sub_icon_names_count = 0;

void sub_icon_names_add(uint16_t start, const std::string &names) {
  if (sub_icon_names == nullptr) {
    sub_icon_names = static_cast<char *>(sub_alloc(SUB_ICON_NAMES_CAPACITY));
    if (sub_icon_names == nullptr) {
      ESP_LOGW(TAG, "Out of memory for the icon names");
      return;
    }
  }  // if (sub_icon_names == nullptr)
  if (start == 0) {
    sub_icon_names[0] = ',';
    sub_icon_names[1] = '\0';
    sub_icon_names_len = 1;
    sub_icon_names_count = 0;
  } else if (start != sub_icon_names_count) {
    ESP_LOGW(TAG, "Icon names: chunk at %" PRIu16 " but %" PRIu16 " received; ignored", start, sub_icon_names_count);
    return;
  }
  if (names.empty()) {
    return;
  }
  if (sub_icon_names_len + names.size() + 2 > SUB_ICON_NAMES_CAPACITY) {
    ESP_LOGW(TAG, "Icon names: more than %" PRIu32 " bytes; the rest is not resolved", SUB_ICON_NAMES_CAPACITY);
    sub_icon_names_count = UINT16_MAX;  // Refuse the following chunks too
    return;
  }
  memcpy(sub_icon_names + sub_icon_names_len, names.data(), names.size());
  sub_icon_names_len += names.size();
  sub_icon_names[sub_icon_names_len++] = ',';
  sub_icon_names[sub_icon_names_len] = '\0';
  sub_icon_names_count = static_cast<uint16_t>(sub_icon_names_count + 1 + std::count(names.begin(), names.end(), ','));
  ESP_LOGV(TAG, "Icon names: %" PRIu16 " received", sub_icon_names_count);
}

bool sub_icon_lookup(const char *name, char *glyph, size_t size) {
  if (name == nullptr || glyph == nullptr || size < 4) {
    return false;
  }
  if (strncmp(name, "mdi:", 4) == 0) {
    name += 4;
  }
  const size_t name_len = strlen(name);
  if (name_len == 0 || name_len > 64) {
    return false;
  }

  // Names the Blueprint maps outside the font order (its special_icons)
  if (strcmp(name, "blank") == 0 || strcmp(name, "void") == 0) {
    return sub_copy(glyph, size, "\uFFFF");
  }
  if (strcmp(name, "unknown") == 0 || strcmp(name, "unavailable") == 0) {
    return sub_copy(glyph, size, Icons::MDI_ALERT_CIRCLE);
  }
  if (sub_icon_names == nullptr || sub_icon_names_len < 2) {
    return false;  // The list has not arrived yet
  }

  // ",name," marks the whole name; its position is the number of names before it
  char needle[68];
  snprintf(needle, sizeof(needle), ",%s,", name);
  const char *found = strstr(sub_icon_names, needle);
  if (found == nullptr) {
    return false;
  }
  const uint32_t index = static_cast<uint32_t>(std::count(static_cast<const char *>(sub_icon_names), found, ','));
  const uint32_t codepoint = 0xE000 + index;
  if (codepoint > 0xFFFF) {
    return false;
  }
  // UTF-8, always three bytes in this range
  glyph[0] = static_cast<char>(0xE0 | (codepoint >> 12));
  glyph[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
  glyph[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
  glyph[3] = '\0';
  return true;
}

void sub_set_icon(const char *page, const char *component, const char *icon) {
  const uint16_t idx = sub_find_target(sub_bindings, sub_count, page, component);
  if (idx >= sub_count) {
    ESP_LOGD(TAG, "%s.%s is not bound; icon ignored", page, component);
    return;
  }
  SubRuntime &rt = sub_runtime[idx];
  if (!sub_copy(rt.icon_on, sizeof(rt.icon_on), icon)) {
    ESP_LOGW(TAG, "%s.%s: icon does not fit; ignored", page, component);
    sub_copy(rt.icon_on, sizeof(rt.icon_on), rt.icon_off);  // Keep the previous icon
    return;
  }
  sub_copy(rt.icon_off, sizeof(rt.icon_off), icon);
  sub_apply(idx);
}

bool sub_persist(bool verified) {
  // Three-phase write. Chunk keys are reused, so a failure partway through the
  // loop would leave some chunks new and some old while the header still
  // described the old set -- a reboot would then load a mixture. Publishing an
  // empty header first means any later failure leaves the panel with no
  // bindings: visibly wrong, but never silently inconsistent.
  //
  // save() only queues a write, and sync() writes every queued item on its own:
  // one item can fail while the others are written. So each phase is flushed
  // before the next one starts, or the real header could reach NVS while a
  // chunk did not (#374). The result of sync() covers every queued preference,
  // also those of other components, so each phase is confirmed by reading back
  // what was stored.
  const SubHeader invalid{SUB_FORMAT_VERSION, sub_unverified, 0, 0, 0};
  sub_header_pref().save(&invalid);
  global_preferences->sync();
  if (!sub_header_stored(invalid)) {
    if (sub_persist_failed(sub_staged)) {
      return false;  // The preferences were erased and the panel is restarting
    }  // if (erased)
    ESP_LOGE(TAG, "Could not invalidate the stored header; leaving the saved set untouched");
    return false;  // The header was not written, so the previous set is still coherent
  }

  // The staged bindings, packed: only the characters used, in chunks of SUB_PACK_CHUNK_BYTES
  const size_t capacity = static_cast<size_t>(sub_staged) * SUB_PACK_BINDING_MAX + SUB_PACK_CHUNK_BYTES;
  auto *packed = static_cast<uint8_t *>(sub_alloc(capacity));
  if (packed == nullptr) {
    ESP_LOGE(TAG, "Out of memory packing %" PRIu16 " bindings", sub_staged);
    return false;
  }  // if (no memory)
  memset(packed, 0, capacity);  // Pads the last chunk with zeros
  const uint32_t length = sub_pack(sub_staging, sub_staged, packed);
  const uint16_t chunks = static_cast<uint16_t>((length + SUB_PACK_CHUNK_BYTES - 1) / SUB_PACK_CHUNK_BYTES);
  const auto chunk_at = [packed](uint16_t chunk) {
    return reinterpret_cast<const SubPackChunk *>(packed + chunk * SUB_PACK_CHUNK_BYTES);
  };
  bool ok = false;

  // Written twice at most: a first sync writes what fits, which can release the
  // space that the rewrite of an existing key needs before its old copy can be
  // dropped. Chunks already written are then found unchanged and skipped.
  for (uint8_t attempt = 0; attempt < 2 && !ok; ++attempt) {
    for (uint16_t chunk = 0; chunk < chunks; ++chunk) {
      sub_chunk_pref(chunk).save(chunk_at(chunk));
    }  // for each chunk
    // Every chunk must reach NVS before the header describes them
    global_preferences->sync();
    ok = true;
    for (uint16_t chunk = 0; chunk < chunks && ok; ++chunk) {
      ok = sub_chunk_stored(chunk, *chunk_at(chunk));
    }  // for each chunk read back
    if (!ok && attempt == 0) {
      ESP_LOGW(TAG, "The binding chunks failed to save; retrying once");
    }
  }  // for each attempt

  const uint32_t checksum = sub_checksum(SUB_CHECKSUM_INIT, packed, length);
  free(packed);

  uint8_t committed_unverified = sub_unverified;
  if (ok) {
    // The header stays empty until here, so a partial set is never loaded
    committed_unverified = verified ? 0 : static_cast<uint8_t>(sub_unverified + 1);
    const SubHeader header{SUB_FORMAT_VERSION, committed_unverified, sub_staged, length, checksum};
    sub_header_pref().save(&header);
    global_preferences->sync();
    ok = sub_header_stored(header);
  }

  if (ok) {
    // Chunks of a longer set stored before are not described by the header anymore: free their space
    sub_erase_keys(fnv1_hash("nspanel_easy_sub_pack"), chunks, SUB_PACK_CHUNK_COUNT);
  }

  if (ok) {
    // Only once the header has actually reached NVS: a counter advanced for a
    // commit that never landed would eventually trip SUB_MAX_UNVERIFIED_COMMITS
    // and start refusing valid pushes.
    sub_unverified = committed_unverified;
  }

  if (ok) {
    sub_wipe_marker = 0;  // The bindings fit: a later failure may erase the preferences again
  } else {
    if (sub_persist_failed(sub_staged)) {
      return false;  // The preferences were erased and the panel is restarting
    }  // if (erased)
    // Never schedule a restart here. A restart on a write that cannot succeed
    // is an unrecoverable boot loop. The panel keeps running the previously
    // loaded bindings instead, until its next reboot: wrong, but stable and
    // diagnosable.
    ESP_LOGE(TAG,
             "Could not persist %" PRIu16 " binding(s); the stored set is now empty and the panel will "
             "subscribe to nothing after the next reboot. This usually means the NVS partition is full or "
             "fragmented; see \"Could not persist\" in docs/api_subscribe.md.",
             sub_staged);
    return false;
  }

  ESP_LOGI(TAG, "Persisted %" PRIu16 " binding(s) in %" PRIu16 " chunk(s)%s", sub_staged, chunks,
           verified ? "" : " (unverified)");
  return true;
}

/**
 * @brief Compare the staged set against the live one.
 *
 * The comparison is by target, not by position: the same bindings received in
 * a different order are the same set and must not trigger a restart. Targets
 * are unique within each set (see sub_push_binding()), so equal counts plus a
 * match for every staged target means both sets hold the same bindings.
 *
 * @return true when the sets differ and a restart is required.
 */
static bool sub_staged_differs() {
  if (sub_staged != sub_count) {
    return true;
  }
  for (uint16_t idx = 0; idx < sub_staged; ++idx) {
    const SubBinding &staged = sub_staging[idx];
    const uint16_t live = sub_find_target(sub_bindings, sub_count, staged.page, staged.component);
    if (live >= sub_count) {
      ESP_LOGV(TAG, "Binding added: '%s' %s.%s", staged.entity, staged.page, staged.component);
      return true;
    }
    if (!sub_binding_equal(staged, sub_bindings[live])) {
      ESP_LOGV(TAG, "Binding changed: '%s' %s.%s -> '%s' %s.%s", sub_bindings[live].entity, sub_bindings[live].page,
               sub_bindings[live].component, staged.entity, staged.page, staged.component);
      return true;
    }
  }  // for each staged binding
  return false;
}

/**
 * @brief Release the staging buffer and end the current push.
 */
static void sub_push_release() {
  free(sub_staging);
  sub_staging = nullptr;
  sub_staged = 0;
  sub_dropped = 0;
  sub_pushing = false;
  sub_copy(sub_last_push_id, sizeof(sub_last_push_id), sub_push_id);
  sub_last_push_end_ms = App.get_loop_component_start_time();
}

bool sub_push_end(const char *push_id, uint16_t count) {
  // Match the end marker to its push, see sub_push_binding().
  if (sub_push_superseded(push_id)) {
    ESP_LOGD(TAG, "End marker of superseded push '%s' ignored", push_id);
    return false;
  }
  if (sub_pushing && strcmp(push_id, sub_push_id) > 0) {
    // A newer push sent its end marker before any binding reached the panel,
    // so it can only be an empty push. It replaces the one being staged.
    ESP_LOGW(TAG, "Push '%s' superseded by '%s'; discarding %" PRIu16 " staged binding(s)", sub_push_id, push_id,
             sub_staged);
    sub_push_release();
  }  // if newer push
  sub_copy(sub_last_push_id, sizeof(sub_last_push_id), push_id);
  sub_last_push_end_ms = App.get_loop_component_start_time();

  // A push containing no bindings never opens, because sub_push_binding() is
  // what starts one. The blueprint still sends the end marker when every
  // component has been unconfigured, so treat that as an explicit empty set --
  // otherwise the persisted bindings survive and the panel keeps subscribing to
  // entities the user has removed.
  if (!sub_pushing && count == 0) {
    sub_staged = 0;
    if (!sub_staged_differs()) {
      ESP_LOGD(TAG, "Bindings unchanged");
      sub_unverified = 0;
      return false;
    }
    ESP_LOGI(TAG, "All bindings cleared");
    return sub_persist(true);
  }  // if empty push

  if (!sub_pushing) {
    ESP_LOGW(TAG, "End marker received with no push in progress");
    return false;
  }

  if (count != sub_staged + sub_dropped) {
    ESP_LOGE(TAG, "Push incomplete: %" PRIu16 " of %" PRIu16 " binding(s) received, discarding",
             static_cast<uint16_t>(sub_staged + sub_dropped), count);
    sub_push_release();
    return false;
  }

  if (!sub_staged_differs()) {
    ESP_LOGD(TAG, "Bindings unchanged");
    sub_unverified = 0;
    sub_push_release();
    return false;
  }

  if (sub_dropped > 0) {
    ESP_LOGW(TAG, "%" PRIu16 " binding(s) over the limit of %" PRIu16 " were not bound; raise api_subscribe_max",
             sub_dropped, SUB_MAX);
  }
  const bool persisted = sub_persist(true);
  sub_push_release();
  return persisted;
}

bool sub_push_active() { return sub_pushing; }

bool sub_push_timeout() {
  if (!sub_pushing) {
    return false;
  }

  if (sub_unverified >= SUB_MAX_UNVERIFIED_COMMITS) {
    ESP_LOGE(TAG,
             "No end marker after %" PRIu8 " consecutive pushes; refusing to commit. "
             "The blueprint is not sending the end marker, or the push is being truncated.",
             sub_unverified);
    sub_push_release();
    return false;
  }

  if (!sub_staged_differs()) {
    ESP_LOGW(TAG, "No end marker, but bindings are unchanged");
    sub_push_release();
    return false;
  }

  ESP_LOGW(TAG, "No end marker; committing %" PRIu16 " binding(s) unverified", sub_staged);
  const bool persisted = sub_persist(false);
  sub_push_release();
  return persisted;
}

void sub_render_all() {
  for (uint16_t idx = 0; idx < sub_count; ++idx) {
    sub_apply(idx);
  }
}

void sub_render_page(const char *page) {
  for (uint16_t idx = 0; idx < sub_count; ++idx) {
    if (strcmp(sub_bindings[idx].page, page) == 0) {
      sub_apply(idx);
    }  // if (binding on the page)
  }  // for each binding
}

void sub_log_stats(uint32_t window_s) {
  if (sub_stats_received == 0) {
    return;  // Nothing arrived; stay quiet
  }

  // Busiest bindings first: a partial selection, as only a few are named
  uint16_t top[SUB_STATS_TOP] = {};
  uint8_t top_count = 0;
  for (uint16_t idx = 0; idx < sub_count; ++idx) {
    const uint16_t received = sub_runtime[idx].received;
    if (received == 0) {
      continue;
    }
    uint8_t pos = top_count;
    while (pos > 0 && sub_runtime[top[pos - 1]].received < received) {
      if (pos < SUB_STATS_TOP) {
        top[pos] = top[pos - 1];
      }
      --pos;
    }
    if (pos < SUB_STATS_TOP) {
      top[pos] = idx;
      if (top_count < SUB_STATS_TOP) {
        ++top_count;
      }
    }
  }  // for each binding

  char busiest[256] = {};
  size_t used = 0;
  for (uint8_t pos = 0; pos < top_count && used < sizeof(busiest); ++pos) {
    const SubBinding &binding = sub_bindings[top[pos]];
    const int written =
        snprintf(busiest + used, sizeof(busiest) - used, "%s%s%s%s (%" PRIu16 ")", pos > 0 ? ", " : "", binding.entity,
                 binding.attribute[0] != '\0' ? "." : "", binding.attribute, sub_runtime[top[pos]].received);
    if (written < 0) {
      break;
    }
    used += static_cast<size_t>(written);
  }  // for each of the busiest

  ESP_LOGI(TAG, "Updates in the last %" PRIu32 " s: %" PRIu32 " received, %" PRIu32 " changed; busiest: %s", window_s,
           sub_stats_received, sub_stats_changed, busiest);

  sub_stats_received = 0;
  sub_stats_changed = 0;
  for (uint16_t idx = 0; idx < sub_count; ++idx) {
    sub_runtime[idx].received = 0;
  }
}

void sub_dump_config() {
  ESP_LOGCONFIG(TAG, "Subscriptions");
  ESP_LOGCONFIG(TAG, "  Bindings: %" PRIu16 " of %" PRIu16, sub_count, SUB_MAX);
  ESP_LOGCONFIG(TAG, "  Entity length: %" PRIu8 " chars", static_cast<uint8_t>(SUB_ENTITY_LEN - 1));
  if (sub_unverified > 0) {
    ESP_LOGCONFIG(TAG, "  Unverified commits: %" PRIu8, sub_unverified);
  }
  for (uint16_t idx = 0; idx < sub_count; ++idx) {
    const SubBinding &binding = sub_bindings[idx];
    char attribute[SUB_ATTR_LEN + 3] = {};  // " (" + name + ")" + null
    if (binding.attribute[0] != '\0') {
      snprintf(attribute, sizeof(attribute), " (%s)", binding.attribute);
    }
    ESP_LOGCONFIG(TAG, "  %s.%s <- %s%s%s%s", binding.page, binding.component, binding.entity, attribute,
                  binding.inverted ? " (inverted)" : "", sub_renderers[idx] == nullptr ? " [no renderer]" : "");
  }
  nvs_stats_t nvs{};
  if (nvs_get_stats(nullptr, &nvs) == ESP_OK) {
    ESP_LOGCONFIG(TAG, "  NVS entries: %zu used, %zu free of %zu", nvs.used_entries, nvs.free_entries,
                  nvs.total_entries);
  }
  if (SUB_WIPE_NVS_ON_FAILURE) {
    ESP_LOGCONFIG(TAG, "  Erase the preferences when the bindings cannot be stored: YES");
#ifndef USE_API_NOISE
    // Any client of an API without encryption can push bindings, so it could also cause the erase
    ESP_LOGW(TAG, "  api_subscribe_wipe_nvs_on_failure is enabled without API encryption");
#endif  // USE_API_NOISE
  }  // if (SUB_WIPE_NVS_ON_FAILURE)
}

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_SUBSCRIBE
