# Add-on: Cover

## Description

This add-on enables your panel's relays to act as a cover driver,
using its relays to drive the motor to open or close a cover.

- This is an experimental feature, so be prepared to face some issues.
  Please report those so we can improve.
- In its initial version, this control is not fully local,
  and Home Assistant still needs to process the open/close commands.
  We want to change this in the future so it could be fully controlled locally.

> [!WARNING]
> The NSPanel is limited to 2A per relay.
> Don't use it to directly power your cover's motor if it exceeds the panel specifications:
>
> - 150W/110V/Gang, 300W/110V/Total
> - 300W/220V/Gang, 600W/220V/Total

<!-- blockquote separator - to avoid markdown lint 028 error -->

> [!NOTE]
> More details on the [Sonoff NSPanel's page](https://sonoff.tech/product/central-control-panel/nspanel/),
> which has the product specifications in its "Specs & User Manual" section.

## Installation

You will need to add the reference to the `addon_cover` file in your ESPHome
settings in the `package` section and after the `remote_package` (base code),
as shown below:

```yaml
substitutions:
  # Settings - Editable values
  device_name: "YOUR_NSPANEL_NAME"
  friendly_name: "Your panel's friendly name"
  wifi_ssid: !secret wifi_ssid
  wifi_password: !secret wifi_password

  # Add-on configuration (if needed)
  ## Upload TFT
  upload_tft_automatically: true

  ## Cover
  cover_device_class: curtain  # default `""` (none), any of https://www.home-assistant.io/integrations/cover/#device-class
  interlock_wait_time: '250'

# Customization area
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
      # Optional advanced and add-on configurations
      # - esphome/nspanel_esphome_addon_climate_cool.yaml
      # - esphome/nspanel_esphome_addon_climate_heat.yaml
      # - esphome/nspanel_esphome_addon_climate_dual.yaml
      - esphome/nspanel_esphome_addon_cover.yaml
      # - esphome/nspanel_esphome_addon_display_light.yaml  # Show the display as a light in Home Assistant
```

> [!NOTE]
> Occasionally, ESPHome updates may result in the `entity_id` of embedded covers
> being appended with `_2`. If you experience this change, refer to this [forum
> post](https://community.home-assistant.io/t/esphome-devices-all-renamed-with-2-added/388146)
> on the Home Assistant Forum for guidance.

## Configuration

### Substitutions

The following keys are available in your `substitutions` section:

<!-- markdownlint-disable MD013 MD033 -->
| Key | Required | Supported values | Default | Description |
| :- | :-: | :-: | :-: | :- |
| `cover_device_class` | Optional | Any of the [device classes supported by Home Assistant](https://www.home-assistant.io/integrations/cover/#device-class) | `""` (none) | Sets the cover type at compile time. Influences how the entity is represented in Home Assistant. |
| `interlock_wait_time` | Optional | `1` to `5000` | `250` | Relay interlock delay in ms. Imposes a time delay from one relay turning off until the other can turn on, preventing both relays from being on simultaneously.<br>***ATTENTION***: this is a software interlock — see [ESPHome Switch Interlocking](https://esphome.io/components/switch/gpio.html#interlocking). |
<!-- markdownlint-enable MD013 MD033 -->

### UI entities

The remaining settings are accessible via the device's page in Home Assistant
(**Settings** > **Devices & services** > **ESPHome**) and do not require YAML changes:

<!-- markdownlint-disable MD013 MD033 -->
| Entity | Supported values | Default | Description |
| :- | :-: | :-: | :- |
| Cover relays mode | `Relay 1 opens, relay 2 closes` or `Relay 2 opens, relay 1 closes` | `Relay 1 opens, relay 2 closes` | Defines which relay opens the cover and which closes it. |
| Cover open duration | `1ms` to `600000ms` | `1ms` (disabled) | The amount of time it takes the cover to open from the fully closed state. |
| Cover close duration | `1ms` to `600000ms` | `1ms` (disabled) | The amount of time it takes the cover to close from the fully open state. |
| Cover acceleration wait time | `0ms` to `10000ms` | `0ms` | Considers the wait time needed by the cover to start moving after a command is issued, accounting for large inertia. |
| Cover direction change wait time | `0ms` to `10000ms` | `100ms` | Stops the cover and forces a wait time between direction changes, protecting motors. If set, an intermediate stop action will be invoked if an open/close action is issued while moving in the opposite direction. |
| Cover interlock wait time | `1ms` to `5000ms` | `250ms` | Runtime override of the interlock delay set by the `interlock_wait_time` substitution. See note above about software interlock. |
<!-- markdownlint-enable MD013 MD033 -->

> [!IMPORTANT]
> **Migrating from a previous version of this project?**
> The *Cover device class* setting was previously a UI selector on the device's page in Home Assistant.
> It is now set at compile time via the `cover_device_class` substitution.
> If you had previously selected a device class in the UI, add the matching value to your `substitutions`
> and reflash your device.

## Advanced settings

This add-on is based on the [ESPHome Feedback Cover](https://esphome.io/components/cover/feedback.html)
component and supports any of the settings available for that component.
All parameters documented there are available via `!extend cover_embedded`.
If you want to set a specific attribute, use the `!extend` option in your panel's YAML, like the following:

```yaml
cover:
  - id: !extend cover_embedded
    max_duration: 60s
```
