# Panel startup issues

When your panel starts, a number of different things need to be set
and ESPHome must establish a connection to the Nextion display used by your panel
and also to your Home Assistant, which will provide the proper settings required by your panel.
If everything goes right, the screen shows a dimmed "NSPanel Easy" for a few seconds, until ESPHome connects to the display, and then the wake-up page is shown.
Until Home Assistant connects, the values from Home Assistant are empty or greyed out, and the connection icon is grey.
They appear as soon as each one is available, and the connection icon turns red only if the boot completes without Home Assistant.

The boot page is only shown when ESPHome does not connect to the display within about 20 seconds,
or while the display has an older TFT file that has to be updated first.
In many cases, restarting your panel can resolve the issue, but if it is still showing the boot page after a reboot, this guide may help you.

The same page is also the panel's status page: press and hold the background of the **Settings** page for a few seconds to open it at any time,
for example to check the connections or the versions before asking for help.
When opened this way, it behaves like the other pages: use the back button (top right) to leave it,
and it is closed by the page timeout and the sleep timeout as any other page.

Understanding what is shown on the boot page can be useful to find the cause of the problem or when you are looking for help.

## Understanding the boot page

Your boot page probably looks like this:

**Before Home Assistant/ESPHome is connected:**

![Boot page Initializing](pics/eu_boot_initializing.png)

**When Home Assistant/ESPHome is connected:**

![Boot page with IP](pics/eu_boot_with_ip_address.png)

> [!NOTE]
> These pictures show an older version of the page: the log area is replaced by the status area described below.

### What is shown on the screen

#### Versions

Here you may see the version of the TFT, of the firmware and of the Blueprint used by this project.

If some of those versions are not shown, you can have an indication of where a possible communication issue is.

- **TFT** version will always be shown, as that is part of the display itself.
- **Firmware** version will be shown as soon as ESPHome establishes the communication with the display.
- **Blueprint** version requires both ESPHome and the Blueprint (in Home Assistant) to establish communication to be shown.

In normal situations, all the versions should be displayed and should be the same.

#### System

- **IP**: the IP address of the panel, once ESPHome is connected to both the display and the Wi-Fi network.
  "Wi-Fi: Not connected" means that ESPHome is connected to the display, but not to the Wi-Fi network.
- **Framework**: the framework used by ESPHome, which is either `esp-idf` (default for new installations) or `arduino` (default for installations prior v4.1.4).
  If you cannot see that information after a couple of seconds, you may have an issue on the communication between ESPHome and the display (explained later).
- **Baud rate**: the baud rate used by the display to communicate to ESPHome.
  If the display does not respond at the configured rate (default 921600 bps) within a couple of minutes of boot,
  the panel will run a baud scan across the supported Nextion rates.
  This typically completes within seconds for known-good displays.

#### Status

What the display knows by itself, updated every second while the page is shown:

| Line | Meaning |
| :-- | :-- |
| `ESPHome` | Whether ESPHome connected to the display since it started. |
| `HA` | Whether the panel is connected to Home Assistant. |
| `Model` | The display model in use and, in brackets, the default of the TFT file (1 = EU, 2 = US, 3 = US Landscape, 5 = US Landscape, buttons left). `Ack`: 1 once acknowledged. |
| `Pages` | The ids of the home page, of the wake-up page and of the page the back buttons return to. |
| `Brightness` | The regular, dimmed and sleep brightness, in %. |
| `Backlight` | The brightness of the display right now, in %. `Recmod` is the serial mode of the display (0 is expected). |
| `Thermostat` | 1 when the climate page shows the embedded thermostat. `Home` is the visibility of the home page elements. |
| `Button pages` / `Entity pages` | 1 for each button page and entity page that is enabled. |

This information is useful when asking for help, so please include a picture of this page.

#### Reboot button

You can use this button to force a reboot of your panel.
This button is available when ESPHome establishes communication with the display.

