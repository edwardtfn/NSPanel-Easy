---
description: Code-free customization of the Sonoff NSPanel with ESPHome and Home Assistant.
hide:
  - navigation
  - toc
---

<!-- markdownlint-disable MD033 MD041 -->
<div class="pe-hero" markdown>

<div class="pe-hero-text" markdown>

# Your Sonoff NSPanel, configured from Home Assistant

NSPanel Easy turns the Sonoff NSPanel into a fully local wall panel for [Home Assistant](https://www.home-assistant.io/),
built on [ESPHome](https://esphome.io/).
Every page, button and value is set up in a Blueprint, with no code to write.

[Get started](install.md){ .md-button .md-button--primary }
[Migrate from Blackymas](migration_from_blackymas.md){ .md-button }

</div>

<div class="pe-hero-image" markdown>

![NSPanel Easy home page](pics/eu_home.png)

</div>

</div>

## Why NSPanel Easy

<div class="grid cards" markdown>

- :material-cursor-default-click-outline: **No code**

    ---

    Choose entities, icons and colours in a Home Assistant Blueprint.
    The panel follows your changes without recompiling.

- :material-lan-disconnect: **Fully local**

    ---

    No cloud and no internet needed after setup.
    The relays and the embedded thermostat keep working even without Wi-Fi.

- :material-puzzle-outline: **Modular**

    ---

    Add only what you need: a local thermostat, cover control, Bluetooth proxy, and more,
    as [add-ons](addon_climate.md).

</div>

## Everything on one panel

<div class="grid cards" markdown>

- :material-home-outline: **Home and screensaver**

    ---

    Clock, weather, indoor temperature, up to four values and ten status chips.

- :material-gesture-tap-button: **Buttons and entities**

    ---

    Up to 32 buttons and 32 entities, across four pages each, with states shown live.

- :material-lightbulb-on-outline: **Lights, covers and fans**

    ---

    Brightness, colour temperature, colour wheel, effects, positions, tilt and speeds.

- :material-thermostat: **Climate**

    ---

    Target temperature, HVAC modes, presets and fan modes, for any Home Assistant climate.

- :material-speaker: **Media and alarm**

    ---

    Playback, volume and track details; arming modes with a PIN keypad.

- :material-weather-partly-cloudy: **Weather and energy**

    ---

    Five-day forecast and a utilities dashboard with energy flows.

</div>

## Get started in three steps

<div class="pe-steps" markdown>

1. **Flash the firmware.**
   Add NSPanel Easy to ESPHome and install it on the panel.
   See [Installation](install.md).
2. **Upload the display (TFT).**
   The firmware downloads the right display file for your model.
   See [Upload TFT](addon_upload_tft.md).
3. **Import the Blueprint.**
   Create an automation from it and pick your entities.
   See [Blueprint](blueprint.md).

</div>

[![Import the NSPanel Easy Blueprint into Home Assistant](https://my.home-assistant.io/badges/blueprint_import.svg)](https://my.home-assistant.io/redirect/blueprint_import/?blueprint_url=https%3A%2F%2Fraw.githubusercontent.com%2Fedwardtfn%2FNSPanel-Easy%2Frefs%2Ftags%2Flatest%2Fnspanel_easy_blueprint.yaml)

!!! tip "Coming from Blackymas/NSPanel_HA_Blueprint?"
    Migrating takes less than 10 minutes, with no serial flashing and no reconfiguration.
    Follow the [migration guide](migration_from_blackymas.md).

## What you need

- A **Sonoff NSPanel**, EU or US model (the original, not the NSPanel Pro). See [where to buy](where_to_buy.md).
- **Home Assistant** with **ESPHome**.
  See the [version compatibility](version_compatibility.md).

The panel is [translated](localization.md) into many languages, with metric and imperial units.

## Community and support

<div class="grid cards" markdown>

- :fontawesome-brands-github: **Issues and ideas**

    ---

    Report a problem or suggest a feature on [GitHub](https://github.com/edwardtfn/NSPanel-Easy/issues).

- :fontawesome-brands-discord: **Discord**

    ---

    Chat with other users on the [NSPanel Easy Discord](https://discord.gg/KyVPd33znv).

- :material-forum-outline: **Forum**

    ---

    Follow the [Home Assistant Community thread](https://community.home-assistant.io/t/nspanel-easy-blueprint-configured-nspanel-customization/1005033).

</div>

If NSPanel Easy makes your home a little smarter, consider [buying me an ice cream](https://www.buymeacoffee.com/edwardfirmo).
<!-- markdownlint-enable MD033 MD041 -->
