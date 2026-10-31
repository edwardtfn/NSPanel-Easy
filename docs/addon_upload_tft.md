# Add-on: Upload TFT

## Description

This add-on enables your panel to upload TFT firmware files to the Nextion display,
either automatically after boot or manually via a button in Home Assistant.

The Nextion display has its own controller, memory, and firmware.
The UI elements and firmware are compiled into a TFT file which must be loaded
onto the display for proper functionality.
This add-on handles downloading the TFT file from a remote server (or a local URL)
and transferring it to the display over serial.

> [!NOTE]
> This add-on is **included by default** when using the standard `nspanel_esphome.yaml` package.
> You only need to reference it explicitly if you are using
> [granular package selection](install.md#granular-package-selection)
> or if you previously removed it.

### How it works

When the device boots and connects to Home Assistant, a version check compares
the current TFT version on the display with the expected version from the firmware.
Depending on your configuration, the add-on will either:

1. **Automatically upload** the matching TFT file after a configurable wait period, or
2. **Wait for a manual trigger** via the "Update TFT display" button in Home Assistant.

The TFT file URL is resolved in this order:

1. If `nextion_update_url` is set in your substitutions → use it as-is, bypassing all other logic
2. Otherwise → build the URL automatically from the **Display model** selector and the current
   firmware version

> [!IMPORTANT]
> The **Display model** selector (under **Configuration** on your device's page in Home Assistant)
> serves two purposes: it determines which TFT file is downloaded during an upload, and it
> configures the panel's runtime behavior (touch calibration, button layout, display orientation).
> Always keep it set to the option matching your physical hardware.

## Installation

This add-on is already part of the default `nspanel_esphome.yaml` package,
so most users do not need to change anything.

If you are using granular package selection, add the reference to the
`addon_upload_tft` file in your ESPHome settings in the `packages` section,
after the core and standard packages:

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
      - nspanel_esphome.yaml # Basic package (already includes this add-on)
      # Optional advanced and add-on configurations
      # - esphome/nspanel_esphome_addon_climate_cool.yaml
      # - esphome/nspanel_esphome_addon_climate_heat.yaml
      # - esphome/nspanel_esphome_addon_climate_dual.yaml
      # - esphome/nspanel_esphome_addon_cover.yaml
      # - esphome/nspanel_esphome_addon_display_light.yaml
```

If you use granular package selection (individual files instead of `nspanel_esphome.yaml`),
add the Upload TFT package explicitly:

```yaml
packages:
  remote_package:
    url: https://github.com/edwardtfn/NSPanel-Easy
    ref: latest
    refresh: 300s
    files:
      - esphome/nspanel_esphome_core.yaml
      - esphome/nspanel_esphome_standard.yaml
      - esphome/nspanel_esphome_addon_upload_tft.yaml  # Upload TFT add-on
```

## Configuration

The following keys are available to be used in your `substitutions`:

<!-- markdownlint-disable MD013 -->
| Key | Required | Supported values | Default | Description |
| :- | :-: | :-: | :-: | :- |
| `upload_tft_automatically` | Optional | `true` or `false` | `true` | When enabled, the device will automatically upload the TFT file when a version mismatch is detected after boot. When disabled, you must manually press the "Update TFT display" button in Home Assistant. |
| `upload_tft_wait_ms_after_setup` | Optional | Positive integer (milliseconds) | `65432` (~65 s) | Time to wait after display setup completes before starting an automatic TFT upload. This delay allows the system to stabilize after boot. Reduce for faster updates; increase if you experience boot instability. |
| `tft_upload_http_timeout` | Optional | Duration (e.g. `10s`, `30s`) | `10s` | How long to wait for the HTTP server to respond while downloading the TFT file. Raise this when the file is served over a slow or congested network, or by a busy Home Assistant instance. |
| `tft_upload_watchdog_timeout` | Optional | Duration (e.g. `15s`, `45s`) | `15s` | How long the upload may stall before the watchdog aborts the transfer. Keep it above `tft_upload_http_timeout`, otherwise the watchdog aborts before a slow request has a chance to complete. |
| `nextion_update_base_url` | Optional | Valid HTTP/HTTPS base URL | `https://raw.githubusercontent.com/edwardtfn/NSPanel-Easy/v${version}/hmi` | Base URL used when building the TFT download URL automatically. Override this to host TFT files on a local server while still benefiting from automatic model and version selection. |
| `nextion_update_url` | Optional | Valid HTTP/HTTPS URL | _(empty)_ | Full URL override for the TFT file. When set, this takes absolute priority. The model selector and version logic are completely bypassed and this URL is used as-is. Use only when you need full control over the TFT source, such as for custom TFT files. See [important note below](#nextion_update_url-behaviour). |
| `include_action_upload_tft` | Optional | `true` or `false` | `false` | When set to `true`, registers the `upload_tft` API action, allowing the TFT upload to be triggered from Home Assistant scripts or automations via `esphome.<panel>_upload_tft`. Disabled by default to reduce memory usage at boot. See [important note below](#include_action_upload_tft-behaviour). |
| `include_button_scan_baud_rate` | Optional | `true` or `false` | `false` | When set to `true`, registers a "Scan baud rate" diagnostic button that probes all Nextion-supported baud rates to find a working rate, then triggers a TFT upload at the discovered rate. Useful for recovering panels that cannot communicate at the configured rate. See [important note below](#include_button_scan_baud_rate-behaviour). |
<!-- markdownlint-enable MD013 -->

### `nextion_update_url` behaviour

> [!WARNING]
> Setting `nextion_update_url` disables all automatic URL management. The **Display model**
> selector is still used for runtime behavior (touch calibration, button layout), but the
> TFT file downloaded will always be the one at the URL you specified, regardless of which
> model is selected or which version is current. You are responsible for keeping this URL
> pointing to a compatible and up-to-date TFT file.

When `nextion_update_url` is left empty (the default), the add-on builds the download URL
automatically from the selected **Display model** and the current firmware version. This is
the recommended configuration for most users.

### `include_action_upload_tft` behaviour

> [!NOTE]
> This substitution was introduced to address boot-time memory exhaustion on devices running
> memory-intensive configurations (e.g. Bluetooth Proxy). Registering API actions consumes
> heap memory at boot; disabling unused actions recovers that memory.

When set to `false` (the default), the `upload_tft` API action is not registered.
All other functionality of this add-on remains fully operational: automatic TFT upload on
version mismatch, the "Update TFT display" button in Home Assistant, and the boot baud
rate scan are all unaffected.

Set this to `true` only if you need to trigger TFT uploads programmatically from Home
Assistant scripts or automations using the `esphome.<panel>_upload_tft` service call.

> [!IMPORTANT]
> If you were previously calling `esphome.<panel>_upload_tft` from an automation or script,
> add `include_action_upload_tft: true` to your substitutions to restore that behaviour.
> This is a breaking change introduced to improve boot stability on memory-constrained devices.

### `include_button_scan_baud_rate` behaviour

> [!NOTE]
> The boot process automatically scans baud rates when the display does not respond at
> the configured rate within a couple of minutes (see [Boot baud rate scan](#boot-baud-rate-scan)
> below). The diagnostic button described here is a manual fallback for cases where the
> automatic scan did not run, did not succeed, or where you want to force a rescan
> without rebooting the panel.

When set to `true`, this add-on exposes a "Scan baud rate" button. Pressing it:

1. Power-cycles the Nextion display
2. Probes each Nextion-supported baud rate for a response
3. On success, switches the ESP UART to the discovered rate and starts a TFT upload

### Display model options

This add-on extends the **Display model** selector with additional options beyond the hardware
models defined in the base firmware:

| Option | TFT file | Notes |
| --- | --- | --- |
| NSPanel EU | `nspanel_landscape.tft` | Standard EU hardware |
| NSPanel US | `nspanel_portrait.tft` | Standard US hardware (portrait) |
| NSPanel US Landscape | `nspanel_landscape.tft` | US hardware mounted in landscape |
| NSPanel Blank | `nspanel_blank.tft` | First-time installation only. See [NSPanel Blank](nspanel_blank.md) |

### Example: Automatic updates with a longer wait time

```yaml
substitutions:
  device_name: "YOUR_NSPANEL_NAME"
  friendly_name: "Your panel's friendly name"
  wifi_ssid: !secret wifi_ssid
  wifi_password: !secret wifi_password
  ota_password: ""  # Optional: set OTA password, or use ${wifi_password} for backward compatibility (see migration guide)
  language: en      # Language code - see docs/localization.md for all supported codes

  # Upload TFT configuration
  upload_tft_automatically: true
  upload_tft_wait_ms_after_setup: 120000  # Wait 2 minutes after setup instead of ~65s

packages:
  remote_package:
    url: https://github.com/edwardtfn/NSPanel-Easy
    ref: latest
    refresh: 300s
    files:
      - nspanel_esphome.yaml
```

### Example: Manual updates only

```yaml
substitutions:
  device_name: "YOUR_NSPANEL_NAME"
  friendly_name: "Your panel's friendly name"
  wifi_ssid: !secret wifi_ssid
  wifi_password: !secret wifi_password
  ota_password: ""  # Optional: set OTA password, or use ${wifi_password} for backward compatibility (see migration guide)
  language: en      # Language code - see docs/localization.md for all supported codes

  # Disable automatic TFT upload
  upload_tft_automatically: false

packages:
  remote_package:
    url: https://github.com/edwardtfn/NSPanel-Easy
    ref: latest
    refresh: 300s
    files:
      - nspanel_esphome.yaml
```

### Example: Using a locally hosted TFT file

Use `nextion_update_url` only when you need to serve a custom or locally hosted TFT file.
For standard setups, the automatic URL resolution based on the **Display model** selector
is recommended instead.

```yaml
substitutions:
  device_name: "YOUR_NSPANEL_NAME"
  friendly_name: "Your panel's friendly name"
  wifi_ssid: !secret wifi_ssid
  wifi_password: !secret wifi_password
  ota_password: ""  # Optional: set OTA password, or use ${wifi_password} for backward compatibility (see migration guide)
  language: en      # Language code - see docs/localization.md for all supported codes

  # Use a locally hosted TFT file - bypasses automatic model and version selection
  nextion_update_url: "http://homeassistant.local:8123/local/nspanel_landscape.tft"

packages:
  remote_package:
    url: https://github.com/edwardtfn/NSPanel-Easy
    ref: latest
    refresh: 300s
    files:
      - nspanel_esphome.yaml
```

### Example: Using a local base URL with automatic model selection

If your network cannot reach GitHub but you want to keep automatic model and version selection,
override `nextion_update_base_url` instead of `nextion_update_url`:

```yaml
substitutions:
  device_name: "YOUR_NSPANEL_NAME"
  friendly_name: "Your panel's friendly name"
  wifi_ssid: !secret wifi_ssid
  wifi_password: !secret wifi_password
  ota_password: ""  # Optional: set OTA password, or use ${wifi_password} for backward compatibility (see migration guide)
  language: en      # Language code - see docs/localization.md for all supported codes

  # Mirror the TFT files locally and point to your server
  nextion_update_base_url: "http://homeassistant.local:8123/local/nspanel-easy/"

packages:
  remote_package:
    url: https://github.com/edwardtfn/NSPanel-Easy
    ref: latest
    refresh: 300s
    files:
      - nspanel_esphome.yaml
```

## Updating the TFT

### Automatic updates

When `upload_tft_automatically` is set to `true` (the default), the device will:

1. Boot and connect to Home Assistant.
2. Receive the current TFT version from the display.
3. Compare it with the expected version from the firmware.
4. If a mismatch is detected, wait for the configured delay
   (`upload_tft_wait_ms_after_setup`) after display setup completes.
5. Automatically start the TFT upload.

> [!NOTE]
> The wait period exists to prevent the TFT upload from starting before the boot process
> has fully completed, as early uploads have been reported to cause issues on some devices.
> During this time the panel may not behave as expected, since the display is running
> an incompatible TFT version.
>
> If you prefer not to wait, you can press the "Update TFT display" button in Home Assistant
> at any time to trigger an immediate upload, bypassing the remaining wait.

### Manual updates

To manually trigger a TFT upload:

1. Go to **Settings** > **Devices & services** > **ESPHome** and select your panel.
2. Under **Configuration**, verify that **Display model** is set to the option matching
   your physical hardware.
3. Press the "Update TFT display" button.
4. The display will start the update process and the device will restart when complete.

## Memory considerations

The TFT upload process requires a significant amount of free memory on the ESP32.
If Bluetooth is enabled (e.g. with the [Bluetooth Proxy add-on](addon_bluetooth_proxy.md)),
this add-on stops the Bluetooth stack and releases its memory before the upload starts,
so both can be enabled at the same time. The panel restarts after the upload, restoring Bluetooth.
If the memory cannot be released, the log shows `Bluetooth memory release failed` and the upload is still attempted.

Other memory-intensive customizations may still cause the upload to fail due to insufficient memory.
In such cases, see the [Memory Optimization](install.md#memory-optimization-strategies)
section in the installation guide.

## Boot baud rate scan

If the display does not respond at the configured baud rate (default 921600 bps) within
a couple of minutes of boot, the panel automatically scans all Nextion-supported baud rates to
find one where the display answers. This recovers panels in the following situations:

- A previous TFT upload was interrupted, leaving the display at an unexpected rate
- The display is running stock or legacy firmware (typically 9600 or 115200 bps)
- A user manually changed the rate via Nextion commands

When the scan finds a working rate, the panel continues operating at that rate for
the current session and triggers a TFT upload. The new TFT enforces the configured
rate on its next boot, so subsequent boots return to the configured rate automatically.

If the scan finds no responsive rate, the panel power-cycles the display and repeats
the wait-and-scan cycle indefinitely.

The same scan runs when a TFT upload starts while the display is not responding, so
the upload is sent at the rate the display is actually using. If no rate answers, the
upload is aborted and the panel restarts.

The scan typically completes within seconds for known-good displays. Probe timing can
be tuned via the `BAUD_RATE_PROBE_MS` substitution (default 1500 ms per rate).
See the [Hardware UART configuration](hw_uart.md) for details.

## Troubleshooting

For common TFT transfer issues and solutions, see the
[TFT Transfer Troubleshooting Guide](tft_upload.md).
