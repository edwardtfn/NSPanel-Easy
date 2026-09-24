// api_subscriptions.cpp

#ifdef NSPANEL_EASY_SUBSCRIBE

#include "api_subscriptions.h"
#include "base.h"
#include "nextion_components.h"

#include <cinttypes>

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

namespace esphome::nspanel_easy {

static const char *const TAG = "nspanel.api.sub";

SubBinding *sub_bindings = nullptr;
SubRuntime *sub_runtime = nullptr;
SubRenderFn *sub_renderers = nullptr;
uint16_t sub_count = 0;

/**
 * @brief Header persisted alongside the binding chunks.
 *
 * version and entity_len together form the persistence contract. The chunk
 * blobs are sized from SUB_ENTITY_LEN, so a build whose entity length differs
 * from the stored one cannot read them; checking here turns that into one
 * explicit discard-and-repush instead of a run of "chunk missing" warnings.
 */
struct SubHeader {
  uint8_t version;     ///< SUB_FORMAT_VERSION at the time of writing
  uint8_t entity_len;  ///< SUB_ENTITY_LEN at the time of writing
  uint16_t count;      ///< Number of persisted bindings
  uint8_t unverified;  ///< Consecutive commits that never received an end marker
};

/// @brief Staging buffer, allocated for the duration of a push only.
static SubBinding *sub_staging = nullptr;

/// @brief Bindings received in the push currently in progress.
static uint16_t sub_staged = 0;

/// @brief Whether a push is currently in progress.
static bool sub_pushing = false;

/// @brief Consecutive commits that never received an end marker.
static uint8_t sub_unverified = 0;

/**
 * @brief Allocate, preferring PSRAM and falling back to internal DRAM.
 *
 * Not every NSPanel carries PSRAM, so the fallback is mandatory rather than a
 * convenience. Allocations here are made once and never resized, which is what
 * lets the API hold raw pointers into the binding array.
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

/**
 * @brief Preference handle for one binding chunk.
 *
 * Chunking keeps the compare-before-write copy in ESP32Preferences::is_changed()
 *
 * @param chunk Chunk index.
 * @return Preference object for SubChunk.
 */
static ESPPreferenceObject sub_chunk_pref(uint16_t chunk) {
  return global_preferences->make_preference<SubChunk>(fnv1_hash("nspanel_easy_sub_chunk") + chunk);
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
    ESP_LOGD(TAG, "No persisted bindings");
    return 0;
  }
  if (header.version != SUB_FORMAT_VERSION) {
    ESP_LOGW(TAG, "Persisted bindings use format %" PRIu8 ", expected %" PRIu8 "; discarding", header.version,
             SUB_FORMAT_VERSION);
    sub_unverified = 0;
    return 0;
  }
  if (header.entity_len != SUB_ENTITY_LEN) {
    // The chunk blobs are sized from SUB_ENTITY_LEN and cannot be reinterpreted.
    ESP_LOGW(TAG, "Persisted bindings use entity length %" PRIu8 ", expected %" PRIu8 "; discarding", header.entity_len,
             static_cast<uint8_t>(SUB_ENTITY_LEN));
    sub_unverified = 0;
    return 0;
  }

  sub_unverified = header.unverified;

  if (header.count == 0 || header.count > SUB_MAX) {
    ESP_LOGW(TAG, "Persisted binding count out of range (%" PRIu16 "), ignoring", header.count);
    return 0;
  }

  sub_bindings = static_cast<SubBinding *>(sub_alloc(sizeof(SubBinding) * header.count));
  sub_runtime = static_cast<SubRuntime *>(sub_alloc(sizeof(SubRuntime) * header.count));
  sub_renderers = static_cast<SubRenderFn *>(sub_alloc(sizeof(SubRenderFn) * header.count));
  if (sub_bindings == nullptr || sub_runtime == nullptr || sub_renderers == nullptr) {
    ESP_LOGE(TAG, "Out of memory loading %" PRIu16 " bindings", header.count);
    free(sub_bindings);
    free(sub_runtime);
    free(sub_renderers);
    sub_bindings = nullptr;
    sub_runtime = nullptr;
    sub_renderers = nullptr;
    return 0;
  }

