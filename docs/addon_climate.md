# Add-on: Climate

## Description

This add-on enables the use of your panel's relays to act as a thermostat (either cooler or heater)
using the internal temperature sensor and independent of the network availability.

For slow systems such as water underfloor heating, a PI/PWM control is available as an alternative to the on/off control,
see [PID mode](#pid-mode-pipwm-control-for-slow-systems).

### Attention

1. The NSPanel is limited to 2A per relay.
Don't use it to directly power your cooler/heater if it exceeds the panel specifications:
    - 150W/110V/Gang, 300W/110V/Total
    - 300W/220V/Gang, 600W/220V/Total
2. A target temperature must be set on the climate entity in Home Assistant or the page Climate in your panel.

> [!NOTE]
> More details on the [Sonoff NSPanel's page](https://sonoff.tech/product/central-control-panel/nspanel/)
> and the [product specifications document](https://sonoff.tech/wp-content/uploads/2021/11/%E4%BA%A7%E5%93%81%E5%8F%82%E6%95%B0%E8%A1%A8-NSPanel-20210831.pdf).

## Installation

You will need to add the reference to `addon_climate_heat`, `addon_climate_cool` or `addon_climate_dual`
(or one of the [PID packages](#pid-mode-pipwm-control-for-slow-systems)) files on your ESPHome settings in the `package` section
and after the `remote_package` (base code), as shown below (for `heat` in this example):

> [!NOTE]
> Occasionally, ESPHome updates may result in the `entity_id` of embedded thermostats being appended with `_2`.
> If you experience this change, refer to this [forum post](https://community.home-assistant.io/t/esphome-devices-all-renamed-with-2-added/388146) on the Home Assistant Forum for guidance.
>
> The Blueprint recognizes the embedded thermostat by its `entity_id` ending in `_thermostat`.
> If its `entity_id` no longer ends in `_thermostat` (for example after renaming or localizing it, or with the `_2` suffix above),
> the Blueprint handles it as an external climate entity, which is slower and does not work while the panel is offline ([#640](https://github.com/edwardtfn/NSPanel-Easy/issues/640)).

```yaml
substitutions:
  # Settings - Editable values
  device_name: "YOUR_NSPANEL_NAME"
  friendly_name: "Your panel's friendly name"
  wifi_ssid: !secret wifi_ssid
  wifi_password: !secret wifi_password
  ota_password: ""  # Optional: set OTA password, or use ${wifi_password} for backward compatibility (see migration guide)
  language: en      # Language code - see docs/localization.md for all supported codes

  # Add-on configuration (if needed)
  ## Upload TFT
  upload_tft_automatically: true

  ## Add-on climate
  heater_relay: "1"  # Possible values: "1" or "2"

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
      - esphome/nspanel_esphome_addon_climate_heat.yaml
      # - esphome/nspanel_esphome_addon_climate_dual.yaml
      # - esphome/nspanel_esphome_addon_climate_pid_single.yaml  # PI/PWM control, see PID mode below
      # - esphome/nspanel_esphome_addon_climate_pid_dual.yaml    # PI/PWM control, see PID mode below
      # - esphome/nspanel_esphome_addon_cover.yaml
      # - esphome/nspanel_esphome_addon_display_light.yaml  # Show the display as a light in Home Assistant

```

## Configuration

The following keys are available to be used in your `substitutions`:

<!-- markdownlint-disable MD013 MD033 -->
| Key | Required | Supported values | Default | Description |
| :- | :-: | :-: | :-: | :- |
| cooler_relay | Mandatory for *cool* and *dual* | `1` or `2` | `0` (disabled) | Relay used to control the cooler. Use `1` for "Relay 1" or `2` for "Relay 2". |
| heater_relay | Mandatory for *heat* and *dual* | `1` or `2` | `0` (disabled) | Relay used to control the heater. Use `1` for "Relay 1" or `2` for "Relay 2". |
| temp_units | Optional | `°C` or `°F` | `°C` | Temperature unit used by everything the panel renders locally, not just this add-on. It is defined in the core package, so setting it here also changes the embedded temperature sensor and the Blueprint must be set to the same unit. See [Localization](localization.md#temperature-unit). |
| min_off_time | Optional | Positive integer representing the number of seconds | `300` | Minimum duration (in seconds) the cooling/heating action must be disengaged before it may be engaged. |
| min_run_time | Optional | Positive integer representing the number of seconds | `300` | Minimum duration (in seconds) the cooling/heating action must be engaged before it may be disengaged. |
| min_idle_time | Optional | Positive integer representing the number of seconds | `30` | Minimum duration (in seconds) the idle action must be active before calling another climate action. |
| target_low | Optional | Number representing a temperature in the selected unit | `18` (°C) or `64` (°F) | The initial lower threshold for the target temperature. |
| target_high | Optional | Number representing a temperature in the selected unit | `24` (°C) or `75` (°F) | The initial higher threshold for the target temperature. |
| temp_min | Optional | Number representing a temperature in the selected unit | *cool: `15` (°C) or `60` (°F)*<br>*heat: `7` (°C) or `45` (°F)*<br>*dual: `7` (°C) or `45` (°F)* | The minimum temperature the climate device can reach. Used to set the range of the frontend gauge. |
| temp_max | Optional | Number representing a temperature in the selected unit | *cool: `45` (°C) or `113` (°F)*<br>*heat: `35` (°C) or `95` (°F)*<br>*dual: `45` (°C) or `113` (°F)* | The maximum temperature the climate device can reach. Used to set the range of the frontend gauge. |
| cool_deadband | Optional | Number representing a temperature hysteresis in the selected unit | `0.5` (°C) or `1` (°F) | The minimum temperature differential (temperature above the set point) before engaging cooling. |
| cool_overrun | Optional | Number representing a temperature hysteresis in the selected unit | `0.5` (°C) or `1` (°F) | The minimum temperature differential (cooling beyond the set point) before disengaging cooling. |
| heat_deadband | Optional | Number representing a temperature hysteresis in the selected unit | `0.5` (°C) or `1` (°F) | The minimum temperature differential (temperature below the set point) before engaging heat. |
| heat_overrun | Optional | Number representing a temperature hysteresis in the selected unit | `0.5` (°C) or `1` (°F) | The minimum temperature differential (heating beyond the set point) before disengaging heat. |

<!-- markdownlint-enable MD013 MD033 -->

- All values must be delimited with `""`
- For more details on the keys, see [ESPHome Base Climate Configurations](https://esphome.io/components/climate/index.html#base-climate-configuration)
  and [ESPHome Climate Thermostat - Additional actions behavior](https://esphome.io/components/climate/thermostat.html#additional-actions-behavior).

### Target temperature step

The target temperature is set in steps of `0.5` with `temp_units: °C`, or `1` with `temp_units: °F`.
For a different step, e.g. `0.1` °C, extend the thermostat in your ESPHome settings, at the same level as `substitutions:`:

```yaml
climate:
  - id: !extend thermostat_embedded
    visual:
      temperature_step:
        target_temperature: 0.1
        current_temperature: 0.1  # Same precision, so Home Assistant shows the target with decimals
```

Write the step in the unit of `temp_units`, without a unit after the number (e.g. `0.5` for half a degree on a °F panel), and as a multiple of `0.1`.
The climate page and Home Assistant then use the same step, as long as Home Assistant uses the same unit as `temp_units` (see the note below).
Both round the step to one decimal, but not the same way, so a step such as `0.25` becomes 0.3 on the climate page and 0.2 in Home Assistant.
Home Assistant shows temperatures with the precision of `current_temperature` (whole degrees from `1`, half degrees from `0.5`, tenths below).
Keep `current_temperature: 0.1` when the target step is not a multiple of `0.5`, so a target such as 20.2 is not shown rounded.
With a unit, newer ESPHome versions convert a step written in °F into °C, so on a °F panel `1 °F` becomes about 0.6, on the climate page and in Home Assistant.

> [!NOTE]
> Home Assistant applies the step in its own temperature unit, without converting it.
> If Home Assistant uses a different unit than `temp_units`, it moves the target temperature in steps of the same
> number in its own unit (e.g. steps of 1 °C for a °F panel), while the climate page keeps the configured step.
> This is a Home Assistant limitation, reported in [home-assistant/core#190241](https://github.com/home-assistant/core/issues/190241).

## Temperature source

By default, the thermostat uses the panel's own temperature sensor, including the temperature correction set under the panel's device page in Home Assistant.
That sensor reads high because the panel heats itself up.
Select a [calibration reference](blueprint.md#nspanel-temperature-sensor---calibration-reference-optional) in the Blueprint to correct it automatically.
The thermostat keeps regulating on the panel's own sensor, so it keeps working fully on its own while Home Assistant is unavailable.

If the reference is placed elsewhere in the room and you want the thermostat to follow it,
enable **Climate entity** > **Embedded Thermostat - Regulate on the Calibration Reference** in the Blueprint.
The thermostat then regulates on the reference directly, and the panel's sensor is still calibrated against it, so it can take over with little difference.
The thermostat falls back to the calibrated panel sensor when:

- the reference is `unavailable`, `unknown` or has no number,
- Home Assistant has been unavailable for longer than `api_stale_grace_period` (30 seconds by default),
- the reference has not changed for 2 hours (`temp_reference_max_age`), as some sensors keep reporting their last value long after they stopped working, or
- the reference differs from the panel's sensor by more than 10 °C (18 °F), which usually means it reports in a different unit. A warning is logged.

It switches back to the reference as soon as it can be trusted again.

> [!NOTE]
> The reference must report in the same unit as `temp_units`. The panel's own sensor and thermostat cannot be selected.
> Changing these settings restarts the panel once, as with any other [API subscription](api_subscribe.md).

## Modes

The climate page shows only the HVAC modes supported by the thermostat, based on its configuration.
With the default configuration, the `heat` add-on offers `Heat` and `Off`, the `cool` add-on offers `Cool` and `Off`, and the `dual` add-on offers `Heat/Cool`, `Heat`, `Cool` and `Off`.
If you customize `thermostat_embedded` to support more modes (e.g. `auto_mode`, `dry_action` or `fan_only_action`), the matching buttons are shown automatically.

## Presets

The thermostat presets are available on the climate page through the preset button, which opens a list with the available presets.
ESPHome's standard presets (`Home`, `Away`, `Boost`, `Comfort`, `Eco`, `Sleep`, `Activity`) come first, followed by your custom presets.
This list is built by the panel itself, so presets can be changed even while Home Assistant is unavailable.
The preset button shows the icon of the active standard preset and is highlighted while a preset is active. Custom presets use the generic preset icon.

By default, the add-on defines the presets `Off` and `Home`. See [Climate custom presets](customization.md#climate-custom-presets) to define your own.

## Backup mode (local control only when Home Assistant is unavailable)

The optional `addon_climate_backup` package lets Home Assistant (or any other system) be the primary climate controller,
while the embedded thermostat takes over one or two relays only when Home Assistant is unavailable.

The embedded thermostat keeps running all the time on its normal relays (typically `heater_relay: "0"`, a dummy relay),
so it has no effect while Home Assistant is connected.
When Home Assistant has been unavailable for longer than `climate_backup_delay`, the backup relays start to follow the embedded thermostat.
When Home Assistant is back, the backup relays are turned off and control returns to Home Assistant.

Home Assistant counts as connected when its ESPHome integration is connected to the panel and receiving its states, as it normally is.
Other connections to the panel's API that do not receive states (for example, viewing the logs) do not count:
with only those, Home Assistant is considered unavailable.

While Home Assistant is connected, the panel does not switch the backup relays on its own:
Home Assistant must switch them, e.g. with its own thermostat or an automation.
If you want the panel's thermostat to switch the relay all the time, with or without Home Assistant, you don't need this package.
Select the relay in `heater_relay` (or `cooler_relay`) instead, and Home Assistant can still change the target temperature and the mode:

```yaml
substitutions:
  ## Add-on climate
  heater_relay: "1"  # The embedded thermostat switches relay 1, with or without Home Assistant
```

> [!IMPORTANT]
> This package requires one of the climate add-ons (`heat`, `cool` or `dual`) to be included as well; otherwise ESPHome reports `Source for extension of ID 'relay_0' was not found`.
> The backup relays must be different from each other and from `heater_relay` / `cooler_relay`; this is checked at build time.
> The embedded thermostat must be left in an active mode (e.g. preset `Home`), otherwise there is nothing to mirror during an outage.
> The backup relay should be dedicated to this purpose. While the backup is active, any external change to it (e.g. from the hardware buttons) is reverted to match the embedded thermostat.
> A reboot restarts the `climate_backup_delay` countdown. Without Home Assistant, the panel reboots after `api: reboot_timeout` (`60min` in this project),
> and without Wi-Fi after `wifi: reboot_timeout` (`15min` by default in ESPHome). If longer outages are expected, consider increasing these (or setting them to `0s`) in your local yaml.

<!-- markdownlint-disable MD013 -->
| Key | Required | Supported values | Default | Description |
| :- | :-: | :-: | :-: | :- |
| backup_heater_relay | Optional | `0`, `1` or `2` | `0` (disabled) | Relay driven by the heat action while Home Assistant is unavailable. |
| backup_cooler_relay | Optional | `0`, `1` or `2` | `0` (disabled) | Relay driven by the cool action while Home Assistant is unavailable. |
| climate_backup_delay | Optional | Positive integer representing the number of seconds | `300` | Time without Home Assistant before the backup takes over. This also covers the time after a reboot. |

<!-- markdownlint-enable MD013 -->

```yaml
substitutions:
  ## Add-on climate
  heater_relay: "0"         # The embedded thermostat drives a dummy relay while Home Assistant is available
  backup_heater_relay: "2"  # Relay 2 is driven only when Home Assistant is unavailable

packages:
  remote_package:
    url: https://github.com/edwardtfn/NSPanel-Easy
    ref: latest
    refresh: 300s
    files:
      - nspanel_esphome.yaml # Basic package
      - esphome/nspanel_esphome_addon_climate_heat.yaml
      - esphome/nspanel_esphome_addon_climate_backup.yaml
```

## PID mode (PI/PWM control for slow systems)

The optional PID packages replace the two-point (on/off with hysteresis) control of the climate add-ons
with a PI controller and slow PWM output.
This suits slow systems with a high thermal mass, such as water underfloor heating:
instead of opening the valve fully until the target is reached (and overshooting while the screed keeps releasing heat),
the controller opens it for a share of each PWM cycle that matches the actual heat demand.

Two variants are available:

<!-- markdownlint-disable MD013 -->
| Package | Setpoints | Heat/cool selection | Relays | Display UI |
| :- | :-: | :- | :- | :-: |
| `nspanel_esphome_addon_climate_pid_single.yaml` | One | Central, from a Home Assistant entity (e.g. switched together with the heat pump) | One shared relay (`valve_relay`) for heating and cooling | Heat |
| `nspanel_esphome_addon_climate_pid_dual.yaml` | Two (low/high) | On the panel, through the thermostat mode (`heat`, `cool`, `heat_cool`) | Separate relays (`heater_relay`, `cooler_relay`) | Dual |

<!-- markdownlint-enable MD013 -->

How it works:

- The embedded thermostat (`thermostat_embedded`) stays in place as the user interface and setpoint holder,
  so the climate page, the climate chip and the Blueprint work as usual. It only switches the dummy relay `relay_0`.
- Two internal PID controllers (heat and cool) do the actual control and are synced with the thermostat every `pid_sync_interval` seconds.
- The controller output (0-100 %) is turned into on/off cycles of the panel relay by a `slow_pwm` output.
- The controllers regulate on the same temperature as the thermostat, so they follow the calibration reference as described in [Temperature source](#temperature-source).
- The regulation runs entirely on the panel and keeps working without Home Assistant.
- While a controller is off, its integral term is kept, so it continues from the learned value when switched on again.
  It is also saved to flash every 30 minutes, so the controllers continue from it after a reboot.
- *Dual only:* heating and cooling never run at the same time. If both controllers demand at once, e.g. in `heat_cool` mode with a retained integral term,
  the one whose setpoint is exceeded more wins. `heater_relay` and `cooler_relay` must be different relays (checked at build time).
  The climate chip and the climate page show the relay that actually runs.
- *Single only:* the thermostat offers the modes `off` and `heat`, which act as "room off/on".
  The operating mode (heat or cool) is read from `pid_mode_entity` and stored in flash,
  so the panel keeps the last mode while Home Assistant is unavailable.
  When the operating mode changes, the integral terms are reset and the valve is closed before the other controller takes over.
  The display follows the operating mode: the icon on the climate page shows a fire (heat) or a snowflake (cool),
  colored while the valve is driven and gray while idle. The climate chip shows the same when its visibility is set to always;
  with the default visibility (only while active), it is hidden while the valve is idle. On the climate page, the mode button shows heat or cool accordingly;
  both buttons only switch the room on, the operating mode itself is changed in Home Assistant.
- Presets: the dual add-on keeps `Off` and `Home`, as described in [Presets](#presets).
  The single add-on offers `Comfort`, `Eco` and `Off` instead, see [PID presets](#pid-presets-single-only).

> [!IMPORTANT]
> Include **only one** climate package: either `pid_single` or `pid_dual`, not together with `climate_heat`, `climate_cool`, `climate_dual` or each other; this is checked at build time.
> The PID packages include the climate base themselves.
> The `climate_backup` add-on cannot be combined with the PID packages, as it follows the thermostat's actions, which do not drive the relays here; this is checked at build time.
> After a reboot, the controllers continue from the integral term saved up to 30 minutes earlier; only the first boot starts from `pid_starting_integral`.
> Without Home Assistant the panel reboots after `api: reboot_timeout` (`60min` in this project),
> and without Wi-Fi after `wifi: reboot_timeout` (`15min` by default in ESPHome). Each reboot briefly interrupts the regulation and ends a running boost;
> the controllers then continue from their saved integral term. Setting both to `0s` in your local yaml avoids these reboots during long outages,
> but a panel whose connection hangs then stays offline until it is power-cycled.
> Thermostatic radiator heads, if present, must be set well above the target temperature, otherwise two controllers fight each other.
> The relays driven by the PID packages must be dedicated to them: a hardware button set to control the same relay
> fights the PWM output until its next switch.

<!-- Separates the two alerts -->

> [!CAUTION]
> When cooling through the floor, surfaces below the dew point collect condensation, which can damage the floor and building.
> The PID packages do not monitor the dew point: the controllers, and the boost of the single package, open the valve regardless of humidity.
> Cooling requires dew-point protection on the heating system side, e.g. in the heat pump or at the manifold.

### PID configuration

The following keys are available in your `substitutions`, in addition to the common climate keys above
(`temp_units`, `temp_min`, `temp_max`, `target_low`, `target_high`, `heat_deadband`, `heat_overrun`, `cool_deadband`, `cool_overrun`).
In PID mode, the deadband and overrun keys of the thermostat only affect the activity shown on the display, not the relays.

<!-- markdownlint-disable MD013 MD033 -->
| Key | Package | Required | Supported values | Default | Description |
| :- | :-: | :-: | :-: | :-: | :- |
| valve_relay | single | Optional | `1` or `2` | `1` | Relay switching the valve actuators, used for heating and cooling. |
| pid_mode_entity | single | Optional | Entity ID in Home Assistant | `input_select.climate_mode` | Entity holding the central operating mode. |
| pid_mode_heat_state | single | Optional | String | `Heat` | State of `pid_mode_entity` that selects heating. |
| pid_mode_cool_state | single | Optional | String | `Cool` | State of `pid_mode_entity` that selects cooling. Any other state is ignored. |
| heater_relay | dual | Mandatory | `1` or `2` | `0` (disabled) | Relay driven by the heat controller. |
| cooler_relay | dual | Mandatory | `1` or `2` | `0` (disabled) | Relay driven by the cool controller. |
| pid_kp | both | Optional | Number | `0.3` | Proportional gain: output share per Kelvin of error (`0.3` = 30 %/K). Seed for the first boot only. |
| pid_tn | both | Optional | Minutes | `180` | Integral time Tn. Seed for the first boot only; `ki` is calculated as `kp / (Tn × 60)`. |
| pid_kd | both | Optional | Number | `0.0` | Derivative term, usually not useful for slow systems. |
| pid_deadband | both | Optional | Number in Kelvin | `0.25` | Deadband around the setpoint. Inside it, the proportional term is disabled and the integral term is slowed down. Also in Kelvin when `temp_units` is `°F`. Seed for the first boot only. |
| pwm_period | both | Optional | Seconds | `1200` | PWM cycle time. Seed for the first boot only. |
| pid_starting_integral | both | Optional | `0.0` to `1.0` | `0.0` | Integral term on the first boot, e.g. the typical average output in winter, to shorten the first settling. Afterwards the integral term is restored from flash. |
| pid_sync_interval | both | Optional | Seconds | `10` | Interval for syncing the thermostat to the controllers. |
| pid_sample_interval | both | Optional | Seconds | `30` | Interval between controller computations. The controllers average their output over 5 computations, or 15 inside the deadband, so this sets how long that smoothing lasts. |
| pid_boost_duration | single | Optional | Minutes | `30` | Boost duration. Seed for the first boot only. |
| pid_boost_chip | single | Optional | `chip01` to `chip07` or `""` | `""` (none) | User chip on the home page taken over for the boost. That chip can no longer be configured in the Blueprint. |
| pid_boost_value | single | Optional | `value01` to `value04` or `""` | `""` (none) | Value slot on the climate page showing the remaining boost time. Leave the matching **Climate page - Entity State** empty in the Blueprint, as both would write to it. |
| pid_comfort_entity | single | Optional | Entity ID in Home Assistant | `input_number.climate_comfort` | Entity with the temperature of the `Comfort` preset, in `temp_units`. |
| pid_eco_entity | single | Optional | Entity ID in Home Assistant | `input_number.climate_eco` | Entity with the temperature of the `Eco` preset, in `temp_units`. |
| pid_comfort_temp | single | Optional | Number in `temp_units` | `21` (°C) or `70` (°F) | Temperature of the `Comfort` preset until `pid_comfort_entity` has reported a value. |
| pid_eco_temp | single | Optional | Number in `temp_units` | `18` (°C) or `64` (°F) | Temperature of the `Eco` preset until `pid_eco_entity` has reported a value. |
| pid_preset_buttons | single | Optional | `true` or `false` | `true` | Show `Comfort` and `Eco` as buttons on the climate page instead of the preset picker. |
| pid_number_mode | both | Optional | `box` or `slider` | `box` | Input mode of the number entities in Home Assistant: type a value or use a slider. |
| pid_name_* | both | Optional | String | English names, see below | Entity names, e.g. to localize them or to keep existing entity IDs. |

<!-- markdownlint-enable MD013 MD033 -->

### PID entities

After the first boot, the tuning entities are the source of the parameters (`restore_value`).
Changes are applied immediately, without reflashing.

<!-- markdownlint-disable MD013 -->
| Entity (default name) | Name key | Category | Description |
| :- | :- | :-: | :- |
| PID kp | `pid_name_kp` | Config | Proportional gain (`0.05` to `1.0`). Applies to both controllers. |
| PID Tn | `pid_name_tn` | Config | Integral time in minutes (`10` to `600`). Applies to both controllers. |
| PID PWM period | `pid_name_pwm_period` | Config | PWM cycle time in seconds (`300` to `3600`). |
| PID Deadband | `pid_name_deadband` | Config | Deadband in Kelvin (`0.0` to `1.0`). |
| PID Reset integral | `pid_name_reset_integral` | Config | Button resetting the integral terms of both controllers. |
| PID Heat output | `pid_name_heat_output` | Diagnostic | Output of the heat controller (0-100 %). |
| PID Cool output | `pid_name_cool_output` | Diagnostic | Output of the cool controller (0-100 %). |
| PID Heat integral | `pid_name_heat_integral` | Diagnostic | Integral term of the heat controller. |
| PID Cool integral | `pid_name_cool_integral` | Diagnostic | Integral term of the cool controller. |
| PID Operating mode | `pid_name_mode` | Diagnostic | *Single only:* operating mode currently used by the panel. |
| PID Boost | `pid_name_boost` | - | *Single only:* switch opening the valve fully for the boost duration, see below. |
| PID Boost duration | `pid_name_boost_duration` | Config | *Single only:* boost duration in minutes (`5` to `240`). |
| PID Boost end | `pid_name_boost_end` | - | *Single only:* end of the running boost as timestamp, shown as a countdown in Home Assistant; unknown while no boost is running. |

<!-- markdownlint-enable MD013 -->

### PID boost (single only)

The `PID Boost` switch opens the valve fully for `PID Boost duration` minutes, bypassing the controller.
It differs from AHC's party mode, which raises the setpoint: the boost opens the valve fully, regardless of the setpoint.
In cooling mode, this means full cooling.
The controllers keep running in the background; when the boost ends, the valve is handed back to the controller of the active operating mode.
Turning the switch off ends the boost early, turning it on again restarts the duration.
The boost is ignored while the room is off, and it ends when the room is switched off or the operating mode changes.
On the climate page, a rocket button starts and stops the boost.
Optionally, the boost can also have its own chip on the home page (rocket, orange when heating, blue when cooling), in the user chip slot set by `pid_boost_chip`,
and its remaining time can be shown in minutes next to an hourglass in the climate page value slot set by `pid_boost_value`.
Both are driven by the panel itself and also work without Home Assistant; the slots they use are no longer available in the Blueprint.
The climate chip keeps showing the active fire or snowflake, as the valve is open.
The `PID Boost end` sensor holds the end time; Home Assistant shows it as a live countdown (e.g. "in 25 minutes").
It needs a synchronized clock and stays unknown if the panel has no time from Home Assistant yet.

> [!NOTE]
> The integral term keeps working during the boost. As the room approaches the setpoint faster,
> it usually builds up less than without the boost, but a slight overshoot after the boost is possible.

### PID presets (single only)

The single add-on offers the presets `Comfort`, `Eco` and `Off` on the climate page and in Home Assistant.
On the climate page, `Comfort` (sofa) and `Eco` (leaf) are two buttons in the mode button row instead of the preset picker.
A button is highlighted while the setpoint equals its temperature, so a manual change to another temperature clears the highlight.
`Off` is the off button. Set `pid_preset_buttons: "false"` to use the preset picker instead.
`Comfort` and `Eco` switch the room on and set the setpoint to the value of `pid_comfort_entity` or `pid_eco_entity`.
These values are stored in flash, so the presets also work while Home Assistant is unavailable.
When the value changes while its preset is active, the setpoint follows.
Until an entity has reported a value, the preset uses `pid_comfort_temp` or `pid_eco_temp`.

When the setpoint is set to the comfort or eco value from elsewhere, e.g. by a schedule in Home Assistant, the panel marks the matching preset within `pid_sync_interval` seconds.

> [!NOTE]
> A setpoint that matches neither value keeps the last preset marked, as ESPHome cannot unselect a preset.

### Use with Advanced Heating Control (AHC)

The `pid_single` add-on was designed for and tested with the [Advanced Heating Control](https://github.com/panhans/HomeAssistant) Blueprint (AHC V5) by panhans:
AHC decides which setpoint applies when (schedules, presence, windows), and the panel turns it into a valve position on its own, also while Home Assistant is unavailable.
The `pid_dual` add-on is not suited for AHC: AHC only sets a single target temperature, which a thermostat with two setpoints rejects.

Recommended setup:

- Select the panel's climate entity under **Thermostats / Climates** and set **Operation / HVAC Mode** to `heat` for comfort and eco.
  With this add-on, `heat` means "room on"; whether the panel heats or cools follows `pid_mode_entity`, not AHC.
  Don't use AHC's `cool` or `heat_cool` modes: the panel does not offer them, so AHC would skip it.
- Leave **Enable Calibration** off. The panel regulates on its own temperature source, see [Temperature source](#temperature-source).
- Use `input_number` helpers for **Comfort Temperature** and **Eco Temperature**.
  To use different values for heating and cooling, let an automation copy the matching values into these helpers when `pid_mode_entity` changes; AHC applies them right away.
- Set `pid_comfort_entity` and `pid_eco_entity` to the same helpers. The panel's `Comfort` and `Eco` presets then use AHC's temperatures,
  and the preset matching the setpoint written by AHC is marked on the panel.
- AHC does not react when the room is switched on at the panel. To apply the current setpoint right away,
  let an automation fire AHC's `ahc_delay_event` when the panel's climate entity changes to `heat`.

AHC features with this add-on:

<!-- markdownlint-disable MD013 -->
| AHC feature | Status | Note |
| :- | :- | :- |
| Thermostats / Climates, Comfort and Eco Temperature (`input_number`), HVAC mode `heat` | Tested | Basis of the setup above. |
| Schedules | Tested | |
| Persons, Guest Mode, Proximity, Presence Sensor, Off If Nobody Home | Expected to work | Only decide between comfort, eco and off. |
| Party mode (timers or booleans), Force Eco Temperature | Expected to work | A number at the end of a party timer's name sets its temperature. |
| Windows & Doors | Expected to work | Set **Window Open Temperature** to `0` (off). A temperature above 0 is a setpoint, so in cooling mode the panel would cool with the window open. |
| Away Temperature Offset | Heating only | AHC always subtracts the offset, so in cooling mode the room is cooled more while away. |
| Frost Protection | Heating only | Sets a low fixed temperature (e.g. 5 °C) after a long absence, which means full cooling in cooling mode. |
| Off If Above/Below Room Temperature, Outside and Room Temperature Threshold, Aggressive mode | Heating only | AHC applies heating logic, as its HVAC mode is `heat`. |
| Liming Protection | Heating only | Sets the maximum temperature, so the controller opens the valve; in cooling mode it stays closed. |
| Force Max Temperature | Heating only | Same as above, without a duration. |
| Calibration, Valve Positioning | Not applicable | Meant for TRVs; the panel regulates and drives the valve itself. |
| `cool` / `heat_cool` HVAC modes | Not applicable | Heating and cooling are switched centrally with `pid_mode_entity`. |

<!-- markdownlint-enable MD013 -->

### PID tuning

#### How the output is formed

Every `pid_sample_interval` seconds, each active controller computes an output between 0 and 100 % from two parts (with the default `pid_kd: 0`, see [Adjusting the parameters](#adjusting-the-parameters)):

- **Proportional part (`kp`):** reacts to the current difference from the setpoint. With `kp` at `0.3`, a room 1 K below the setpoint adds 30 %.
- **Integral part (`Tn`):** collects the difference over time. While the room stays too cold, it keeps rising until the output is enough.
  Once settled, it holds the room's base load, for example "this room needs 25 % heating in winter", and the proportional part only corrects deviations.

The output becomes the on-time of each PWM cycle: 25 % of a 20-minute `PID PWM period` keeps the valve relay on for 5 minutes and off for 15.

#### Reading the diagnostic entities

- **PID Heat output** / **PID Cool output** move around the base load once the room has settled.
- **PID Heat integral** / **PID Cool integral** change slowly, over hours. For underfloor heating in winter, 20 % to 40 % is typical; in mild weather it drops close to 0.
  The cool integral is negative (`-100 %` to `0 %`), so read it by its magnitude: about `-20 %` to `-40 %` is a typical cooling base load.
  **PID Cool output** is shown as a positive duty, like the heat output.
- An output close to 100 % for hours, or an integral stuck at 100 % (`-100 %` when cooling), means the system cannot keep up:
  the flow temperature is too low (too high when cooling), or the setpoint too high for the heating system (too low for cooling).
- A very low output and an integral close to 0 while the room is warm enough means the flow temperature is too high.
  With a weather-compensated flow temperature, adjust the heating curve rather than the controller.

#### Adjusting the parameters

<!-- markdownlint-disable MD013 -->
| Entity | Raise it when | Lower it when |
| :- | :- | :- |
| PID kp | The room reaches the setpoint very slowly. | The temperature swings up and down around the setpoint, or the output jumps. |
| PID Tn | The room overshoots and swings slowly, over hours. | A lasting difference from the setpoint takes very long to disappear. |
| PID PWM period | The relay should switch less often, or the valve actuators are slow (thermal actuators take a few minutes to open). | The system reacts fast, as radiators do. |
| PID Deadband | The output reacts nervously to small temperature changes. | The temperature should be held more tightly. |

<!-- markdownlint-enable MD013 -->

Use **PID Reset integral** only after a major change, such as a new heating curve or a different room setup: the learned base load is lost and takes hours to build up again.

As a starting point, the defaults suit underfloor heating in screed (`kp` `0.3`, `Tn` 180 minutes, 20-minute PWM period).
Radiators react faster and usually work better with about `kp` `0.6`, `Tn` 50 minutes and a 10-minute PWM period, as in the [dual setpoints example](#example-dual-setpoints).
Leave `pid_kd` at `0`: the derivative part reacts to how fast the temperature changes, which does not help slow systems and amplifies sensor noise.

#### Tuning procedure

1. Start with the defaults and leave them for one or two days. A slow system reacts over hours, so every change needs time to show its effect.
2. Watch the room temperature and the controller output together, for example in a Home Assistant history graph.
3. Change only one value at a time, by about 30 % to 50 %, and wait another day.
4. Aim for a temperature within about ±0.3 K of the setpoint and an output that moves calmly around the base load, without jumping between 0 % and 100 %.

Typical patterns:

- **Slow waves over several hours around the setpoint:** `kp` too high or `Tn` too short. Lower `kp` first.
- **The room stays below the setpoint for a long time, then creeps up:** `Tn` too long.
- **The output jumps every few minutes:** the deadband is too small or `kp` too high.
- **Slightly too warm after each morning heat-up:** usually the heat stored in the screed. A somewhat longer `Tn` or lower `kp` helps, but some overshoot is normal with screed.

Avoid smoothing filters on the temperature sensor with time windows longer than one minute; they delay the reaction of an already slow system.

### Example: single setpoint with central operating mode

Home Assistant (e.g. in a package):

```yaml
input_select:
  climate_mode:
    name: Climate operating mode
    options:
      - Heat
      - Cool
```

Panel:

```yaml
substitutions:
  # Settings - Editable values
  device_name: "YOUR_NSPANEL_NAME"
  friendly_name: "Your panel's friendly name"
  wifi_ssid: !secret wifi_ssid
  wifi_password: !secret wifi_password

  ## Add-on climate PID
  valve_relay: "1"
  pid_mode_entity: input_select.climate_mode

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
      - esphome/nspanel_esphome_addon_climate_pid_single.yaml
```

### Example: dual setpoints

```yaml
substitutions:
  # Settings - Editable values
  device_name: "YOUR_NSPANEL_NAME"
  friendly_name: "Your panel's friendly name"
  wifi_ssid: !secret wifi_ssid
  wifi_password: !secret wifi_password

  ## Add-on climate PID
  heater_relay: "1"
  cooler_relay: "2"
  pid_kp: "0.6"   # Radiators react faster than underfloor heating
  pid_tn: "50"
  pwm_period: "600"

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
      - esphome/nspanel_esphome_addon_climate_pid_dual.yaml
```

## Examples

### Cooler

```yaml
substitutions:
  # Settings - Editable values
  device_name: "YOUR_NSPANEL_NAME"
  friendly_name: "Your panel's friendly name"
  wifi_ssid: !secret wifi_ssid
  wifi_password: !secret wifi_password
  ota_password: ""  # Optional: set OTA password, or use ${wifi_password} for backward compatibility (see migration guide)
  language: en      # Language code - see docs/localization.md for all supported codes

  # Add-on configuration (if needed)
  ## Upload TFT
  upload_tft_automatically: true

  ## Add-on climate
  cooler_relay: "1"     # Possible values: "1" or "2"
  temp_units: "°F"      # Temperatures in Fahrenheit
  temp_min: "40"        # Min supported temperature is 40°F
  temp_max: "80"        # Max supported temperature is 80°F
  cool_deadband: "1.2"  # Temperature delta before engaging cooling
  cool_overrun: "1.2"   # Temperature delta before disengaging cooling

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
      - esphome/nspanel_esphome_addon_climate_cool.yaml
      # - esphome/nspanel_esphome_addon_climate_heat.yaml
      # - esphome/nspanel_esphome_addon_climate_dual.yaml
      # - esphome/nspanel_esphome_addon_display_light.yaml  # Show the display as a light in Home Assistant
```

### Heater

```yaml
substitutions:
  # Settings - Editable values
  device_name: "YOUR_NSPANEL_NAME"
  friendly_name: "Your panel's friendly name"
  wifi_ssid: !secret wifi_ssid
  wifi_password: !secret wifi_password
  ota_password: ""  # Optional: set OTA password, or use ${wifi_password} for backward compatibility (see migration guide)
  language: en      # Language code - see docs/localization.md for all supported codes

  # Add-on configuration (if needed)
  ## Upload TFT
  upload_tft_automatically: true

  ## Add-on climate
  heater_relay: "1"     # Possible values: "1" or "2"
  temp_units: "°F"      # Temperatures in Fahrenheit
  temp_min: "40"        # Min supported temperature is 40°F
  temp_max: "80"        # Max supported temperature is 80°F
  heat_deadband: "1.2"  # Temperature delta before engaging heat
  heat_overrun: "1.2"   # Temperature delta before disengaging heat

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
      - esphome/nspanel_esphome_addon_climate_heat.yaml
      # - esphome/nspanel_esphome_addon_climate_dual.yaml
      # - esphome/nspanel_esphome_addon_display_light.yaml  # Show the display as a light in Home Assistant
```

### Dual

```yaml
substitutions:
  # Settings - Editable values
  device_name: "YOUR_NSPANEL_NAME"
  friendly_name: "Your panel's friendly name"
  wifi_ssid: !secret wifi_ssid
  wifi_password: !secret wifi_password
  ota_password: ""  # Optional: set OTA password, or use ${wifi_password} for backward compatibility (see migration guide)
  language: en      # Language code - see docs/localization.md for all supported codes

  # Add-on configuration (if needed)
  ## Upload TFT
  upload_tft_automatically: true

  ## Add-on climate
  heater_relay: "1"     # Possible values: "1" or "2"
  cooler_relay: "2"     # Possible values: "1" or "2"
  temp_units: "°F"      # Temperatures in Fahrenheit
  temp_min: "40"        # Min supported temperature is 40°F
  temp_max: "80"        # Max supported temperature is 80°F
  cool_deadband: "1.2"  # Temperature delta before engaging cooling
  cool_overrun: "1.2"   # Temperature delta before disengaging cooling
  heat_deadband: "1.2"  # Temperature delta before engaging heat
  heat_overrun: "1.2"   # Temperature delta before disengaging heat

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
      - esphome/nspanel_esphome_addon_climate_dual.yaml
      # - esphome/nspanel_esphome_addon_display_light.yaml  # Show the display as a light in Home Assistant
```

### Real Use Case Example: Water Underfloor Heating with NSPanel

The NSPanel works nicely as a replacement for existing water underfloor heating controllers, installed at eye level and often powered by 100-240VAC.
In my specific use case, the control valve is of the 'normally closed' type, meaning that an open relay indicates 'no heating.'
I have utilized relay #1 for this purpose. It's important to note that this setup does not act as a PID controller, but operates on a simple on-off mechanism.
The system initiates heating at a preset value below and turns it off at a value above the set point.
I have set these values to `0.1°C` below and `0.1°C` above the target temperature, respectively.
These adjustments have been effectively maintaining the desired temperature in all of our four rooms.
The thermostat's settings range from a minimum of `15°C` to a maximum of `22°C`, with a granularity of `0.1°C` for adjustments.
Furthermore, I have selected `18.3°C` as the default target temperature.

```yaml
substitutions:
  ## Add-on climate
  heater_relay: "1"     # Use relay 1
  temp_min: "15"
  temp_max: "22"
  target_low: "18.3"    # Default target temperature
  heat_deadband: "0.1"
  heat_overrun: "0.1"

climate:
  - id: !extend thermostat_embedded
    visual:
      temperature_step:
        target_temperature: 0.1  # See "Target temperature step" above
```

Please note that employing any filter to smooth the temperature readings, such as averaging, is discouraged if the relevant timescale exceeds 1min.
Such filtering methods can delay the response times of an already slow underfloor heating system.
Despite this, the temperature stability achieved is quite satisfactory, as illustrated in the graph below.

![Temperature vs Time](pics/addon_underfloor.png)
*On March 24th (around 9:20 PM) and March 25th (around 7:30AM) a window was opened, and all heaters were set to 'off.' for 30 min*
