# API Subscriptions

The API subscription engine lets the panel receive Home Assistant entity states directly over the
ESPHome API, instead of having an automation react to every state change and push the result to the
panel.

The Blueprint still decides *which* entity drives *which* component. Once that is registered, Home
Assistant streams state changes straight to the panel, and the panel renders them locally — no
automation run, no trace, no logbook entry.

## Summary

- [Why this exists](#why-this-exists)
- [How it works](#how-it-works)
- [Enabling the engine](#enabling-the-engine)
- [Action reference](#action-reference)
  - [`api_subscribe`](#api_subscribe)
  - [`api_subscribe_end`](#api_subscribe_end)
  - [Push identifier](#push-identifier)
- [Driving the panel without the Blueprint](#driving-the-panel-without-the-blueprint)
- [State classification](#state-classification)
- [Appearance resolution](#appearance-resolution)
- [Limits and behaviour](#limits-and-behaviour)
- [Troubleshooting](#troubleshooting)

## Why this exists

Without subscriptions, every state change of every entity shown on the panel triggers the Blueprint
automation. On a panel with a busy motion sensor and a handful of lights, that is thousands of
automation runs per day, each one producing a trace and competing with everything else Home
Assistant is doing.

With subscriptions, Home Assistant pushes the raw state string to the panel over the existing API
connection. The panel classifies it, resolves the icon and colour, and writes to the display. The
automation runs once per session, not once per state change.

## How it works

### Bindings

A **binding** connects one Home Assistant entity to one panel component. It consists of:

| Field | Purpose |
| --- | --- |
| `page` | Which page the target component belongs to, e.g. `chips` |
| `component` | Which component on that page, e.g. `chip01` |
| `entity` | The Home Assistant `entity_id` to follow |
| `attribute` | Read this attribute instead of the entity's state; empty to use the state |
| `device_class` | Picks the icon by domain, device class and state (see [Appearance resolution](#appearance-resolution)) |
| `inverted` | Render the component as visible while the entity is inactive |

Bindings are stored in the panel's NVS partition. Appearance — icons and colours — is **not**
stored, and arrives with every push.

### Reading an attribute instead of the state

A binding may name an `attribute`, in which case the panel subscribes to that attribute and the
entity's own state is never received. This is how the outdoor temperature reads `temperature` from a
weather entity, and how the indoor temperature reads `current_temperature` from a climate entity.

The attribute value reaches the renderer exactly where the state normally would, so classification
and appearance resolution behave as if it were the state. An attribute that exists but holds no value
arrives as the literal `None`, which classifies as unrecognised. An attribute that is missing from the
entity is not sent at all, so the panel keeps the last value it received. Climate entities typically
drop their attributes when they become unavailable; bind the entity's state as well where that matters,
as the temperature calibration does.

An entity may be bound twice — once for its state and once for an attribute — targeting different
components. The home page does this with a weather entity, which drives both the weather picture and
the outdoor temperature.

### Lifecycle

1. **Boot.** Before the API connects, the panel loads its persisted bindings and registers one
   Home Assistant state subscription per binding, or one for the named attribute where a binding
   has one. Climate bindings register a second subscription for the `hvac_action` attribute, unless
   they already name an attribute of their own.
2. **Connect.** Home Assistant delivers the current state of every subscribed
   entity immediately. The panel classifies each state, and renders once a
   binding push has supplied appearance.
3. **Push.** The Blueprint sends one `api_subscribe` call per component, then closes with
   `api_subscribe_end` carrying how many it sent. Every call carries the same `push_id`, so
   overlapping pushes are kept apart: only the newest one is staged. Appearance is applied
   immediately; entity bindings are staged.
4. **Commit.** If the staged set differs from the persisted one, the panel saves it and restarts.
   If it matches, nothing happens — which is the normal case on every reconnect.
   If the save fails, the panel does **not** restart: it keeps the bindings it loaded at boot.
   A restart on a set that was never saved would reload the old bindings and repeat indefinitely.
5. **Steady state.** Home Assistant pushes state changes. The panel renders them. No automation
   runs.

### Why a restart is required

ESPHome only sends subscription requests to Home Assistant during the burst that follows the
initial `SubscribeHomeAssistantStatesRequest`. Once that burst drains, `APIConnection` stops
sending, and a subscription registered later is stored but never transmitted.

The Blueprint can only push bindings *after* the API is connected, which is always after that burst
has drained. A new binding therefore cannot take effect in the session that registered it. The
panel persists the binding and restarts, so the next boot registers it in time.

This is a limitation of ESPHome, not a design choice. If ESPHome gains runtime subscriptions, both
the persistence layer and the restart become unnecessary.

## Enabling the engine

Add the package to your device YAML:

```yaml
packages:
  nspanel_easy:
    url: https://github.com/edwardtfn/NSPanel-Easy/
    ref: main
    files:
      - esphome/nspanel_esphome.yaml
      - esphome/nspanel_esphome_api_subscribe.yaml
    refresh: 300s
```

The package enables `api: homeassistant_states: true` automatically, which is required —
`subscribe_home_assistant_state()` does not exist without it.

### Options

| Substitution | Default | Description |
| --- | --- | --- |
| `api_subscribe_max` | `128` | Maximum number of bindings. Storage is allocated to the configured count, not to this limit. |
| `api_subscribe_entity_len` | `96` | Buffer size for an `entity_id`, null terminator included (`32` to `255`). A longer `entity_id` is not bound; the log reports the size to set. |
| `api_subscribe_debounce` | `10s` | Quiet period after the last binding before a restart is applied. |
| `api_subscribe_timeout` | `120s` | Time without an end marker before the push is committed unverified. |

Lowering `api_subscribe_max` reduces the size of the staging buffer used during a push. It does not
reduce steady-state memory, which scales with the number of bindings actually configured.

## Action reference

### `api_subscribe`

Registers one binding. Called once per subscribable component.

| Parameter | Type | Description |
| --- | --- | --- |
| `push_id` | string | Identifier of the push, the same for every call of one push, up to 23 characters. See [Push identifier](#push-identifier). |
| `page` | string | Target page. `chips`, `home`, `wakeup`, `calibration`, `buttonpage01` to `buttonpage04` and `climate` have renderers. |
| `component` | string | Target component, e.g. `chip01`. |
| `entity` | string | Home Assistant `entity_id`. |
| `attribute` | string | Attribute to follow instead of the state. Send an empty string to follow the state. |
| `device_class` | string | HA `device_class`, for the icon by domain, device class and state. Send an empty string when the entity has none. |
| `icon_on` | string | Icon codepoint for the active state. Empty resolves on-device. |
| `icon_off` | string | Icon codepoint for the inactive state. Empty resolves on-device. |
| `color_on` | int[] | RGB array for the active state, e.g. `[200, 204, 200]`. |
| `color_off` | int[] | RGB array for the inactive state. |
| `inverted` | bool | Render the component as visible while the entity is inactive. |
| `icon_name` | string | Icon by name, e.g. `mdi:lamp`, used when `icon_on` is empty: the panel works out the codepoint from the icon names (see [`api_subscribe_icon_names`](#api_subscribe_icon_names)). Send an empty string for none. |

```yaml
action: esphome.my_panel_api_subscribe
data:
  push_id: "20261103T170405123456"
  page: chips
  component: chip01
  entity: binary_sensor.corridor_motion
  attribute: ""
  device_class: ""
  icon_on: "\uE1B1"
  icon_off: ""
  color_on: [255, 193, 7]
  color_off: [92, 92, 92]
  inverted: false
  icon_name: ""
```

> [!IMPORTANT]
> A push replaces the complete set of bindings. Any component not included is unsubscribed.
> Calling `api_subscribe` on its own, outside a full push, will replace every existing binding.

### `api_subscribe_end`

Closes the push and states how many bindings were sent.

| Parameter | Type | Description |
| --- | --- | --- |
| `push_id` | string | Identifier of the push, as sent with its bindings. |
| `count` | int | Number of `api_subscribe` calls made in this push. |

```yaml
action: esphome.my_panel_api_subscribe_end
data:
  push_id: "20261103T170405123456"
  count: 7
```

If the count does not match what the panel received, the push is discarded and the previously
persisted bindings stay in effect. This prevents a truncated push from silently wiping bindings.

Bindings beyond `api_subscribe_max` are counted but not kept: the push still matches its end marker,
the bindings that fit are committed, and the log names the ones left out.

### `api_subscribe_icon_names`

Sends the MDI icon names, so the panel can turn an icon name (`icon_name`, or an entity's own `icon`
attribute on the button pages) into its codepoint. The names are listed in codepoint order: the name at
position *n* is the icon at `U+E000 + n`, so only the names travel. Sent in chunks once per session;
kept in PSRAM and never persisted, so it causes no restart.

| Parameter | Type | Description |
| --- | --- | --- |
| `start` | int | Position of the first name of the chunk. `0` starts a new list. |
| `names` | string | Comma-separated names of the chunk, without `mdi:`. |

The Blueprint sends its own `all_icon_names` list. Without it, icons given by name cannot be resolved,
and the panel falls back to the icon by domain, device class and state.

### Push identifier

The panel compares `push_id` values as strings to tell which push started last. A call with a newer
`push_id` replaces the push being staged, and calls with an older one are ignored, so bindings from
overlapping pushes are never saved together. Use a value that sorts by start time, such as a UTC
timestamp with a fixed number of digits, as the Blueprint does. Keep a non-digit character in it (the
`T` below): Home Assistant turns an all-digit template result into a number, and one this large cannot
be stored by the recorder.

```yaml
push_id: '{{ utcnow().strftime("%Y%m%dT%H%M%S%f") }}'
```

Compute it once per push, before the first `api_subscribe` call, and send the same value with every
call of that push, including `api_subscribe_end`. For one minute after a push ends, calls with an
older `push_id` are still ignored as late calls of an older push, so if Home Assistant's clock moves
backwards, a push sent within that minute is ignored.

## Driving the panel without the Blueprint

The engine has no dependency on the Blueprint. Any automation, script, or external integration that
can call ESPHome actions can register bindings.

A minimal Home Assistant script that binds two chips and the home page weather picture:

```yaml
alias: NSPanel bindings
sequence:
  - variables:
      push_id: '{{ utcnow().strftime("%Y%m%dT%H%M%S%f") }}'
  - action: esphome.my_panel_api_subscribe
    data:
      push_id: "{{ push_id }}"
      page: chips
      component: chip01
      entity: binary_sensor.front_door
      attribute: ""
      device_class: ""
      icon_on: "\uE18D"
      icon_off: "\uE18C"
      color_on: [255, 0, 0]
      color_off: [92, 92, 92]
      inverted: false
      icon_name: ""
  - action: esphome.my_panel_api_subscribe
    data:
      push_id: "{{ push_id }}"
      page: chips
      component: chip02
      entity: cover.garage_door
      attribute: ""
      device_class: garage
      icon_on: ""
      icon_off: ""
      color_on: [200, 204, 200]
      color_off: [92, 92, 92]
      inverted: false
      icon_name: ""
  - action: esphome.my_panel_api_subscribe
    data:
      push_id: "{{ push_id }}"
      page: home
      component: weather
      entity: weather.home
      attribute: ""
      device_class: ""
      icon_on: ""
      icon_off: ""
      color_on: [200, 204, 200]
      color_off: [92, 92, 92]
      inverted: false
      icon_name: ""
  - action: esphome.my_panel_api_subscribe_end
    data:
      push_id: "{{ push_id }}"
      count: 3
mode: single
```

Trigger it on `homeassistant_start` and on the panel's boot announcement, and the panel is fully
configured without the Blueprint.

### Resolving icons yourself

For domains where the appearance is the same across every visible state, the panel expects you to
supply `icon_on` and `icon_off`. Two rules matter:

- Resolve the icon for the state in which the component will be **shown**, not for the entity's
  current state. A motion sensor that happens to be idle when you push would otherwise freeze the
  "no motion" icon onto a chip that only ever appears when motion is detected.
- Send the codepoint, not the `mdi:` name. The
  [MDI Icons Cheatsheet](https://edwardtfn.github.io/NSPanel-Easy/icons/cheatsheet.html) lists
  every supported icon.

For `alarm_control_panel`, `climate`, `cover`, `lock` and `water_heater`, leave both icon fields
empty and the panel resolves them from the domain and state. See
[Appearance resolution](#appearance-resolution).

The weather picture and both temperature components ignore the icon and colour fields entirely: the
picture comes from a built-in condition table, and the temperatures are rendered as text.

## Button pages

With the Blueprint, the button pages (`buttonpage01` to `buttonpage04`) are drawn by the panel from
bindings, without an automation run per state change. Each button has one binding for its entity on
component `buttonNN`, and optional bindings on the same component with a role suffix, so every target
stays unique:

| Component | Binds | Used for |
| --- | --- | --- |
| `buttonNN` | The entity's state | Pressed background, icon and colour, availability, momentary flash |
| `buttonNN:v` | A value attribute (below), or `last_triggered` for automations and scripts | The value next to the icon, or the momentary flash of a run |
| `buttonNN:c` | `rgb_color` (lights) or `action` (humidifiers) | The icon colour |
| `buttonNN:s` | The State Indicator Entity's state | The pressed background, instead of the entity's own state |
| `buttonNN:i` | The entity's own `icon` attribute | Follows an icon that changes at runtime, resolved with the icon names |

The value attribute is `brightness` for lights, `percentage` for fans, `current_position` for covers,
`current_temperature` for climates and `current_humidity` for humidifiers.

The settings that are not bindings arrive once per session with `buttons_setup`, one call per page.
They are kept in PSRAM and never persisted, so they cause no restart:

| Parameter | Type | Description |
| --- | --- | --- |
| `page` | string | Button page, e.g. `buttonpage01`. |
| `page_label` | string | Page title; empty keeps the page's default. |
| `labels` | string[] | Button labels, in button order. |
| `state_values` | string[] | State Values, in button order; an empty string uses the default on/off logic. |
| `confirm` | int | Bit mask of the buttons asking for a confirmation (bit 0 = `button01`). |
| `icon_font` | int | Nextion font of the button icons. |
| `temperature_unit` | string | Unit shown after a climate's current temperature. |

## State classification

Every incoming state is classified into one of four categories, and visibility follows from the
category and the `inverted` flag.

| Category | Not inverted | Inverted |
| --- | --- | --- |
| Active | Visible | Hidden |
| Inactive | Hidden | Visible |
| Transitional | Visible | Visible |
| Unrecognised | Hidden | Hidden |

Transitional states are shown in both polarities so that movement is always surfaced. A garage door
chip shows an amber arrow for the whole travel, regardless of how it is configured.

Unrecognised states — including `unavailable`, `unknown` and `none` — hide the component in both
polarities. An entity that drops off the network never lights an inverted component by accident.

### Per-domain states

| Domain | Active | Transitional | Inactive |
| --- | --- | --- | --- |
| `alarm_control_panel` | `armed_home`, `armed_away`, `armed_night`, `armed_vacation`, `armed_custom_bypass`, `triggered` | `arming`, `pending`, `disarming` | `disarmed` |
| `climate` | `heat`, `heating`, `cool`, `cooling`, `heat_cool`, `dry`, `drying`, `fan`, `fan_only`, `auto` | — | `off`, `idle` |
| `cover` | `open` | `opening`, `closing` | `closed` |
| `lock` | `unlocked`, `open`, `jammed` | `locking`, `unlocking`, `opening` | `locked` |
| `water_heater` | `on`, `eco`, `electric`, `gas`, `heat_pump`, `high_demand`, `performance` | — | `off` |
| `input_number`, `number`, `sensor` | any other usable value | — | `off`, `false` |
| everything else | `on`, `true`, `1`, `active`, `home`, `playing` | — | `off`, `false`, `0`, `not_home`, `idle`, `standby`, `paused` |

`climate` bindings subscribe to both the state and the `hvac_action` attribute. When `hvac_action`
holds a usable value it takes precedence, so a thermostat set to `heat` but currently idle is
classified as inactive. A climate binding that names an attribute of its own — such as the indoor
temperature reading `current_temperature` — does not subscribe to `hvac_action`, because it wants
the attribute value rather than the action.

The weather picture and both temperature components do not use classification at all. They consume
the raw value: the picture looks the condition up in a table, and the temperatures parse a number.

## Appearance resolution

For five domains the icon and colour change across the visible state set, so the panel resolves
them locally. Send empty icon fields to use this.

| Domain | Resolved from |
| --- | --- |
| `alarm_control_panel` | State — a distinct shield icon and colour per armed, transitional and disarmed state |
| `climate` | `hvac_action` when usable, otherwise the hvac mode |
| `cover` | `device_class` and state — thirteen device classes, each with open, opening, closed and closing icons |
| `lock` | State — locked, unlocked and transitional |
| `water_heater` | Operation mode |

Sending a non-empty `icon_on` or `icon_off` overrides resolution for that state, which is how a user
icon override reaches the panel. An `icon_name` is used next, resolved with the icon names sent by
[`api_subscribe_icon_names`](#api_subscribe_icon_names).

Every other domain resolves its icon from built-in rules, a copy of the Blueprint's device class icons
and domain defaults: the icon of the device class for the actual state (for example a door open or
closed, a valve opening), then the domain default (for a `sun` entity, by day or by night). The rules
are part of the firmware, so they work before Home Assistant connects. The colour is `color_on` or
`color_off`, by how the state classifies.

Covers with no `device_class`, or with one the panel does not recognise, fall back to a generic
blinds icon in all four states.

### The home page weather picture

The picture is resolved from three things: the weather condition, whether the sun is above the
horizon, and the active theme. Conditions the panel does not recognise, along with `unknown` and
`unavailable`, resolve to a blank picture, so the component needs no visibility handling.

Sun elevation is computed on the panel from coordinates supplied once by the Blueprint and stored
across reboots. Until coordinates arrive, the panel treats 06:00 to 18:00 local time as daytime.

### The home page temperatures

Temperatures sourced from Home Assistant are rendered exactly as reported, with no unit conversion.
The number of decimal places follows the panel's compiled unit: none for Fahrenheit, one for
Celsius.

The outdoor temperature is hidden when its source has no usable number. The indoor temperature falls
back to the panel's own sensor instead, so that component is never blank. That sensor reads in
Celsius and is converted when the panel is compiled for Fahrenheit.

## Limits and behaviour

- **`chips`, `home`, `wakeup`, `calibration`, the button pages and `climate` have renderers.** `climate` draws the
  climate page custom buttons (components `button08` and `button09`, icon only). `wakeup` and `calibration` draw
  nothing: they feed the wake-up sensors and the temperature reference. A `calibration` binding
  targets component `temperature` for auto-calibration only, or `thermostat` to also make it the
  Climate add-on's thermostat temperature. Bindings for other pages are accepted, persisted and
  subscribed, but nothing is drawn and `dump_config` marks them `[no renderer]`.
- **`chip_relay1`, `chip_relay2` and `chip_climate` cannot be bound.** They are driven locally by
  the relay and embedded-thermostat logic and keep working without Home Assistant. Attempting to
  bind them logs a warning and is ignored.
- **Chips and custom buttons stay hidden after a boot until a binding push arrives.**
  Appearance is not persisted, so the panel knows a component's state before it
  knows what to draw. The weather picture and the temperatures are unaffected: they use no pushed
  appearance and render as soon as a value arrives.
- **A binding change costs one restart.** Appearance changes, inversion changes and repeated
  identical pushes do not.
- **Bindings over `api_subscribe_max` are left out, not the whole push.** The log names them; raise
  the limit if they matter.
- **The panel refuses to commit unverified pushes indefinitely.** After three consecutive pushes
  with no end marker, the panel keeps its last good set and logs an error rather than restarting in
  a loop.
- **Value entities are active whenever they hold a usable value.** An `input_number`, `number` or
  `sensor` state such as `23.4` or `heating` shows a chip or custom button bound to it. Only a
  boolean-like `off` or `false`, as reported by a template sensor rendering a condition, counts as
  inactive. The two temperature components parse the number directly rather than classifying it.

## Troubleshooting

### Nothing renders after binding

Check `dump_config` for the loaded bindings:

```text
[C][nspanel.api.sub]: Subscriptions
[C][nspanel.api.sub]:   Bindings: 3 of 128
[C][nspanel.api.sub]:   chips.chip01 <- binary_sensor.front_door
[C][nspanel.api.sub]:   home.weather <- weather.home
[C][nspanel.api.sub]:   home.outdoor_temp <- weather.home (temperature)
```

An attribute, where one is used, is shown in parentheses after the entity. This is how two bindings
on the same entity are told apart.

If a chip or custom button is listed but nothing draws, no binding push has supplied appearance in
this session. Restart the panel, or re-run whatever registers your bindings. Reloading an unchanged
automation does not push the bindings again.

The weather picture and the temperatures do not use pushed appearance, so a blank one means no value
has arrived. Set the log level to `VERBOSE` and look for the `nspanel.api.sub` line naming that
component: if it never appears, Home Assistant is not sending the state or attribute; if it appears
with a value that is not a number, the binding is following the wrong attribute.

If a binding shows `[no renderer]`, the target page has no renderer — see
[Limits and behaviour](#limits-and-behaviour).

### A component bound to an attribute shows nothing

Check that the entity really exposes that attribute, in Developer Tools → States. An attribute that
is missing from the entity is never sent, so the component shows nothing until it appears. An
attribute that exists but holds no value arrives as the literal `None`: a chip or button will stay
hidden, and a temperature will be hidden or fall back to the panel's own sensor.

Note that a binding with an attribute never receives the entity's state. An entity that becomes
`unavailable` usually drops its attributes, and since a missing attribute is not sent, the component
keeps its last value. Bind the entity's state as well where that matters, as the temperature
calibration does for a climate reference.

### The panel restarts on every reconnect

The staged set is differing from the persisted one every time. Usually the Blueprint is sending
bindings in a different order between pushes, or a `device_class` is being sent for a non-cover
entity in one push and not another. Both count as a change.

### `Push incomplete` in the log

The end marker's count did not match how many bindings arrived. The push was discarded and the
previous bindings are still in effect. Check whether any `api_subscribe` call is failing validation
— an entity_id longer than 63 characters is rejected.

### `binding(s) over the limit` in the log

The push held more bindings than `api_subscribe_max`. The ones that fit were committed; the log line
before it names each one left out. Raise `api_subscribe_max`, or bind fewer components.

### Checking the subscription traffic

Once a minute, while updates arrive, the panel logs how many Home Assistant sent, how many changed a
value, and the five busiest bindings, e.g.
`Updates in the last 60 s: 42 received, 30 changed; busiest: sensor.power (20), ...`. An entity that
updates very often costs traffic even while its page is hidden; bind a less chatty one if needed.

### `refusing to commit` in the log

Three consecutive pushes arrived without an end marker. Verify that the automation is calling
`api_subscribe_end` after the last binding. The panel keeps running its last good set until this is
resolved.

### `failed the integrity check` in the log

The bindings stored on the panel were incomplete or did not match their checksum, for example after a
power loss while they were being written. The panel discards them all, instead of using a mixed set,
and starts without bindings. The Blueprint sends them again once Home Assistant connects, and the
panel then stores them and restarts once.

### `Could not persist` in the log

The bindings could not be written to NVS, so the panel is still running the set it loaded at boot
and your changes have not taken effect. The NVS partition can be full or fragmented; a factory reset of
the panel clears it. The entry counts in `dump_config` show whether the partition is genuinely too
small, which happens on panels first flashed with much older firmware — those need to be flashed
over USB once.
