# NSPanel Easy — Documentation

Welcome to the NSPanel Easy documentation hub.

NSPanel Easy is a code-free customization platform for the Sonoff NSPanel, built
on [ESPHome](https://esphome.io/) and [Home Assistant](https://www.home-assistant.io/).
This page indexes every guide available for the project — setup, add-ons,
reference material, and troubleshooting.

For a high-level overview of the project, see the
[main README](https://github.com/edwardtfn/NSPanel-Easy/blob/main/README.md).

## Start Here

New to NSPanel Easy, or migrating from another project? Pick the path that
matches your situation:

- **First-time setup** — [Installation and Setup](install.md) walks you through
  flashing ESPHome, uploading the TFT, and importing the Blueprint.
- **Coming from [Blackymas/NSPanel_HA_Blueprint](https://github.com/Blackymas/NSPanel_HA_Blueprint)**
  — follow the [Migration Guide](migration_from_blackymas.md). It typically
  takes less than 10 minutes per panel, with no serial flashing required.
- **Don't have a panel yet?** — the [Where to Buy](where_to_buy.md) guide lists
  retailers by region, including pre-flashed options.

## Setup and Configuration

- [Installation and Setup](install.md) — first-time flashing, from ESPHome to
  Blueprint import.
- [Blueprint](blueprint.md) — description of every setting available in the
  Home Assistant Blueprint.
- [HowTo Guide](howto.md) — day-to-day tasks: wake-up pages, RTTTL sounds,
  input booleans, updating Wi-Fi credentials, and more.
- [Localization](localization.md) — supported languages and how to change the
  panel's display language.
- [Version Compatibility Matrix](version_compatibility.md) — supported versions
  of Home Assistant and ESPHome.

## Add-ons

Optional packages that extend the base firmware:

- [Add-on: Upload TFT](addon_upload_tft.md) — automatic and manual TFT updates,
  model selection, and custom TFT hosting.
- [Add-on: Bluetooth Proxy](addon_bluetooth_proxy.md) — extend Home Assistant's
  Bluetooth range and track BLE devices through the panel.
- [Add-on: Climate](addon_climate.md) — use the panel's relays as a local
  thermostat (heater, cooler, or both) that keeps working without Wi-Fi.
- [Add-on: Cover](addon_cover.md) — drive a cover motor from the panel's
  relays.
- [Add-on: Display Light](addon_display_light.md) — expose the display
  brightness as a Home Assistant light entity.

## Reference

- [API](api.md) — custom ESPHome actions (`upload_tft`, `notification_show`,
  `rtttl_play`, `qrcode`, and more) with parameters and examples.
- [API Subscriptions](api_subscribe.md) — drive panel components directly from
  Home Assistant entity states, without an automation running per state change.
- [Alarm Control Panel](alarm.md) — configure and secure alarm control from
  the panel.
- [Customization](customization.md) — advanced examples: API encryption, custom
  OTA passwords, static IPs, reboot-on-API-failure, and more.
- [MDI Icons Cheatsheet](https://edwardtfn.github.io/NSPanel-Easy/icons/cheatsheet.html)
  — searchable list of all Material Design Icons supported by the panel.
  Click any icon to copy its `mdi:` name.
- [Using Different Versions](different_version.md) — switching between `main`,
  `beta`, `dev`, or a tagged release for each component.
- [NSPanel Blank](nspanel_blank.md) — the lightweight TFT used for first-time
  installations, when the original Sonoff firmware is still on the Nextion
  display.

## Troubleshooting

- [Panel Startup Issues](error_initializing.md) — the panel won't finish
  booting or shows an unexpected screen on startup.
- [Compiling Errors](error_compiling.md) — ESPHome fails to build the firmware.
- [TFT Upload Issues](tft_upload.md) — the TFT transfer fails, times out, or
  the display stays on the old version.

## Community and Support

- **Discord** — [discord.gg/KyVPd33znv](https://discord.gg/KyVPd33znv) is the
  fastest channel for questions, discussion, and sharing your setup.
- **GitHub Issues** — [bug reports and feature requests](https://github.com/edwardtfn/NSPanel-Easy/issues).
- **Home Assistant Community forum** — [NSPanel Easy thread](https://community.home-assistant.io/t/nspanel-easy-blueprint-configured-nspanel-customization/1005033)
  for longer-form discussion.

## Contributing

Contributions of all kinds are welcome — code, documentation, translations, and
testing. See [CONTRIBUTING.md](https://github.com/edwardtfn/NSPanel-Easy/blob/main/CONTRIBUTING.md)
for guidelines, branching conventions, and how to run the local linters.

Not a developer? You can still help by improving the docs, translating
strings, testing open [pull requests](https://github.com/edwardtfn/NSPanel-Easy/pulls),
or sharing your setup with the community.

## Setup and Overview Videos

> [!NOTE]
> These videos were recorded for the original Blackymas/NSPanel_HA_Blueprint
> project, which NSPanel Easy is derived from. The core concepts and UI are
> very similar, so they remain useful as an overview — but some specific steps
> (URLs, file paths, Blueprint options) may differ. Always cross-reference
> with the [Installation and Setup](install.md) guide for current instructions.

- [Overview and features](https://www.youtube.com/watch?v=b7vW4YtUaTs) — Mark Watt Tech
- [How to setup](https://www.youtube.com/watch?v=jpSTA_ILB8g) — Mark Watt Tech
- [(DE) Einrichtungs und Konfigurations Video](https://www.youtube.com/watch?v=3afPFg6kUdc) — SmartHome Yourself
