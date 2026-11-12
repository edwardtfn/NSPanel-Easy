# API

This document provides details on custom actions designed for integration with Home Assistant, including their usage, parameters, and examples.

## Summary

- [Action Documentation](#action-documentation)
  - [API Subscribe Action (`api_subscribe`)](#api-subscribe-actions-api_subscribe-api_subscribe_end):
    Binds a Home Assistant entity directly to a panel component, so state changes render without an automation run.
  - [API Subscribe End Action (`api_subscribe_end`)](#api-subscribe-actions-api_subscribe-api_subscribe_end): Closes a binding push and states how many bindings were sent.
  - [API Subscribe Icon Names Action (`api_subscribe_icon_names`)](api_subscribe.md#api_subscribe_icon_names):
    Sends the MDI icon names, so the panel resolves icons given by name.
  - [Buttons Setup Action (`buttons_setup`)](api_subscribe.md#button-pages): Sends the settings of a button page that the panel draws itself.
  - [Component Color Action (`component_color`)](#component-color-action-component_color): Changes the foreground color of a specified component on the display.
  - [Component Text List Action (`component_text_list`)](#component-text-list-action-component_text_list): Updates the text of a specified component on the display using a list of values.
  - [Component Value Action (`component_val`)](#component-value-action-component_val): Updates the value of a specified component on the display.
  - [Components Visibility Action (`components_visibility`)](#components-visibility-action-components_visibility): Hides or shows a specified component on the display.
  - [Entity Details Show Action (`entity_details_show`)](#entity-details-show-action-entity_details_show): Displays detailed information for a specific entity.
  - [Entity Snapshot Action (`entity_snapshot`)](#entity-snapshot-action-entity_snapshot):
    Sends the state and attributes of the entities on an entity details page, for the panel to render.
  - [Hardware Button State Indication Action (`hw_button_state`)](#hardware-button-state-indication-action-hw_button_state):
    Updates the visual state (on/off) of the left and right hardware button indicators on the panel.
  - [Icon Action (`icon`)](#icon-action-icon): Updates a chip or custom button's icon, color, and visibility.
  - [Notification Clear Action (`notification_clear`)](#notification-clear-action-notification_clear): Clears the current notification from the screen.
  - [Notification Show Action (`notification_show`)](#notification-show-action-notification_show): Displays a notification-message on the screen.
  - [QR Code Action (`qrcode`)](#qr-code-action-qrcode): Displays a QR code on the panel or updates the QR code information for local control.
  - [RTTTL Play Action (`rtttl_play`)](#rtttl-play-action-rtttl_play): Plays melodies encoded in the RTTTL format.
  - [Upload TFT Action (`upload_tft`)](#tft-file-update-action-upload_tft): Enables TFT file updates from a URL, requiring the "Upload TFT" add-on. Opt-in.
  - [Utilities Group Refresh Action (`utilities_group_refresh`)](#utilities-group-refresh-action-utilities_group_refresh): Updates utility group display values and direction indicators.
  - [Wake Up Action (`wake_up`)](#wake-up-action-wake_up): Activates the display from a screensaver or low-brightness state. Opt-in.
  - [Alarm Settings Page Action (`page_alarm`)](#alarm-settings-page-action-page_alarm): Updates the Alarm page with current state information.
- [Screen Components](#screen-components)
  - [Chips](#chips)
    - [User-defined Chips](#user-defined-chips)
    - [Relays Chips](#relays-chips)
    - [Climate Chip](#climate-chip)
  - [Home page - Custom Buttons](#home-page---custom-buttons)
  - [Home page - Values](#home-page---values)
  - [Entities Pages - Values](#entities-pages---values)

## Action Documentation

### General Guidance

In general, there’s no validation of parameters in an action call.
Please validate inputs on the caller side; otherwise this can crash the ESPHome side and likely restart the panel.

One example is colors. In almost all cases, an array of three unsigned integers between 0 and 255 is expected.
If you send anything different, the conversion to the RGB565 used by Nextion will crash.

#### Action naming and `<your_panel_name>`

All examples use `esphome.<your_panel_name>_<action>` as a placeholder.
The actual action name in Home Assistant is derived from your ESPHome device name, slugified (lowercase, spaces replaced with underscores).
How the name is derived depends on the Home Assistant version that discovered the device:

- **Newer Home Assistant versions** use the `friendly_name` substitution (e.g., `friendly_name: "Living Room Panel"` → `living_room_panel`).
- **Older Home Assistant versions** used the `name` substitution instead (e.g., `name: "livingroompanel"` → `livingroompanel`).

You can look up the action names available on your Home Assistant instance under
**Settings** > **Developer tools** > **Actions**, then search for your panel name.

### Table of Contents
<!-- markdownlint-disable MD013 -->
| Action ID | Action Name | Description |
| --------- | ----------- | ----------- |
| [`api_subscribe`](#api-subscribe-actions-api_subscribe-api_subscribe_end) | [API Subscribe Action](#api-subscribe-actions-api_subscribe-api_subscribe_end) | Binds a Home Assistant entity directly to a panel component, so state changes render without an automation run. |
| [`api_subscribe_end`](#api-subscribe-actions-api_subscribe-api_subscribe_end) | [API Subscribe End Action](#api-subscribe-actions-api_subscribe-api_subscribe_end) | Closes a binding push and states how many bindings were sent. |
| [`api_subscribe_icon_names`](api_subscribe.md#api_subscribe_icon_names) | [API Subscribe Icon Names Action](api_subscribe.md#api_subscribe_icon_names) | Sends the MDI icon names, so the panel resolves icons given by name. |
| [`buttons_setup`](api_subscribe.md#button-pages) | [Buttons Setup Action](api_subscribe.md#button-pages) | Sends the settings of a button page that the panel draws itself. |
| [`component_color`](#component-color-action-component_color) | [Component Color Action](#component-color-action-component_color) | Changes the foreground color of a specified component on the display. |
| [`component_text_list`](#component-text-list-action-component_text_list) | [Component Text List Action](#component-text-list-action-component_text_list) | Updates the text of a specified component on the display using a list of values. |
| [`component_val`](#component-value-action-component_val) | [Component Value Action](#component-value-action-component_val) | Updates the value of a specified component on the display. |
| [`components_visibility`](#components-visibility-action-components_visibility) | [Components Visibility Action](#components-visibility-action-components_visibility) | Hides or shows a specified component on the display. |
| [`entity_details_show`](#entity-details-show-action-entity_details_show) | [Entity Details Show Action](#entity-details-show-action-entity_details_show) | Displays detailed information for a specific entity. |
| [`entity_snapshot`](#entity-snapshot-action-entity_snapshot) | [Entity Snapshot Action](#entity-snapshot-action-entity_snapshot) | Sends the state and attributes of the entities on an entity details page, for the panel to render. |
| [`hw_button_state`](#hardware-button-state-indication-action-hw_button_state) | [Hardware Button State Indication Action](#hardware-button-state-indication-action-hw_button_state) | Updates the visual state (on/off) of the left and right hardware button indicators on the panel. |
| [`icon`](#icon-action-icon) | [Icon Action](#icon-action-icon) | Updates a chip or custom button's icon, color, and visibility. |
| [`notification_clear`](#notification-clear-action-notification_clear) | [Notification Clear Action](#notification-clear-action-notification_clear) | Clears the current notification from the screen. |
| [`notification_show`](#notification-show-action-notification_show) | [Notification Show Action](#notification-show-action-notification_show) | Displays a notification-message on the screen. |
| [`page_alarm`](#alarm-settings-page-action-page_alarm) | [Alarm Settings Page Action](#alarm-settings-page-action-page_alarm) | Updates the Alarm page with current state information. |
| [`qrcode`](#qr-code-action-qrcode) | [QR Code Action](#qr-code-action-qrcode) | Displays a QR code on the panel or updates the QR code information for local control. |
| [`rtttl_play`](#rtttl-play-action-rtttl_play) | [RTTTL Play Action](#rtttl-play-action-rtttl_play) | Plays melodies encoded in the RTTTL format. |
| [`upload_tft`](#tft-file-update-action-upload_tft) | [Upload TFT Action](#tft-file-update-action-upload_tft) | Enables TFT file updates from a URL, requiring the "Upload TFT" add-on. Opt-in. |
| [`utilities_group_refresh`](#utilities-group-refresh-action-utilities_group_refresh) | [Utilities Group Refresh Action](#utilities-group-refresh-action-utilities_group_refresh) | Updates utility group display values and direction indicators. |
| [`wake_up`](#wake-up-action-wake_up) | [Wake Up Action](#wake-up-action-wake_up) | Activates the display from a screensaver or low-brightness state. Opt-in. |
<!-- markdownlint-enable MD013 -->

### API Subscribe Actions: `api_subscribe`, `api_subscribe_end`

These two actions, with `api_subscribe_icon_names` and `buttons_setup`, register bindings between Home Assistant entities and panel components. Once
bound, Home Assistant streams state changes straight to the panel over the API connection and the
panel renders them locally — no automation runs per state change.

Unlike the other actions on this page, they configure an ongoing subscription rather than performing
a one-off update, and they carry lifecycle rules that do not fit this reference format: a push
replaces the complete set of bindings, must be closed with an end marker, and a change to any bound
entity requires a restart to take effect.

See [API Subscriptions](api_subscribe.md) for the full documentation, including parameters, state
classification, on-device appearance resolution, and troubleshooting.

### Component Color Action: `component_color`

Changes the foreground color of a specified component on the display, enabling dynamic color updates for user interface customization.

**Usage:**
This action is ideal for creating visually dynamic interfaces, allowing elements to change color based on conditions,
events, or user actions, such as indicating status changes or highlighting specific UI components.

**Parameters:**

- `page` (string): Identifier of the page where the component is.
    Use `mem` when setting memory vars or leave empty for current page or global vars.
- `id` (string): Identifier of the component whose color will be updated.
    It is essential that this matches the component's ID in your display layout to ensure the correct element is targeted.
- `color` (int[]): The new color for the component, specified as an RGB array (e.g., `[255, 0, 0]` for red).

> [!IMPORTANT]
> **Using `page: mem` for Memory Variables**
>
> The base implementation of `component_color` action does **not** include handling for `page: mem`.
> This is by design to support a modular architecture.
>
> To use `page: mem`, you must ensure that the appropriate extension file is included in your configuration.
> These extension files use ESPHome's `!extend` feature to add `page == "mem"` handling for their specific memory variables.
> For example:
>
> - `nspanel_esphome_version.yaml` extends these actions to handle version-related memory variables
> - `nspanel_esphome_page_utilities.yaml` extends these actions for utilities-specific memory variables
> - Other component files may add their own `mem` handlers
>
> **Before using `page: mem` in your automations:**
>
> 1. Verify that your ESPHome configuration includes the extension file that handles the specific memory variable you want to update
> 2. Check the component's documentation to confirm which memory variables are supported
> 3. If `page: mem` doesn't work as expected, ensure the required extension package is loaded in your configuration
>
> When an extension is not present, using `page: mem` will have no effect, and the action may be silently ignored.

**Home Assistant Example:**

```yaml
action: esphome.<your_panel_name>_component_color
data:
  page: home
  id: time
  color: [255, 0, 0]  # Changes the component's color to red
```

> [!NOTE]
> Replace `<your_panel_name>` with the slugified name of your panel (see [Action naming](#action-naming-and-your_panel_name)).
>
> Ensure the `id` and color parameters accurately target and define the new color for the component.

### Component Text List Action: `component_text_list`

Updates the text of a specified component on the display using a list of values, enabling dynamic text content updates.

Replaces the former `component_text` action. Single-value callers should pass a one-element list.

**Usage:**
Ideal for user interfaces that require real-time text updates, such as status messages, labels, or any text-based information display.
Also used by page modules that store list-shaped data in memory (e.g. the options list for the `popup_select` picker page).

**Parameters:**

- `page` (string): Identifier of the page where the component is. Use `mem` when setting memory vars or leave empty for current page or global vars.
- `id` (string): Identifier of the component whose text will be updated. Ensure this matches the component's ID in your display layout.
- `txt_list` (string[]): List of text values.
    For Nextion-targeted pages, only the last element of the list is applied (earlier elements are discarded since each write to the same component would be immediately overwritten).
    For `page: mem` targets handled by list-aware extensions, the full list is passed through.

> [!IMPORTANT]
> **Using `page: mem` for Memory Variables**
>
> The base implementation of `component_text_list` action does **not** include handling for `page: mem`.
> This is by design to support a modular architecture.
>
> To use `page: mem`, you must ensure that the appropriate extension file is included in your configuration.
> These extension files use ESPHome's `!extend` feature to add `page == "mem"` handling for their specific memory variables.
> For example:
>
> - `nspanel_esphome_version.yaml` extends these actions to handle version-related memory variables
> - `nspanel_esphome_page_utilities.yaml` extends these actions for utilities-specific memory variables
> - Other component files may add their own `mem` handlers
>
> Some `mem` targets are provided by the core packages and are therefore always available:
>
> - `command` — sends each non-empty element of `txt_list` to the display as a raw Nextion command,
>   in the order given (provided by `nspanel_esphome_hw_display.yaml`, part of `nspanel_esphome_core.yaml`).
>   This replaces the former `command` action, e.g. `txt_list: ["page home"]` opens the home page.
> - `details_overlap` — sets the title and icon of the next entity details page opened with
>   [`entity_details_show`](#entity-details-show-action-entity_details_show).
>   `txt_list` is `[entity, title, icon]`, where `entity` must match the entity passed to `entity_details_show`,
>   `title` is the page title and `icon` is an MDI icon name (`mdi:<name>`); leave `title` or `icon` blank to use the entity's own.
>   The panel does not interpret these values: it stores them as-is and reports them back to the Blueprint when the page opens,
>   and the Blueprint converts the icon to its glyph. The values are cleared when the panel leaves the entity details pages
>   (provided by `nspanel_esphome_api.yaml`, part of `nspanel_esphome_core.yaml`)
>
> **Before using `page: mem` in your automations:**
>
> 1. Verify that your ESPHome configuration includes the extension file that handles the specific memory variable you want to update
> 2. Check the component's documentation to confirm which memory variables are supported
> 3. If `page: mem` doesn't work as expected, ensure the required extension package is loaded in your configuration
>
> When an extension is not present, using `page: mem` will have no effect, and the action may be silently ignored.

**Home Assistant Example (single value, typical migration from `component_text`):**

```yaml
action: esphome.<your_panel_name>_component_text_list
data:
  page: home
  id: time
  txt_list:
    - "12:34"
```

**Home Assistant Example (list of values, for list-aware memory namespaces):**

```yaml
action: esphome.<your_panel_name>_component_text_list
data:
  page: mem
  id: popup_select_options
  txt_list:
    - "None"
    - "Rainbow"
    - "Pulse"
    - "Twinkle"
```

The `popup_select` picker accepts up to 32 options and shows them in pages of 8, navigated by swiping up or down on the list or with the arrow buttons below it. Options beyond the 32nd are discarded.

> [!NOTE]
> Replace `<your_panel_name>` with the slugified name of your panel (see [Action naming](#action-naming-and-your_panel_name)).
>
> Make sure the `id` corresponds to the correct component on your display (or the correct memory namespace key) for the text update to work as intended.

### Component Value Action: `component_val`

Updates the value of a specified component on the display, enabling dynamic value updates.

**Usage:**
Ideal for interfaces requiring real-time updates of numerical values, such as counters, temperature readings, or any numeric indicators.

**Parameters:**

- `page` (string): Identifier of the page where the component is. Use `mem` when setting memory vars or leave empty for current page or global vars.
- `id` (string): Identifier of the component whose value will be updated. It's crucial this matches the component's ID in your display layout accurately.
- `val` (int): The new integer value to be set for the component.
  This can represent various data types, depending on the component's purpose (e.g., temperature, humidity levels).

> [!IMPORTANT]
> **Using `page: mem` for Memory Variables**
>
> The base implementation of `component_val` action does **not** include handling for `page: mem`.
> This is by design to support a modular architecture.
>
> To use `page: mem`, you must ensure that the appropriate extension file is included in your configuration.
> These extension files use ESPHome's `!extend` feature to add `page == "mem"` handling for their specific memory variables.
> For example:
>
> - `nspanel_esphome_version.yaml` extends these actions to handle version-related memory variables
> - `nspanel_esphome_page_utilities.yaml` extends these actions for utilities-specific memory variables
> - Other component files may add their own `mem` handlers
>
> **Before using `page: mem` in your automations:**
>
> 1. Verify that your ESPHome configuration includes the extension file that handles the specific memory variable you want to update
> 2. Check the component's documentation to confirm which memory variables are supported
> 3. If `page: mem` doesn't work as expected, ensure the required extension package is loaded in your configuration
>
> When an extension is not present, using `page: mem` will have no effect, and the action may be silently ignored.

**Home Assistant Example:**

```yaml
action: esphome.<your_panel_name>_component_val
data:
  page: cover
  id: coverslider
  val: 25
```

> [!NOTE]
> Replace `<your_panel_name>` with the slugified name of your panel (see [Action naming](#action-naming-and-your_panel_name)).
>
> Ensure the `id` accurately matches the component on your display to successfully update its value.

### Components Visibility Action: `components_visibility`

Hides or shows a list of components on the display, allowing for dynamic interface changes.

**Usage:**
This action is ideal for creating interactive user interfaces that adapt by hiding or showing certain elements based on user actions, conditions, or events.

**Parameters:**

- `page` (string): Identifier of the page where the component is. Use `mem` when setting memory vars or leave empty for current page or global vars.
- `ids` (string[]): Array of identifiers of the components to be hidden/shown.
  It is crucial that this matches the component's ID in your display layout to ensure the correct element is hidden/shown.
- `visible` (bool): Set to true to show the component, or false to hide it.

**Home Assistant Example:**

```yaml
action: esphome.<your_panel_name>_components_visibility
data:
  page: home
  ids: [ "date", "time" ]  # Hides the date and time display on Home page
  visible: false
```
<!-- markdownlint-disable MD028 -->
> [!NOTE]
> Replace `<your_panel_name>` with the slugified name of your panel (see [Action naming](#action-naming-and-your_panel_name)).
>
> Ensure the ids match the components on your display you wish to hide or show.

> [!IMPORTANT]
> This command only works when the target page is visible.
>
> If a component being hidden/shown is not part of the current page, the command will fail and an error message will be logged.
<!-- markdownlint-enable MD028 -->

### Entity Details Show Action: `entity_details_show`

This action is designed to display detailed information about a specific entity within the panel's interface.
It enables users to navigate to a dedicated page showing extensive details about an entity, such as a light or a climate,
and provides a structured way to return to either the home page or a specific button page.

**Usage:**
Ideal for interfaces requiring detailed entity information across various contexts.
By specifying the `entity` and `back_page`, users are offered a seamless navigation experience,
ensuring they can easily access detailed information and return to their initial navigation point within the interface.

**Parameters:**

- `entity` (string): The unique identifier of the entity (`entity_id` in Home Assistant) for which details are displayed.
  The domain is used to select the corresponding detail page.
  Supported domains are:
  - `alarm_control_panel`
  - `button`
  - `climate`
  - `cover`
  - `fan`
  - `humidifier` (shown on the water heater page)
  - `input_boolean`
  - `input_button`
  - `light`
  - `media_player`
  - `switch`
  - `water_heater`

  Use the special value `embedded_climate` to open the built-in climate control page for the panel's relay-based thermostat.
  If the details page for that domain is already shown for a different entity, the page is reloaded with the new entity.
  Requests for the entity already shown are ignored.
- `back_page` (string): The page identifier to return to after viewing entity details.
  Valid options are `home` for the home page or `buttonpage01` to `buttonpage04` for button pages.
  No other pages are supported to maintain navigation consistency.
  Leave it empty to keep the current return target unchanged.

**Home Assistant Example:**

```yaml
action: esphome.<your_panel_name>_entity_details_show
data:
  entity: "light.living_room"
  back_page: "buttonpage01"
```

To show a custom title or icon on the details page, set them with `component_text_list` right before calling this action:

```yaml
action: esphome.<your_panel_name>_component_text_list
data:
  page: mem
  id: details_overlap
  txt_list:
    - "light.living_room"
    - "Reading light"
    - "mdi:floor-lamp"
```

<!-- markdownlint-disable MD028 -->
> [!NOTE]
> The title and icon are rendered by the NSPanel Easy Blueprint when the page opens, so pass the icon as an MDI name (`mdi:<name>`), not as a glyph.
> Without the Blueprint, or when the entity does not match, the entity's own name and icon are used.

> [!NOTE]
> Replace `<your_panel_name>` with the slugified name of your panel (see [Action naming](#action-naming-and-your_panel_name)).
> This setup provides a direct and user-friendly way to access and return from detailed entity information, enhancing the interface's usability.
<!-- markdownlint-enable MD028 -->

### Entity Snapshot Action: `entity_snapshot`

Sends the state and attributes of the entities shown on an entity details page, and the panel renders the page from them.
The NSPanel Easy Blueprint uses it for the alarm, button, climate, cover, fan, light, media player, select, switch, timer, vacuum / lawn mower and water heater (also humidifier) pages;
the other entity details pages still receive each text and value from the Blueprint.

**Usage:**
Meant for the Blueprint, which sends a snapshot when an entity details page opens and whenever one of its entities changes.

**Parameters:**

- `snapshot` (string): JSON with the page and its entities:
  - `page` (string): page the snapshot is for, e.g. `select`.
  - `constructor` (boolean): `true` for the first snapshot after the page opens, which also sets the parts that do not change while it is open.
  - `compact_unit_display` (boolean, optional): units follow values without a space, e.g. `50%` instead of `50 %`.
  - `settled` (boolean, optional): the snapshot is sent after the entity went quiet, so pending presses on the page are cleared.
  - `entities` (list): one object per entity, with:
    - `role` (string): role of the entity on the page; `main` for the entity the page shows, `battery` for the device's battery sensor on the vacuum page.
    - `entity_id` (string), `exists` (boolean), `state` (string) and `name` (string, the entity's friendly name).
    - `attributes` (object): the attributes the page needs, e.g. `options` for the select page, or `supported_features`,
      `brightness` and the colour temperature range for the light page; `icon` and `device_class` for the title icon.

  Snapshots larger than 8 KB are ignored, and so are snapshots for a page that is no longer shown.

**Home Assistant Example:**

```yaml
action: esphome.<your_panel_name>_entity_snapshot
data:
  snapshot: >-
    {"page": "select", "constructor": true, "entities": [{"role": "main", "entity_id": "input_select.mode",
    "exists": true, "state": "Auto", "name": "Mode", "attributes": {"options": ["Auto", "Manual"]}}]}
```

> [!NOTE]
> Replace `<your_panel_name>` with the slugified name of your panel (see [Action naming](#action-naming-and-your_panel_name)).

### Hardware Button State Indication Action: `hw_button_state`

This action dynamically updates the on-screen indication bars for the hardware buttons, reflecting the current state of the entities they control.
It's designed to provide immediate visual feedback,
enhancing the user interface by showing the active/inactive state of the left and right hardware button indicators on the panel.

**Usage:**
Utilize this action to modify the visual state (on/off) of hardware button indicators on the panel, corresponding to the state of entities controlled by these buttons.
This allows for visual feedback that matches the operational state of the buttons.

**Parameters:**

- `button_mask` (int): A bitwise identifier for buttons. Use `1` for the left button, `2` for the right button, and `3` for both buttons.
- `state` (bool): The state to apply to the button(s) indicated by `button_mask`. True for on (active), false for off (inactive).

**Home Assistant Example:**

```yaml
action: esphome.<your_panel_name>_hw_button_state
data:
  button_mask: 3  # Targets both the left (1) and right (2) buttons
  state: true     # Turns the indication bars on for both buttons
```

> [!NOTE]
> Replace `<your_panel_name>` with the slugified name of your panel (see [Action naming](#action-naming-and-your_panel_name)).
> This action leverages a bitmask (`button_mask`) for flexible control over multiple hardware buttons simultaneously,
> offering a streamlined method for updating their visual states.

### Icon Action: `icon`

Updates a chip or custom button's icon, color, and visibility within Home Assistant.

**Usage:**
This action is ideal for dynamically updating icons on your Panel, allowing for a customizable and interactive user interface.

**Parameters:**

- `page` (string): Use `chips` for chip components or use the page name for other components.
- `id` (string): Identifier of the chip or button component. Refer to "[Screen components](#screen-components)" for more details.
- `icon` (string): Icon codepoint from
  [HASwitchPlate Material Design Icons](https://htmlpreview.github.io/?https://github.com/jobr99/Generate-HASP-Fonts/blob/master/cheatsheet.html).
Example: "\uE6E8" for `mdi:lightbulb-on-outline`.
- `icon_color` (int[]): RGB color array for the icon. Example: `[0, 255, 0]` for green.
- `visible` (bool): Flag indicating whether the icon should be visible (`true`) or hidden (`false`).

**Home Assistant Example:**

```yaml
action: esphome.<your_panel_name>_icon
data:
  page: chips
  id: chip03
  icon: "\uE6E8"           # Example for mdi:lightbulb-on-outline
  icon_color: [0, 255, 0]  # Green
  visible: true
```

> [!NOTE]
> Replace `<your_panel_name>` with the slugified name of your panel (see [Action naming](#action-naming-and-your_panel_name)).

### Notification Clear Action: `notification_clear`

Removes any displayed notification from the screen, allowing the display to return to its normal state or view.

**Usage:**
This action is essential after displaying notifications or alerts.
It ensures the user interface remains clean and uncluttered by clearing messages once they are no longer needed or have been acknowledged.

**Home Assistant Example:**

```yaml
action: esphome.<your_panel_name>_notification_clear
```

> [!NOTE]
> Replace `<your_panel_name>` with the slugified name of your panel (see [Action naming](#action-naming-and-your_panel_name)).
This simple action clears the current notification from the display, maintaining a tidy interface.

### Notification Show Action: `notification_show`

Displays a notification message on the screen, enabling dynamic presentation of information or alerts.

**Usage:**
Designed for scenarios requiring immediate feedback or notification on the display,
this action is suitable for showing alerts, informational messages, or updates directly on the screen interface.

**Parameters:**

- `label` (string): A brief title or label for the notification, typically displayed in a prominent, bold format.
- `message` (string): The detailed message or content of the notification, providing the main information or alert to the user.
The system will automatically wrap text to fit the display unless `\r` is used to insert manual line breaks. When `\r` is present,
the system respects only the line breaks provided by the user, enhancing message formatting control.

**Home Assistant Example:**

```yaml
action: esphome.<your_panel_name>_notification_show
data:
  label: "Security Alert"
  message: "Front door opened at 10:30 PM\rPlease check the entrance."
```

> [!NOTE]
> Replace `<your_panel_name>` with the slugified name of your panel (see [Action naming](#action-naming-and-your_panel_name)).
> This ensures the action executes correctly, displaying the notification with the specified `label` and `message`.
>
> Utilize `\r` within the message for custom line breaks, offering precise formatting control.

### Alarm Settings Page Action: `page_alarm`

Populates the alarm settings page with the current configuration and state information,
seamlessly integrating with the panel's interface to reflect the latest settings and statuses of the alarm system.

**Usage:**
The `page_alarm` action is specifically designed to dynamically update the alarm settings page,
providing users with immediate access to the alarm system's controls and information.
It plays a crucial role in ensuring a responsive and user-friendly interface for alarm management,
allowing for real-time interaction with the alarm system directly from the panel.

**Parameters:**

- `state` (string): The current state of the alarm system, such as "armed_home" or "disarmed".
- `supported_features` (int): A bitmask indicating the features supported by the alarm system.
This determines the controls and options available on the alarm settings page.
The bitmask values are aligned with the
[Home Assistant Alarm Control Panel Supported Features](#supported-features-of-the-alarm-control-panel-entity).
- `code_format` (string): Specifies the code format required for arming/disarming the alarm system, such as numeric or alphanumeric.
- `code_arm_required` (bool): Indicates whether a code is required to arm the system.
- `entity` (string): The entity ID of the alarm system, used for state updates and control actions.

**Home Assistant Example:**

```yaml
action: esphome.<your_panel_name>_page_alarm
data:
  state: "disarmed"
  supported_features: 31  # Example: Supports arm/disarm, home/away modes, etc.
  code_format: "number"
  code_arm_required: true
  entity: "alarm_control_panel.home_alarm"
```

> [!NOTE]
> Replace `<your_panel_name>` with the slugified name of your panel (see [Action naming](#action-naming-and-your_panel_name)).
> This action configuration allows for the alarm settings page to dynamically reflect the current features, state,
> and control options of your alarm system, enhancing the overall user experience.

#### Supported features of the alarm control panel entity

- ARM_HOME = 1
- ARM_AWAY = 2
- ARM_NIGHT = 4
- TRIGGER = 8
- ARM_CUSTOM_BYPASS = 16
- ARM_VACATION = 32

### QR Code Action: `qrcode`

Displays a QR code on the display, which can be used for various purposes such as sharing a WiFi password or linking to a website.

**Usage:**
This action enables the dynamic display of QR codes on a specified page of the ESPHome user interface, allowing for the convenient sharing of information.
It's particularly useful for settings where quick, scannable access to data is beneficial.

**Parameters:**

- `title` (string): The title or heading to display above the QR code, providing context or instructions.
- `qrcode` (string): The actual data or URL to be encoded into the QR code.
- `show` (bool): A flag that determines whether to immediately display the QR code page upon calling the action.

**Home Assistant Example:**

```yaml
action: esphome.<your_panel_name>_qrcode
data:
  title: "Wi-Fi Access"
  qrcode: "WIFI:T:WPA;S:mynetwork;P:mypass;;"
  show: true
```
<!-- markdownlint-disable MD028 -->
> [!NOTE]
> Replace `<your_panel_name>` with the slugified name of your panel (see [Action naming](#action-naming-and-your_panel_name)).
> This action will generate and display the QR code based on the provided data, navigating to the QR code page if `show` is set to `true`.

> [!NOTE]
> This action is typically invoked during initialization to preload QR Code information based on blueprint settings,
> ensuring the data is accessible on the panel even without Wi-Fi connectivity.
> To update the QR Code information without immediately displaying it, call this action with `show: false`.
> This allows for seamless updates to the QR Code content without disrupting the current user interface.
<!-- markdownlint-enable MD028 -->

### RTTTL Play Action: `rtttl_play`

Plays melodies encoded in the RTTTL format, enabling the integration of audio feedback or alerts with simple text-based melody strings.

**Usage:**
This action is perfect for projects requiring audio signals, such as notifications, alerts, or simple melodies,
by interpreting RTTTL (Ring Tone Text Transfer Language) strings.
RTTTL is a compact format for storing melody sequences in a text-based format, making it ideal for simple audio devices like buzzers.

**Parameters:**

- `tone` (string): The RTTTL string representing the melody to be played.
This string must conform to the RTTTL format, which includes the melody's name, default settings, and a sequence of notes.

**Example Tones:**
For example tones and further inspiration, you can visit this list with [examples of RTTTL songs](https://esphome.io/components/rtttl/#common-beeps).

**Home Assistant Example:**

```yaml
action: esphome.<your_panel_name>_rtttl_play
data:
  tone: "The Simpsons:d=4,o=5,b=160:c.6,e6,f#6,8a6,g.6,e6,c6,8a,8f#,8f#,8f#,2g,8p,8p,8f#,8f#,8f#,8g,a#.,8c6,8c6,8c6,c6"
```

> [!NOTE]
> Replace `<your_panel_name>` with the slugified name of your panel (see [Action naming](#action-naming-and-your_panel_name)).
>
> Ensure the `tone` parameter contains a valid RTTTL string to successfully play the melody.

### TFT File Update Action: `upload_tft`

> [!IMPORTANT]
> This action is not registered by default. To enable it, add `include_action_upload_tft: true` to the substitutions in your ESPHome YAML.
> See [Reducing Registered API Actions](install.md#reducing-registered-api-actions) for details.

Enables the remote update of the panel's TFT file from a specified URL or a default location,
available exclusively with the "Upload TFT" add-on installed.
This action is valuable for downloading alternative TFT files for customization or addressing file access issues.

**Usage:**
This action is crucial for dynamically updating the TFT file, facilitating seamless transitions between different configurations or updates.
It's particularly useful for applying custom interface designs or updates when direct access to the repository is limited.

**Parameters:**

- `url` (string): The URL for downloading the TFT file.
If set to "default", it utilizes the URL from the "**Update TFT - Display Model**" selection in Home Assistant (**Settings** > **Devices & Actions** > **ESPHome**).

**Home Assistant Example:**

```yaml
action: esphome.<your_panel_name>_upload_tft
data:
  url: "http://homeassistant.local:8123/local/custom_tft_file.tft"  # URL to the new TFT file
```
<!-- markdownlint-disable MD028 -->
> [!NOTE]
> Replace `<your_panel_name>` with the slugified name of your panel (see [Action naming](#action-naming-and-your_panel_name)).
> Using "default" fetches the URL associated with the selected display model in Home Assistant settings, simplifying updates or customizations.

> [!WARNING]
> The "Upload TFT" add-on must be installed for this action to be available, enhancing the panel's flexibility for interface customization or troubleshooting.
<!-- markdownlint-enable MD028 -->

### Utilities Group Refresh Action: `utilities_group_refresh`

Updates utility group display values and direction indicators on the panel. This action is used to refresh the displayed values for utility groups dynamically.

**Usage:**
This action is particularly useful for updating display elements associated with utility groups, such as values and directional indicators, based on specified parameters.

**Parameters:**

- `group_id` (string): The unique identifier for the utility group.
- `value1` (string): The first value to be displayed for the group.
- `value2` (string): The second value to be displayed for the group.
- `direction` (int): The direction indicator value for the group.
- `constructor` (bool): A flag to indicate if this is a constructor call, which forces updates regardless of current values.

**Home Assistant Example:**

```yaml
action: esphome.<your_panel_name>_utilities_group_refresh
data:
  group_id: "home"
  value1: "100 kWh"
  value2: "26 °C"
  direction: 1
  constructor: false
```

> [!NOTE]
> Replace `<your_panel_name>` with the slugified name of your panel (see [Action naming](#action-naming-and-your_panel_name)).

This action updates utility group display elements with the specified values and direction indicators dynamically.

### Wake Up Action: `wake_up`

> [!IMPORTANT]
> This action is not registered by default. To enable it, add `include_action_wake_up: true` to the substitutions in your ESPHome YAML.
> See [Reducing Registered API Actions](install.md#reducing-registered-api-actions) for details.

Activates the display from a screensaver or low-power state, enabling dynamic interface adjustments based on user interactions or automated triggers.

**Usage:**
Ideal for scenarios requiring the display to become active upon certain events,
such as motion detection, thereby conserving energy while ensuring the display is available when needed.

**Parameters:**

- `reset_timer` (bool): Determines whether to reset the sleep and dimming timers upon waking up the display.
Setting this to `true` ensures the display remains active during user presence, while `false` retains the existing timer durations.

**Home Assistant Example:**

```yaml
action: esphome.<your_panel_name>_wake_up
data:
  reset_timer: true
```

> [!NOTE]
> Replace `<your_panel_name>` with the slugified name of your panel (see [Action naming](#action-naming-and-your_panel_name)).
> This ensures the action executes correctly,
> waking the display and optionally resetting timers based on the `reset_timer` parameter.

**Practical Use Case: Motion Sensor Activation:**
This action can be seamlessly integrated with a motion sensor to wake the display when motion is detected, making it instantly usable.
Additionally, if the display is already awake, calling this action with `reset_timer: true` can reset the sleep timer,
keeping the display active as long as there is movement and allowing it to sleep normally once no motion is detected.

The example below integrates the `wake_up` action with a motion sensor to ensure the display wakes or remains awake during periods of activity,
reverting to sleep mode after inactivity.

```yaml
automation:
  - alias: "Wake Display on Motion"
    trigger:
      - platform: state
        entity_id: binary_sensor.motion_sensor_123
        to: 'on'
    action:
      - action: esphome.<your_panel_name>_wake_up
        data:
          reset_timer: true
    mode: restart
```

> [!NOTE]
> Adjust `<your_panel_name>` and `binary_sensor.motion_sensor_123` to your actual panel and sensor entity IDs.
> This setup ensures the display is responsive to environmental conditions, enhancing user interaction while managing energy consumption efficiently.

## Screen components

### Chips

![Image](pics/Nextion_Components_Home_Chips_EU.png)
![Image](pics/Nextion_Components_Home_Chips_US.png)

#### User-defined Chips

- **Description**: Chips are status indicator icons shown on the home page and screensaver. Their behaviour is controlled by the blueprint.
- **Alternative**: User-defined chips can be driven directly from Home Assistant entity states instead of the blueprint. See [API Subscriptions](api_subscribe.md).
- **Availability**: Rendered on home page and screensaver. Use `page: chips` in the `icon` action to update them from any page context.
- **Ids**: `chip01` to `chip07`.

#### Relays Chips

- **Description**: Icons representing each of the relay states.
- **Availability**: Rendered on home page and screensaver.
- **Ids**: `chip_relay1` and `chip_relay2`.

#### Climate Chip

- **Description**: Icon representing the state of the main climate entity.
- **Availability**: Rendered on home page and screensaver.
- **Ids**: `chip_climate`.

### Home Page - Custom buttons

![Image](pics/Nextion_Components_Home_Custom_Buttons_EU.png)
![Image](pics/Nextion_Components_Home_Custom_Buttons_US.png)

### Home Page - Values

![Image](pics/Nextion_Components_Home_Values_EU.png)
![Image](pics/Nextion_Components_Home_Values_US.png)
This is a multi-component system, with names `value01` to `value04` containing the state of the entity,
where `value01_icon` to `value04_icon` supports the icons.

#### Driving a home page value from your own automation

A value slot that has no entity selected in the Blueprint ("Home page - Entity State") is not managed by it,
so an automation can write its text, its icon and the icon color.
For example, to show the next waste collection in the first slot:

```yaml
# Text
- action: esphome.<your_panel_name>_component_text_list
  data:
    page: home
    id: value01
    txt_list:
      - "Paper"

# Icon and its color
- action: esphome.<your_panel_name>_icon
  data:
    page: home
    id: value01_icon
    icon: "\uE394"           # mdi:newspaper
    icon_color: [0, 120, 255]
    visible: true
```

To show nothing, send an empty text to both components:

```yaml
- action: esphome.<your_panel_name>_component_text_list
  data:
    page: home
    id: value01
    txt_list:
      - ""
- action: esphome.<your_panel_name>_component_text_list
  data:
    page: home
    id: value01_icon
    txt_list:
      - ""
```

The color of the text can be changed with the [`component_color`](#component-color-action-component_color) action (`page: home`, `id: value01`).

> [!NOTE]
> The panel does not keep these values: they are cleared when the panel restarts and when the Blueprint automation is saved or reloaded.
> Make your automation send them again in those cases, for example with a trigger for when the panel becomes available.

### Entities Pages - Values

Just like in "[Home Page - Values](#home-page---values)", this is a multi-component system, with names `value01` to `value08` containing the state of the entity,
where `value01_icon` to `value08_icon` supports the icons and, exclusively in the Entities pages, `value01_label` to `value08_label`,
which will contain the friendly name or some alternative label for the entities.

The texts can be written with the [`component_text_list`](#component-text-list-action-component_text_list) action, and the icons with the [`icon`](#icon-action-icon) action.
