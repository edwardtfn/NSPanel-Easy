// chips.cpp

#ifdef NSPANEL_EASY_CHIPS

#include "chips.h"
#include "nextion_components.h"

#include "esphome/components/nextion/nextion.h"

#include <string>

#ifdef NSPANEL_EASY_SUBSCRIBE
#include "base.h"

#include "esphome/core/log.h"
#endif  // NSPANEL_EASY_SUBSCRIBE

namespace esphome::nspanel_easy {

static const char *const TAG = "nspanel.chips";

ChipState chip_states[CHIP_COUNT] = {};
bool chips_mirror_page = false;

void chip_render(uint8_t idx) {
  if (idx >= CHIP_COUNT || nextion_display == nullptr) {
    return;
  }

  const ChipState &the_chip = chip_states[idx];
  const char *text = the_chip.visible ? the_chip.icon : "";

  nextion_display->set_component_text(CHIP_NAMES_HOME[idx], text);
  if (the_chip.visible) {
    nextion_display->set_component_font_color(CHIP_NAMES_HOME[idx], the_chip.color);
  }

  if (chips_mirror_page) {
    nextion_display->set_component_text(CHIP_NAMES[idx], text);
    if (the_chip.visible) {
      nextion_display->set_component_font_color(CHIP_NAMES[idx], the_chip.color);
    }
  }
}

#ifdef NSPANEL_EASY_SUBSCRIBE

void chip_sub_render(const SubBinding &binding, const SubRuntime &rt, const char *state, bool visible) {
  const uint8_t idx = find_chip_index(binding.component);
  if (idx == UINT8_MAX) {
    ESP_LOGW(TAG, "'%s' is not a chip component", binding.component);
    return;
  }

  if (nextion_display == nullptr) {
    return;  // Boot has not handed the display over yet
  }

  ChipState &the_chip = chip_states[idx];
  the_chip.subscribed = true;  // Blocks blueprint pushes for this slot

  // Nothing to draw until the blueprint has pushed appearance for this session.
  // Appearance is not persisted, so a chip stays hidden after a boot until then.
  if (!rt.has_appearance) {
    return;
  }

  // A chip reports what is happening now, so a stale state is not shown at all.
  const bool shown = visible && !api_stale;
  bool changed = false;

  if (shown) {
    const bool active = (rt.last_state == SUB_STATE_ON);
    const char *icon = active ? rt.icon_on : rt.icon_off;
    uint16_t color = active ? rt.color_on : rt.color_off;

    // An icon pushed by the blueprint wins; otherwise resolve from the domain
    // and state, which is what the multi-state domains rely on.
    if (icon[0] == '\0') {
      const SubAppearance look = resolve_sub_appearance(static_cast<SubDomain>(binding.domain), binding.device_class,
                                                        state, rt.color_on, rt.color_off);
      if (look.icon != nullptr) {
        icon = look.icon;
        color = look.color;
      }
    }

    if (icon[0] == '\0') {
      ESP_LOGW(TAG, "%s has no icon, keeping it hidden", binding.component);
      return;
    }

    if (strcmp(the_chip.icon, icon) != 0) {
      strncpy(the_chip.icon, icon, sizeof(the_chip.icon) - 1);
      the_chip.icon[sizeof(the_chip.icon) - 1] = '\0';
      changed = true;
    }
    if (the_chip.color != color) {
      the_chip.color = color;
      changed = true;
    }
  }  // if (shown) — appearance is left untouched while hidden

  if (the_chip.visible != shown) {
    the_chip.visible = shown;
    changed = true;
  }

  if (changed) {
    chip_render(idx);
  }
}

#endif  // NSPANEL_EASY_SUBSCRIBE

}  // namespace esphome::nspanel_easy

#endif  // NSPANEL_EASY_CHIPS