  const uint16_t chunks = (header.count + SUB_CHUNK_SIZE - 1) / SUB_CHUNK_SIZE;
  for (uint16_t chunk = 0; chunk < chunks; ++chunk) {
    SubChunk data{};
    if (!sub_chunk_pref(chunk).load(&data)) {
      ESP_LOGW(TAG, "Chunk %" PRIu16 " missing; loaded %" PRIu16 " binding(s)", chunk, sub_count);
      break;
    }
    for (uint16_t slot = 0; slot < SUB_CHUNK_SIZE && sub_count < header.count; ++slot) {
      sub_bindings[sub_count] = data.bindings[slot];
      sub_renderers[sub_count] = sub_resolve_renderer(sub_bindings[sub_count].page);
      if (sub_renderers[sub_count] == nullptr) {
        ESP_LOGW(TAG, "No renderer for page '%s'", sub_bindings[sub_count].page);
      }
      ++sub_count;
    }
  }  // for each chunk

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
          SubRuntime &rt = sub_runtime[idx];
          if (strncmp(rt.state, state.c_str(), sizeof(rt.state)) == 0) {
            return;  // Unchanged; nothing to recompute
          }
          sub_copy(rt.state, sizeof(rt.state), state.c_str());
          sub_apply(idx);
        }));

    // Climate visibility follows hvac_action when available, so the attribute
    // needs a subscription of its own.
    if (binding.domain == SUB_DOMAIN_CLIMATE && attribute == nullptr) {
      api::global_api_server->subscribe_home_assistant_state(
          binding.entity, SUB_HVAC_ACTION, std::function<void(StringRef)>([idx](StringRef action) {
            SubRuntime &rt = sub_runtime[idx];
            if (strncmp(rt.action, action.c_str(), sizeof(rt.action)) == 0) {
              return;  // Unchanged; nothing to recompute
            }
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

void sub_push_binding(const char *page, const char *component, const char *entity, const char *device_class,
                      const char *attribute, const char *icon_on, const char *icon_off, uint16_t color_on,
                      uint16_t color_off, bool inverted) {
  if (!sub_target_allowed(page, component)) {
    ESP_LOGW(TAG, "%s.%s is driven locally and cannot be bound", page, component);
    return;
  }

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
    sub_pushing = true;
    ESP_LOGD(TAG, "Binding push started");
  }

  if (sub_staged >= SUB_MAX) {
    ESP_LOGW(TAG, "More than %" PRIu16 " bindings pushed; '%s' ignored", SUB_MAX, entity);
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

bool sub_persist(bool verified) {
  // Three-phase write. Chunk keys are reused, so a failure partway through the
  // loop would leave some chunks new and some old while the header still
  // described the old set -- a reboot would then load a mixture. Publishing an
  // empty header first means any later failure leaves the panel with no
  // bindings: visibly wrong, but never silently inconsistent.
  const SubHeader invalid{SUB_FORMAT_VERSION, SUB_ENTITY_LEN, 0, sub_unverified};
  if (!sub_header_pref().save(&invalid) || !global_preferences->sync()) {
    ESP_LOGE(TAG, "Could not invalidate the stored header; leaving the saved set untouched");
    return false;  // Nothing was written, so the previous set is still coherent
  }

  const uint16_t chunks = (sub_staged + SUB_CHUNK_SIZE - 1) / SUB_CHUNK_SIZE;
  bool ok = true;

  for (uint16_t chunk = 0; chunk < chunks; ++chunk) {
    SubChunk data{};
    for (uint16_t slot = 0; slot < SUB_CHUNK_SIZE; ++slot) {
      const uint16_t idx = (chunk * SUB_CHUNK_SIZE) + slot;
      if (idx < sub_staged) {
        data.bindings[slot] = sub_staging[idx];
      }
    }
    if (!sub_chunk_pref(chunk).save(&data)) {
      ESP_LOGW(TAG, "Chunk %" PRIu16 " failed to save; retrying after a sync", chunk);
      // A sync flushes pending writes, which can release space that a rewrite
      // of an existing key needs before the old copy can be dropped.
      global_preferences->sync();
      if (!sub_chunk_pref(chunk).save(&data)) {
        ok = false;
        break;  // The header stays empty, so the partial set is never loaded
      }
    }
  }  // for each chunk

  uint8_t committed_unverified = sub_unverified;
  if (ok) {
    committed_unverified = verified ? 0 : static_cast<uint8_t>(sub_unverified + 1);
    const SubHeader header{SUB_FORMAT_VERSION, SUB_ENTITY_LEN, sub_staged, committed_unverified};
    ok = sub_header_pref().save(&header);
  }

  if (!global_preferences->sync()) {
    ok = false;
  } else if (ok) {
    // Only once the header has actually reached NVS: a counter advanced for a
    // commit that never landed would eventually trip SUB_MAX_UNVERIFIED_COMMITS
    // and start refusing valid pushes.
    sub_unverified = committed_unverified;
  }

  if (!ok) {
    // Never schedule a restart here. A restart on a write that cannot succeed
    // is an unrecoverable boot loop. The panel keeps the previously loaded
    // bindings instead: wrong, but stable and diagnosable.
    ESP_LOGE(TAG,
             "Could not persist %" PRIu16 " binding(s); the stored set is now empty and the panel will "
             "subscribe to nothing after the next reboot. This usually means the NVS partition is full or "
             "fragmented; a factory reset of the panel clears it.",
             sub_staged);
    return false;
  }

  ESP_LOGI(TAG, "Persisted %" PRIu16 " binding(s)%s", sub_staged, verified ? "" : " (unverified)");
  return true;
}

/**
 * @brief Compare the staged set against the live one.
 *
 * @return true when the sets differ and a restart is required.
 */
static bool sub_staged_differs() {
  if (sub_staged != sub_count) {
    return true;
  }
  for (uint16_t idx = 0; idx < sub_staged; ++idx) {
    if (!sub_binding_equal(sub_staging[idx], sub_bindings[idx])) {
      ESP_LOGV(TAG, "Binding %" PRIu16 " changed: '%s' %s.%s -> '%s' %s.%s", idx, sub_bindings[idx].entity,
               sub_bindings[idx].page, sub_bindings[idx].component, sub_staging[idx].entity, sub_staging[idx].page,
               sub_staging[idx].component);
      return true;
    }
  }
  return false;
}

/**
 * @brief Release the staging buffer and end the current push.
 */
static void sub_push_release() {
  free(sub_staging);
  sub_staging = nullptr;
  sub_staged = 0;
  sub_pushing = false;
}

bool sub_push_end(uint16_t count) {
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

  if (count != sub_staged) {
    ESP_LOGE(TAG, "Push incomplete: %" PRIu16 " of %" PRIu16 " binding(s) received, discarding", sub_staged, count);
    sub_push_release();
    return false;
  }

  if (!sub_staged_differs()) {
    ESP_LOGD(TAG, "Bindings unchanged");
    sub_unverified = 0;
    sub_push_release();
    return false;
  }

  const bool persisted = sub_persist(true);
  sub_push_release();
  return persisted;
}

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
}

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_SUBSCRIBE