#### Back button

Only shown when ESPHome is connected to the display. It returns to the home page.

## Common issues

### ESPHome cannot establish communication to your panel's Nextion display

**Symptoms:** The ESPHome version (and other info) won't be shown in the boot page.

<!-- markdownlint-disable MD013 MD033 -->
| Possible causes | Suggestions |
| :-- | :-- |
| You may have an older version of ESPHome installed or ESPHome is not installed. | Make sure you have the latest version of ESPHome and flash your device again. |
| Baud rate mismatch. | The default baud rate for this project is 921600 bps. The panel will automatically scan all supported rates if the display does not respond within a couple of minutes of boot. If the scan does not resolve the issue, check that your display firmware was built for this project. |
<!-- markdownlint-enable MD013 MD033 -->

### Blueprint is not detected

**Symptoms:** The ESPHome version is shown, but not the blueprint version or, the Home page is shown, but the Home Assistant icon is shown in red and typically the functionality is very limited:

![Blueprint icon on red](pics/us_home_blueprint_icon_red.png)

In some cases you can also see duplicate entities on the device's page in Home Assistant, and some of these entities may have the `entity_id` ending with `_2`, `_3`, etc.

![ha_device_sensors_duplicated.png](pics/ha_device_sensors_duplicated.png)

The panel requests its settings from the Blueprint when it starts, when its display restarts, and when the Blueprint automation is saved or reloaded (or Home Assistant starts).
If the settings do not arrive completely, the panel requests them again a few times, waiting longer each time.
When it gives up, the panel keeps working with the settings it has, and its log shows `Blueprint settings incomplete after 5 requests`.

<!-- markdownlint-disable MD013 MD033 -->
| Possible causes | Suggestions |
| :-- | :-- |
| You may have an older version of the blueprint installed or the blueprint isn't installed into your Home Assistant. | [Install the blueprint](https://my.home-assistant.io/redirect/blueprint_import/?blueprint_url=https%3A%2F%2Fraw.githubusercontent.com%2Fedwardtfn%2FNSPanel-Easy%2Frefs%2Ftags%2Flatest%2Fnspanel_easy_blueprint.yaml).<br>[Update the blueprint](howto.md#update-blueprint). |
| You don't have an automation created using the blueprint. | On Home Assistant, go to **Settings** --> **Automations & Scenes** --> **Blueprints** --> **NSPanel Configuration** --> **CREATE AUTOMATION** and follow the [instructions to setup your automation](blueprint.md).<br>**Attention!!** You need one automation per panel, if you have more than one panel set. |
| Your panel is not selected in the automation. | Open the automation related to your panel and make sure the right device is set on the **NSPanel device** field. |
| Your panel's connection to Home Assistant may have issues.<br>This could be from an invalid entity Id (e.g. ending with `_2`), or some action missing registration. | Reconnect the Panel's device to Home Assistant:<br>1. Go to **Settings** --> **Devices & services** --> **ESPHome**<br>2. Delete the device<br>3. Restart Home Assistant host<br>4. Go back to **Settings** --> **Devices & services**<br>5. Click **Add integration**<br>6. Select **ESPHome**<br>7. Enter your panel's hostname or IP address. |
<!-- markdownlint-enable MD013 MD033 -->

## Additional Tips and Resources

After troubleshooting, if issues persist, consult the [Issues](https://github.com/edwardtfn/NSPanel-Easy/issues) and feel free to create a new one asking for more personalized assistance.

Please share as much info as possible, like:

1. Describing (or a picture of) what is on your screen
2. Are you updating from a previous version of this project, or you coming from another NSPanel customization (which one?) or are you customizing for the first time a panel with the original Sonoff settings?
3. Please share the ESPHome logs from when your panel starts to the moment the upload fails.
4. Describe what you have already tried.

## Important note

Remember, these steps are a guideline and might vary slightly based on your specific setup and previously installed system.
