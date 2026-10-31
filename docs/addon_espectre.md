# Add-on: ESPectre motion detection

## Description

This add-on enables the use of your panel's Wi-Fi traffic to act as a motion sensor using the
ESPectre module. See [ESPectre.dev](https://espectre.dev) for details on how the module works and for its license conditions.

If the display is blank, and motion is detected, the display wakes up.

For best results you will need to make adjustments in _Home Assistant > Settings > Devices > ESPHome_ for each panel and calibrate with no-one in the room.

### Attention

1. The NSPanel is capable of running this add-on in addition to everything else it is doing.
   Be kind to it and keep an eye on the image size when building. If you have too many add-ons, you might need to choose.
   It is particularly demanding to use Bluetooth at the same time as Wi-Fi motion detection, as they share the same radio and airtime.
   The tweaks documented below make some necessary compromises to allow both to work together, but the combination is not recommended.
2. This add-on repurposes the status icon (chip) normally used for **relay 2**: it shows motion instead of the relay state.
   The relay itself keeps working, but its state is no longer shown on the display.
   Consider this before combining it with add-ons that use relay 2, such as [Cover](addon_cover.md) or climate backup relay on relay 2 [Climate](addon_climate.md).

## Installation

Add the reference to `addon_espectre` in your ESPHome settings in the `packages` section after the `remote_package` (base code), as shown below:

```yaml
substitutions:
  # Settings - Editable values
  device_name: "YOUR_NSPANEL_NAME"
  friendly_name: "Your panel's friendly name"
  wifi_ssid: !secret wifi_ssid
  wifi_password: !secret wifi_password
  language: en      # Language code - see docs/localization.md for all supported codes

##### My customization - Start #####
##### My customization - End #####

# Basic and optional configurations
packages:
  remote_package:
    url: https://github.com/edwardtfn/NSPanel-Easy
    ref: latest
    refresh: 300s
    files:
      - nspanel_esphome.yaml # Basic package
      # Optional add-on configurations
      - esphome/nspanel_esphome_addon_espectre.yaml
```

## Configuration

The following keys are available to be used in your `substitutions`. All values must be delimited with `""`:

<!-- markdownlint-disable MD013 MD033 -->
| Key | Required | Supported values | Default | Description |
| :- | :-: | :-: | :-: | :- |
| espectre_ref | no | valid ESPectre release tag | "3.0.0" | ESPectre version control |
| espectre_direct_api | no | "false" or "true" | "true" | the direct API is used by the ESPectre App |
| espectre_detection_algorithm | no | "high_accuracy" or "lightweight" | "high_accuracy" | see ESPectre docs |
| espectre_traffic_generator_mode | no | "ping", "dns", "dns_tcp", "wifi_raw" or "external" | "wifi_raw" | "external" if using ESPectre App as traffic generator |
<!-- markdownlint-enable MD013 MD033 -->

## Blueprint settings

In General Settings make sure that `relay 2` has a visible colour and icon - `cyan` and `mdi:motion-sensor` for example.

## ESPectre Manager app for Home Assistant

Head over to [ESPectre.dev](https://espectre.dev) for details.
Getting the balance right between traffic modes may take a bit of work.

> [!NOTE]
> If you have more than a few panels or other devices running ESPectre with default settings you will want to
> install **ESPectre Manager app for Home Assistant** and use it to make necessary adjustments, setting traffic mode to External to use its multicast traffic generator for example.
> Properly configured, there is no problem using a dozen or more panels in combination with **ESPectre Manager**.

## Bluetooth coexistence

This combination has been tested successfully, but for reliable long-term use a dedicated Bluetooth proxy device is recommended.

This tweak shares airtime without Bluetooth taking too long for its scans and disrupting ESPectre.

```yaml
esp32:
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_ESP_COEX_SW_COEXIST_ENABLE: n
      # With Wi-Fi enabled, software_coexistence: false only stops ESPHome from requesting software coexistence,
      # ESP-IDF still enables it by default when Wi-Fi and Bluetooth are both in use, so this sdkconfig option is needed.

esp32_ble_tracker:
  software_coexistence: false
  scan_parameters:
    interval: 100ms
    window: 5ms
```

## Image size

Tested with `ESPHome 2026.9.1`, `ESPectre 3.0.0` and `NSPanel-Easy 2026.10.0`.

### Summary

<!-- markdownlint-disable MD013 MD033 -->
| Addon(s) | Image | RAM | Flash | Impact |
| :- | :-: | :-: | :-: | :- |
| base package | | | | |
| + ESPectre | 1391455 | 39.9% | 75.8% | easily within capacity on both RAM and Flash |
| + ESPectre + climate + climate_backup | 1413351 | 40.5% | 77.0% | minimal increase to RAM or Flash |
| + ESPectre + climate + climate_backup + Bluetooth Proxy | 1813615 | 77.2% | 98.8% | RAM is still OK but very little remaining margin on Flash |
<!-- markdownlint-enable MD013 MD033 -->

> [!NOTE]
> As both the ESPectre code and the base package gain new features over time, the image size is likely to grow and eventually make ESPectre/Bluetooth Proxy coexistence impossible.

### ESPectre

```yaml
substitutions:
  wakeup_with_button_press: true
packages:
  remote_package:
    url: https://github.com/edwardtfn/NSPanel-Easy
    files:
      - nspanel_esphome.yaml # Base package
      - esphome/nspanel_esphome_addon_display_light.yaml
      - esphome/nspanel_esphome_addon_espectre.yaml
```

Image size: 1391455 bytes, RAM: 39.9%, Flash: 75.8%

### ESPectre and climate_backup

```yaml
substitutions:
  wakeup_with_button_press: true
  backup_heater_relay: "1"
  backup_cooler_relay: "0"  # Relay 2 status icon is used by ESPectre
  climate_backup_delay: "3"
packages:
  remote_package:
    url: https://github.com/edwardtfn/NSPanel-Easy
    files:
      - nspanel_esphome.yaml # Base package
      - esphome/nspanel_esphome_addon_display_light.yaml
      - esphome/nspanel_esphome_addon_climate_backup.yaml
      - esphome/nspanel_esphome_addon_climate_dual.yaml
      - esphome/nspanel_esphome_addon_espectre.yaml
```

Image size: 1413351 bytes, RAM: 40.5%, Flash: 77.0%

### ESPectre with Bluetooth Proxy and Climate Backup

```yaml
substitutions:
  wakeup_with_button_press: true
  backup_heater_relay: "1"
  backup_cooler_relay: "0"  # Relay 2 status icon is used by ESPectre
  climate_backup_delay: "3"
esp32:
  framework:
    type: esp-idf
    sdkconfig_options:
      CONFIG_ESP_COEX_SW_COEXIST_ENABLE: n
      # With Wi-Fi enabled, software_coexistence: false only stops ESPHome from requesting software coexistence, and
      # ESP-IDF still enables it by default when Wi-Fi and Bluetooth are both in use, so this sdkconfig option is needed.
esp32_ble_tracker:
  software_coexistence: false
  scan_parameters:
    interval: 100ms
    window: 5ms
packages:
  remote_package:
    url: https://github.com/edwardtfn/NSPanel-Easy
    files:
      - nspanel_esphome.yaml # Base package
      - esphome/nspanel_esphome_addon_display_light.yaml
      - esphome/nspanel_esphome_addon_climate_backup.yaml
      - esphome/nspanel_esphome_addon_climate_heat.yaml
      - esphome/nspanel_esphome_addon_espectre.yaml
      - esphome/nspanel_esphome_addon_bluetooth_proxy.yaml
```

Image size: 1813615 bytes, RAM: 77.2%, Flash: 98.8%
