# ESPHome Substitutions

NSPanel Easy can be tuned at compile time through `substitutions` in your panel's ESPHome YAML.
This page lists every substitution intended for users, with its default and what it changes.

Substitutions not listed here are internal: they hold constants, values shared with the TFT or the Blueprint, or defaults that the Blueprint replaces at boot.
Overriding them is not supported, and they may change or disappear in any release.

## How to set a substitution

Add the substitution to the `substitutions` section of your panel's YAML, next to your other settings, and install the firmware again.
Every substitution is applied at compile time, so a change only takes effect after the next install.

```yaml
substitutions:
  device_name: "living_room_panel"
  friendly_name: "Living Room Panel"
  wifi_ssid: !secret wifi_ssid
  wifi_password: !secret wifi_password

  ##### My customization - Start #####
  boot_sound: true
  api_subscribe_entity_len: '128'
  ##### My customization - End #####
```

## General

<!-- markdownlint-disable MD013 -->
| Substitution | Default | Description |
| :- | :- | :- |
| `device_name` | `nspanel` | Device name in ESPHome, also used to build the entity IDs. See [Installation and Setup](install.md). |
| `friendly_name` | `${device_name}` | Name shown in Home Assistant. |
| `name` | `${device_name}` | Legacy device name, kept for configurations created with older Home Assistant versions. Prefer `device_name`. |
| `api_encryption_key` | `""` | Key to encrypt the communication with Home Assistant. Empty disables the encryption. See [Customization](customization.md#api-encryption). |
| `ota_encryption` | `false` | Encrypts the OTA updates with the API encryption key, and requires it for every update. Recommended. See [Customization](customization.md#ota-encryption). |
| `ota_password` | `""` | Password required for OTA updates. Empty disables the password. When `ota_encryption` is enabled, used only to migrate a panel that still expects a password. See [Customization](customization.md#custom-ota-password). |
| `language` | `en` | Language compiled into the firmware. Only the selected language's strings are stored on the panel. See [Localization](localization.md#setting-your-language). |
| `temp_units` | `°C` | Temperature unit used by everything the panel renders locally: `°C` or `°F`. The Blueprint must be set to the same unit. See [Localization](localization.md#temperature-unit). |
| `sntp_servers` | `""` | Network Time (SNTP) servers (up to 3, separated by `;`) used as a second time source, next to Home Assistant, so the time can be shown before Home Assistant connects. Empty uses only Home Assistant. See [Customization](customization.md#sntp-time-server). |
<!-- markdownlint-enable MD013 -->

## Home page

<!-- markdownlint-disable MD013 -->
| Substitution | Default | Description |
| :- | :- | :- |
| `chips_page_screensaver` | `true` | Shows the chips on the screensaver page as well as on the home page. |
| `chips_page_buttons` | `false` | Shows the chips on the button pages as well, at the same position as on the home page. The page's name then moves to the row below the chips. Without the option, the name stays at the top. |
| `chips_page_alarm` | `false` | Shows the chips on the alarm page as well, at the same position as on the home page. The state icon and the page's name then move to the row below the chips. |
| `home_page_simple` | `false` | Uses a simpler home page: the chips, the Wi-Fi icon, the clock and a big tile for each of the home page's custom buttons 1 to 6, with its icon and name (the button's title, or the entity's name). A tile works as the custom button on the home page. Pressing and holding the clock opens the regular home page. |
| `wifi_icon_colors` | `true` | Colours the Wi-Fi icon by signal strength while everything is connected (green, yellow or orange), and red otherwise. When `false`, the icon is grey while everything is connected, and red otherwise. In both cases it is red until Wi-Fi connects, and then grey while the panel is still booting (waiting for Home Assistant and the Blueprint). |
<!-- markdownlint-enable MD013 -->

## Home Assistant connection

<!-- markdownlint-disable MD013 -->
| Substitution | Default | Description |
| :- | :- | :- |
| `api_stale_grace_period` | `30s` | Time without Home Assistant before the values on display are indicated as stale. After a restart, they are stale until Home Assistant connects, without waiting for this period. Chips of Home Assistant entities, home custom buttons and the weather picture are then hidden (the relay and embedded thermostat chips are controlled locally and stay visible), the indoor temperature falls back to the panel's own sensor, and the outdoor temperature and the home values are greyed out. Everything recovers as soon as Home Assistant is back. |
<!-- markdownlint-enable MD013 -->

## Hardware buttons

<!-- markdownlint-disable MD013 -->
| Substitution | Default | Description |
| :- | :- | :- |
| `buttons_bar_text_follow_color` | `false` | Draws each hardware button label in the colour of its bar, which follows the button state, instead of the label colour set in the Blueprint. |
<!-- markdownlint-enable MD013 -->

The bar colours themselves are set in the Blueprint, not through substitutions.

## Pages

<!-- markdownlint-disable MD013 -->
| Substitution | Default | Description |
| :- | :- | :- |
| `require_disarm_before_rearm` | `false` | While the alarm is armed, hides the other arming modes on the alarm page, so it must be disarmed before being armed in another mode. See [Alarm Control Panel](alarm.md). |
| `page_entities_value_font_large_max_len` | `8` | Longest value on the entities pages shown with the default (largest) font; longer values switch to a smaller font, and shorter ones switch back. Length is counted in characters, with narrow characters (such as `i`, `l`, `.` or `:`) counting as half. Accepts `0` to `255`; set both `page_entities_value_font_*_max_len` to `255` to always keep the default font. |
| `page_entities_value_font_medium_max_len` | `12` | Longest value on the entities pages shown with the medium font; longer values use the smallest font. Counted as in `page_entities_value_font_large_max_len`, and must not be lower than it. |
| `page_button_color_idle` | `16904` | RGB565 colour of the button on the button page while idle. |
| `page_button_color_pressed` | `15483` | RGB565 colour of the button on the button page while pressed. |
| `page_button_color_unavail` | `10565` | RGB565 colour of the button on the button page while its entity is unavailable. |
| `page_button_color_ring` | `21162` | RGB565 colour of the ring around the button on the button page. |
| `page_switch_color_off` | `16904` | RGB565 colour of the switch track on the switch page while off. |
| `page_switch_color_on` | `15483` | RGB565 colour of the switch track on the switch page while on. |
| `page_switch_color_pending` | `33840` | RGB565 colour of the switch track on the switch page while a change is pending. |
| `page_switch_color_thumb` | `63390` | RGB565 colour of the switch thumb on the switch page. |
| `qrcode_background_color` | `65535` | RGB565 background colour of the QR code. |
| `qrcode_foreground_color` | `0` | RGB565 foreground colour of the QR code. |
| `qrcode_border_size` | `8` | Border size of the QR code, as the `dis` attribute of the Nextion QR code component. |
| `qrcode_restore_value` | `false` | Keeps the last QR code, its title and its icon across restarts, so the QR code page also works when the panel restarts without Home Assistant. Off by default, as it uses storage space on the panel. |
| `wakeup_with_button_press` | `false` | Wakes the display when a hardware button is pressed while the screensaver is showing. |
| `wakeup_with_button_press_dimmed` | `false` | When a hardware button press wakes the display, or resets its timers while dimmed, keeps it at the dimmed brightness. |
<!-- markdownlint-enable MD013 -->

## Sounds

Tones use the [RTTTL](https://esphome.io/components/rtttl/) format: `name:d=duration,o=octave,b=tempo:notes`.

<!-- markdownlint-disable MD013 -->
| Substitution | Default | Description |
| :- | :- | :- |
| `boot_sound` | `false` | Plays `tone_boot` when the panel finishes booting. |
| `tone_boot` | `two short:d=4,o=5,b=100:16e6,16e6` | Tone played at boot when `boot_sound` is enabled. |
| `page_unavailable_sound` | `false` | Plays `tone_page_unavailable` when a page is refused because Home Assistant is not connected. |
| `tone_page_unavailable` | `unavailable:d=16,o=5,b=160:a5,e5` | Tone played when a page is refused and `page_unavailable_sound` is enabled. |
| `tone_notification` | `two short:d=4,o=5,b=100:16e6,16e6` | Tone played when a notification is shown, if the notification sound is enabled. |
<!-- markdownlint-enable MD013 -->

## Advanced

These change timing, memory use or diagnostics.
The defaults suit almost every installation, so change them only to solve a specific problem.

<!-- markdownlint-disable MD013 -->
| Substitution | Default | Description |
| :- | :- | :- |
| `api_icon_names_block` | `400` | Icon names asked from the Blueprint in one block. See [API Subscriptions](api_subscribe.md#options). |
| `api_icon_names_retries` | `3` | Retries of one block of icon names before the icons given by name are drawn by domain. See [API Subscriptions](api_subscribe.md#options). |
| `api_icon_names_timeout` | `5s` | Time to wait for a block of icon names before asking for it again. See [API Subscriptions](api_subscribe.md#options). |
| `api_icon_names_spread_ms` | `30000` | Longest random wait, in milliseconds, before the first block of icon names, so panels that start together don't ask at once. See [API Subscriptions](api_subscribe.md#options). |
| `api_icon_names_pause_ms` | `1000` | Wait, in milliseconds, between blocks of icon names, doubled on each retry of a block. See [API Subscriptions](api_subscribe.md#options). |
| `api_subscribe_entity_len` | `96` | Buffer size for an `entity_id` in an [API subscription](api_subscribe.md), including the null terminator. An `entity_id` that does not fit is not bound and does not update; the log then reports the size to set. Accepts `32` to `255`; changing it discards the stored bindings once, and the Blueprint pushes them again. |
| `api_subscribe_max` | `128` | Maximum number of [API subscription](api_subscribe.md#options) bindings. |
| `api_subscribe_debounce` | `10s` | Quiet period after the last binding of a push before the panel restarts to apply it. See [API Subscriptions](api_subscribe.md#options). |
| `api_subscribe_timeout` | `120s` | Time without an end marker before a push is committed unverified. See [API Subscriptions](api_subscribe.md#options). |
| `invalid_cooldown` | `100ms` | Cooldown after an invalid click sequence on the hardware buttons, as in ESPHome's `on_multi_click` [binary sensor automation](https://esphome.io/components/binary_sensor/). |
| `ntc_source_filter_delta` | `0.002` | Minimum change of the smoothed voltage from the temperature sensor before a new reading is published. Raise it if the temperature fluctuates without a real change. |
| `ntc_source_filter_heartbeat` | `5min` | Interval at which the temperature is published again when it has not changed. |
| `temp_calibration_warmup` | `30` | Minutes after boot before [temperature auto-calibration](blueprint.md#nspanel-temperature-sensor---calibration-reference-optional) starts, while the panel's self-heating settles. |
| `temp_calibration_time_constant` | `30` | Minutes for the auto-calibrated correction to cover about 63% of a change in the offset. Higher values react slower but are less affected by short changes in self-heating. |
| `temp_calibration_max_age` | `120` | Minutes without a change of the calibration reference before auto-calibration pauses, keeping the current correction. |
| `temp_calibration_hysteresis` | `0.75` | Fraction of a Temperature Correction step (0.1 °C or 1 °F) the calibrated value must move before the correction changes. Lower values follow the reference more closely; values of 0.5 or less let the correction flip back and forth between two steps. |
| `temp_reference_max_age` | `temp_calibration_max_age` | Minutes without a change of the [calibration reference](blueprint.md#nspanel-temperature-sensor---calibration-reference-optional) before an [embedded thermostat regulating on it](addon_climate.md#temperature-source) falls back to the panel's sensor. |
| `qrcode_initial_value` | `''` | Text of the QR code until one is received. |
| `qrcode_title_initial_value` | `''` | Title of the QR code page until one is received. |
| `qrcode_icon_color_initial_value` | `Colors::RGB565_GRAY_LIGHT` | Colour of the QR code button on the home page until one is received. Accepts an RGB565 number. |
| `THEME_DARK_COLOR_PCO_MAIN` | `65535` | RGB565 font colour for the main information in the dark theme. |
| `THEME_DARK_COLOR_PCO_SECONDARY` | `52857` | RGB565 font colour for secondary information in the dark theme. |
| `THEME_DARK_COLOR_PCO_NAVIGATION` | `33808` | RGB565 font colour for navigation components, such as the back button or the page index, in the dark theme. |
| `THEME_DARK_COLOR_PCO_UNSUPPORTED` | `6339` | RGB565 font colour for unsupported or unavailable components in the dark theme. |
| `THEME_LIGHT_COLOR_PCO_MAIN` | `0` | As `THEME_DARK_COLOR_PCO_MAIN`, for the light theme, which is still under construction. |
| `THEME_LIGHT_COLOR_PCO_SECONDARY` | `29614` | As `THEME_DARK_COLOR_PCO_SECONDARY`, for the light theme. |
| `THEME_LIGHT_COLOR_PCO_NAVIGATION` | `16904` | As `THEME_DARK_COLOR_PCO_NAVIGATION`, for the light theme. |
| `THEME_LIGHT_COLOR_PCO_UNSUPPORTED` | `52857` | As `THEME_DARK_COLOR_PCO_UNSUPPORTED`, for the light theme. |
| `WIFI_USE_PSRAM` | `true` | Lets the Wi-Fi stack allocate its buffers in PSRAM, leaving more internal memory free. |
| `memory_dump_details` | `false` | Adds a detailed breakdown of each memory region to the memory log. For troubleshooting only. |
| `uart_debug` | `false` | Logs the UART traffic with the display. For troubleshooting only. See [Hardware UART](hw_uart.md#configuration). |
<!-- markdownlint-enable MD013 -->

## Documented elsewhere

These are covered in the page of the feature they belong to:

<!-- markdownlint-disable MD013 -->
| Substitutions | Documentation |
| :- | :- |
| `BAUD_RATE`, `BAUD_RATE_TIMEOUT`, `BAUD_RATE_DELAY_AFTER_POWER_ON`, `BAUD_RATE_PROBE_MS` | [Hardware UART](hw_uart.md#configuration) |
| `GPIO_*` | [GPIO Pin Configuration](pins_customization.md#-available-gpio-keys) |
| `include_action_wake_up` | [API: Wake Up Action](api.md#wake-up-action-wake_up) |
| `bluetooth_proxy_active_scan` | [Add-on: Bluetooth Proxy](addon_bluetooth_proxy.md#settings) |
| Climate add-on, including the backup mode | [Add-on: Climate](addon_climate.md#configuration) |
| `cover_device_class`, `interlock_wait_time` | [Add-on: Cover](addon_cover.md#substitutions) |
| `espectre_*` | [Add-on: ESPectre](addon_espectre.md#configuration) |
| TFT upload add-on, including `include_action_upload_tft` and `include_button_scan_baud_rate` | [Add-on: Upload TFT](addon_upload_tft.md#configuration) |
<!-- markdownlint-enable MD013 -->
