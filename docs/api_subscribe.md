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
| `device_class` | Used only for the `cover` domain, to pick the right icon set |
| `inverted` | Render the component as visible while the entity is inactive |

Bindings are stored in the panel's NVS partition. Appearance — icons and colours — is **not**
stored, and arrives with every push.

### Reading an attribute instead of the state

A binding may name an `attribute`, in which case the panel subscribes to that attribute and the
entity's own state is never received. This is how the outdoor temperature reads `temperature` from a
weather entity, and how the indoor temperature reads `current_temperature` from a climate entity.

The attribute value reaches the renderer exactly where the state normally would, so classification
and appearance resolution behave as if it were the state. A missing attribute arrives as the literal
`None`, which classifies as unrecognised.

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
   `api_subscribe_end` carrying how many it sent. Appearance is applied immediately; entity
   bindings are staged.
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
| `api_subscribe_debounce` | `10s` | Quiet period after the last binding before a restart is applied. |
| `api_subscribe_timeout` | `120s` | Time without an end marker before the push is committed unverified. |

Lowering `api_subscribe_max` reduces the size of the staging buffer used during a push. It does not
reduce steady-state memory, which scales with the number of bindings actually configured.

## Action reference

### `api_subscribe`

Registers one binding. Called once per subscribable component.

| Parameter | Type | Description |
| --- | --- | --- |
| `page` | string | Target page. `chips` and `home` have renderers. |
| `component` | string | Target component, e.g. `chip01`. |
| `entity` | string | Home Assistant `entity_id`. |
| `attribute` | string | Attribute to follow instead of the state. Send an empty string to follow the state. |
| `device_class` | string | HA `device_class`. Only read for the `cover` domain; send an empty string otherwise. |
| `icon_on` | string | Icon codepoint for the active state. Empty resolves on-device. |
| `icon_off` | string | Icon codepoint for the inactive state. Empty resolves on-device. |
| `color_on` | int[] | RGB array for the active state, e.g. `[200, 204, 200]`. |
| `color_off` | int[] | RGB array for the inactive state. |
| `inverted` | bool | Render the component as visible while the entity is inactive. |

```yaml
action: esphome.my_panel_api_subscribe
data:
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
```

> [!IMPORTANT]
> A push replaces the complete set of bindings. Any component not included is unsubscribed.
> Calling `api_subscribe` on its own, outside a full push, will replace every existing binding.

### `api_subscribe_end`

Closes the push and states how many bindings were sent.

| Parameter | Type | Description |
| --- | --- | --- |
| `count` | int | Number of `api_subscribe` calls made in this push. |

```yaml
action: esphome.my_panel_api_subscribe_end
data:
  count: 7
```

If the count does not match what the panel received, the push is discarded and the previously
persisted bindings stay in effect. This prevents a truncated push from silently wiping bindings.

## Driving the panel without the Blueprint

The engine has no dependency on the Blueprint. Any automation, script, or external integration that
can call ESPHome actions can register bindings.

A minimal Home Assistant script that binds two chips and the home page weather picture:

```yaml
alias: NSPanel bindings
sequence:
  - action: esphome.my_panel_api_subscribe
    data:
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
  - action: esphome.my_panel_api_subscribe
    data:
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
  - action: esphome.my_panel_api_subscribe
    data:
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
  - action: esphome.my_panel_api_subscribe_end
    data:
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
icon override reaches the panel.

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

- **`chips` and `home` have renderers.** Bindings for other pages are accepted, persisted and
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
this session. Reload the automation, or re-run whatever registers your bindings.

The weather picture and the temperatures do not use pushed appearance, so a blank one means no value
has arrived. Set the log level to `VERBOSE` and look for the `nspanel.api.sub` line naming that
component: if it never appears, Home Assistant is not sending the state or attribute; if it appears
with a value that is not a number, the binding is following the wrong attribute.

If a binding shows `[no renderer]`, the target page has no renderer — see
[Limits and behaviour](#limits-and-behaviour).

### A component bound to an attribute shows nothing

Check that the entity really exposes that attribute, in Developer Tools → States. A missing
attribute arrives as the literal `None`: a chip or button will stay hidden, and a temperature will
be hidden or fall back to the panel's own sensor.

Note that a binding with an attribute never receives the entity's state, so `unavailable` arrives as
`None` rather than as itself.

### The panel restarts on every reconnect

The staged set is differing from the persisted one every time. Usually the Blueprint is sending
bindings in a different order between pushes, or a `device_class` is being sent for a non-cover
entity in one push and not another. Both count as a change.

### `Push incomplete` in the log

The end marker's count did not match how many bindings arrived. The push was discarded and the
previous bindings are still in effect. Check whether any `api_subscribe` call is failing validation
— an entity_id longer than 63 characters is rejected.

### `refusing to commit` in the log

Three consecutive pushes arrived without an end marker. Verify that the automation is calling
`api_subscribe_end` after the last binding. The panel keeps running its last good set until this is
resolved.

### `Could not persist` in the log

The bindings could not be written to NVS, so the panel is still running the set it loaded at boot
and your changes have not taken effect. The NVS partition can be full or fragmented; a factory reset of
the panel clears it. The entry counts in `dump_config` show whether the partition is genuinely too
small, which happens on panels first flashed with much older firmware — those need to be flashed
over USB once.
