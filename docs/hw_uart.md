# Hardware UART

## Description

This package configures the serial connection between the ESP32 and the Nextion
display inside the NSPanel. It is part of the core firmware and is included
automatically by `nspanel_esphome.yaml`.

The UART layer also implements the **boot baud rate scan**, which automatically
recovers panels whose display is operating at an unexpected baud rate (for
example, after a stock or legacy TFT, or after a previously interrupted upload).

> [!NOTE]
> Most users do not need to change anything in this section. The defaults are
> tuned for the standard NSPanel hardware.

## Configuration

The following keys are available to be used in your `substitutions`:

<!-- markdownlint-disable MD013 -->
| Key | Required | Supported values | Default | Description |
| :- | :-: | :-: | :-: | :- |
| `BAUD_RATE` | Optional | `921600`, `115200`, `9600`, `230400`, `57600`, `38400`, `256000`, `512000`, `250000`, `19200`, `31250`, `4800`, `2400` | `921600` | Target operating baud rate between the ESP32 and the Nextion display. The TFT firmware shipped with NSPanel Easy enforces this rate on every boot, so changing it requires reflashing the TFT after the change. See [`BAUD_RATE` behaviour](#baud_rate-behaviour). |
| `BAUD_RATE_TIMEOUT` | Optional | ESPHome duration string | `11s` | Time the panel waits for the display to respond at `BAUD_RATE` after power-cycling it, before triggering the [boot baud rate scan](#boot-baud-rate-scan). Also applied between retries in the reboot loop when the scan finds no responsive rate. |
| `BAUD_RATE_DELAY_AFTER_POWER_ON` | Optional | ESPHome duration string | `2s` | Delay between powering the display on and starting the first probe inside the baud rate scan. Increase if your hardware needs longer to initialize before responding. |
| `BAUD_RATE_PROBE_MS` | Optional | Positive integer (milliseconds) | `1500` | Per-rate probe window during the [boot baud rate scan](#boot-baud-rate-scan) and the diagnostic [Scan baud rate button](addon_upload_tft.md#include_button_scan_baud_rate-behaviour). Increase only if your hardware shows marginal UART timing on a given rate. |
| `uart_debug` | Optional | `true`/`false` | `false` | Logs detailed UART traffic. |
<!-- markdownlint-enable MD013 -->

### `BAUD_RATE` behaviour

`BAUD_RATE` is the rate the ESP32 expects to use after boot. The Nextion side
of the link is configured by the TFT firmware itself (`program.s`), so if you
change `BAUD_RATE` in your YAML you must also upload a TFT compiled with the
matching rate. Otherwise the boot baud rate scan will detect the mismatch on
every boot and the panel will operate in fallback mode until a TFT upload
realigns the two sides.

> [!WARNING]
> Changing `BAUD_RATE` away from the default `921600` is unusual. The official
> TFT files published in this repository are all compiled at `921600 bps`, so
> a change requires either compiling a custom TFT or accepting that the panel
> will run permanently in fallback mode at the lower rate.

## Boot baud rate scan

The display is checked on every watchdog round (the first one about 89 seconds
after boot, then every 89 seconds). If the display is not detected, the panel
power-cycles it, waits `BAUD_RATE_TIMEOUT` (11 seconds by default) for it to
respond at `BAUD_RATE`, and then automatically scans Nextion-supported baud
rates to find one where the display answers.

### When the scan triggers

- The display is not detected on a watchdog round and does not complete its
  setup handshake at the configured `BAUD_RATE` within `BAUD_RATE_TIMEOUT`
  after being power-cycled.
- Power-cycle and retry loops also re-trigger the scan if the display continues
  to be unresponsive.

### Scan order

The scan tries rates in the following order, stopping at the first responsive
rate:

1. `${BAUD_RATE}` (the configured primary rate)
2. `921600` (skipped if same as primary)
3. `115200` (skipped if same as primary)
4. `9600` (skipped if same as primary)
5. `230400`, `57600`, `38400`, `256000`, `512000`, `250000`, `19200`, `31250`,
   `4800`, `2400` (each skipped if same as primary)

The order is optimized for the rates most commonly found in NSPanel
installations: the configured rate first, then the two rates used by stock and
legacy TFTs.

### Outcome

- **Match found**: the ESP UART switches to the discovered rate. If the rate
  is not `BAUD_RATE`, the panel operates in fallback mode for the current
  session and a TFT upload at the discovered rate will realign the link on
  the next boot.
- **No match**: the panel power-cycles the display and repeats the wait-and-scan
  cycle indefinitely. The display never enters a stranded state.

### Timing

With the default settings (`BAUD_RATE_TIMEOUT=11s`, `BAUD_RATE_DELAY_AFTER_POWER_ON=2s`,
`BAUD_RATE_PROBE_MS=1500`), scan duration is roughly:

- Best case (display answers at `BAUD_RATE`): ~1.5 seconds
- Common case (display at one of the top three rates): 1.5 to 4.5 seconds
- Worst case (display at the slowest scanned rate, or no display at all):
  approximately 20 seconds for a full sweep, plus the power-cycle and post-power-on
  delays

## Diagnostic entities

This package exposes one diagnostic entity in Home Assistant:

<!-- markdownlint-disable MD013 -->
| Entity | Type | Description |
| :- | :- | :- |
| `Baud rate` | Sensor (text) | Current operating baud rate of the ESP UART, in `bps`. Updated on every successful baud rate change and periodically thereafter. |
<!-- markdownlint-enable MD013 -->

> [!TIP]
> If you suspect a baud rate problem, check the `Baud rate` sensor first. A
> value other than your configured `BAUD_RATE` indicates the panel is running in fallback
> mode and a TFT upload is required to restore the configured rate.

## Diagnostic button

The companion [Upload TFT add-on](addon_upload_tft.md) exposes an optional
`Scan baud rate` button that runs the same scan on demand. See
[include_button_scan_baud_rate behaviour](addon_upload_tft.md#include_button_scan_baud_rate-behaviour).

## Troubleshooting

For symptoms related to baud rate mismatches and TFT uploads, see the
[TFT Transfer Troubleshooting Guide](tft_upload.md).
