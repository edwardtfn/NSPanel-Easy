# Add-on: ESPectre motion detection

## Description

This add-on enables the use of your panel's Wi-Fi traffic to act as a motion sensor using the
ESPectre module - see [ESPectre.dev](https://espectre.dev) for the details of how the module works and for license conditions.

If the display is blank, and motion is detected, the display wakes up.

For best results you will need to make adjustments in _Home Assistant > Settings > Devices > ESPhome_ for each panel and calibrate with no-one in the room.

### Attention

1. The NSPanel is capable of running this addon in addition to everything else it is doing.
   Be kind to it and keep an eye on the image size when building. If you have too many addons, you might need to choose.
   It is particularly asking a lot to use Bluetooth at the same time as WiFi motion detection as they use the same airspace as well as the same NSPanel.
   The tweaks documented below make some necessary compromises to allow both to work together, but the combination is not recommended. 
2. The ESPectre addon makes use of the display item normally reserved for RELAY 2 and lights it up during motion detection.
   If _you are also using the second relay_, expect the unexpected 😃

## Installation

Add the reference to `addon_espectre` in your ESPHome settings in the `package` section after the `remote_package` (base code), as shown below:

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

## Image size

Testing with `ESPhome 2026.9.1`, `ESPectre 3.0.0` and `NSPanel-Easy 2026.10.0`

### ESPectre and climate_backup

```yaml
substitutions:
  wakeup_with_button_press: true
  backup_heater_relay: "1"
  backup_cooler_relay: "2"
  climate_backup_delay: "3"
captive_portal: !remove
web_server: !remove
api:
  encryption:
    key: !secret api_encryption
ota:
  platform: esphome
  encryption:
  allow_partition_access: true
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
Image size: 1413351 bytes,
RAM: 40.5%,
Flash: 77.0%

### ESPectre

```yaml
substitutions:
  wakeup_with_button_press: true
captive_portal: !remove
web_server: !remove
api:
  encryption:
    key: !secret api_encryption
ota:
  platform: esphome
  encryption:
  allow_partition_access: true
packages:
  remote_package:
    url: https://github.com/edwardtfn/NSPanel-Easy
    files:
      - nspanel_esphome.yaml # Base package
      - esphome/nspanel_esphome_addon_display_light.yaml
      - esphome/nspanel_esphome_addon_espectre.yaml
```
Image size: 1391455 bytes,
RAM: 39.9%,
Flash: 75.8%

### ESPectre with Bluetooth Proxy and Climate Backup

```yaml
substitutions:
  wakeup_with_button_press: true
  backup_heater_relay: "1"
  backup_cooler_relay: "2"
  climate_backup_delay: "3"
captive_portal: !remove
web_server: !remove
api:
  encryption:
    key: !secret api_encryption
ota:
  platform: esphome
  encryption:
  allow_partition_access: true
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
espectre:
  segmentation_window_size_ms: 1000
  evaluation_interval_ms: 250
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
Image size: 1813615 bytes,
RAM: 77.2%,
Flash: 98.8%

> [!NOTE]
> As both the ESPectre code and the base package gain new features over time, Image size is likely to grow and eventually make ESPectre/Bluetooth Proxy coexistence impossible. 

## Configuration

The following keys are available to be used in your `substitutions`. All values must be delimited with `""`:

<!-- markdownlint-disable MD013 MD033 -->
| Key | Required | Supported values | Default | Description |
| :- | :-: | :-: | :-: | :- |
| espectre_ref | no | valid ESPectre release tag | "3.0.0"  | ESPectre version control |
| espectre_direct_api | no | "false" or "true" | "true" | the direct API is used by the ESPectre App |
| espectre_detection_algorithm | no | "high_accuracy" or "lightweight" | "high_accuracy" | see ESPectre docs |
| espectre_traffic_generator_mode | no | "wifi_raw", "internal", "external" | "wifi_raw" | "external" if using ESPectre App as traffic generator |
<!-- markdownlint-enable MD013 MD033 -->

## Blueprint settings

In General Settings make sure that `relay 2` has a visible colour and icon - `cyan` and `mdi:motion-sensor` for example 

## ESPectre Manager app for Home Assistant

Head over to [ESPectre.dev](https://espectre.dev) for details.
Getting the balance right between traffic modes may take a bit of work.

> [!NOTE]
> If you have more than a few panels or other devices running ESPectre with default settings you will run the risk of bringing down your home wireless or even your whole home network.
> Install **ESPectre Manager app for Home Assistant** and use it to make necessary adjustments, setting traffic mode to External to use its multicast traffic generator for example.
> Properly configured, there is no problem using a dozen or more panels in combination with **ESPectre Manager**.

## Bluetooth coexistence 

This combination has been tested successfully, but in the end it was worth adding a dedicated Bluetooth device nearer the washing machine 

```yaml 
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

espectre:
  segmentation_window_size_ms: 1000
  evaluation_interval_ms: 250
```

