# Add-on: ESPectre motion detection

## Description

This add-on enables the use of your panel's Wi-Fi traffic to act as a motion sensor using the
ESPectre module - see [ESPectre.dev](https://espectre.dev) for the details of how the module works.

If the display is blank, and motion is detected, the display wakes up.

For best results you will need to make adjustments in _Home Assistant > Settings > Devices > ESPhome_

### Attention

1. The NSPanel is only just capable of running this addon in addition to everything else it is doing.
   Be kind to it and keep an eye on the image size when building. If you have too many addons, you might need to choose.
2. It is also asking a lot to use Bluetooth at the same time as WiFi motion detection as they use the same airspace.
   The `addon_espectre_and_bluetooth` combination makes some necessary compromises and includes `addon_bluetooth_proxy`.
3. The ESPectre addon makes use of the display item normally reserved for RELAY 2 and lights it up during motion detection.
   If _you are also using the second relay_, expect the unexpected 😃

## Installation

You will need to add the reference to `addon_espectre` or `addon_espectre_and_bluetooth` files on your ESPHome settings in the `package` section after the `remote_package` (base code), as shown below:

> [!NOTE]
> `addon_espectre_and_bluetooth` includes `addon_espectre` and `addon_bluetooth_proxy`, so don't try to add them as well yourself.

```yaml
substitutions:
  # Settings - Editable values
  device_name: "YOUR_NSPANEL_NAME"
  friendly_name: "Your panel's friendly name"
  wifi_ssid: !secret wifi_ssid
  wifi_password: !secret wifi_password
  language: en      # Language code - see docs/localization.md for all supported codes

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
      # Optional add-on configurations - uncomment no more than one of these
      - esphome/nspanel_esphome_addon_espectre_and_bluetooth.yaml  # both
      # - esphome/nspanel_esphome_addon_espectre.yaml  # just ESPectre
      # - esphome/nspanel_esphome_addon_bluetooth_proxy.yaml  # just Bluetooth

```

## Configuration

The following keys are available to be used in your `substitutions`. All values must be delimited with `""`:

<!-- markdownlint-disable MD013 MD033 -->
| Key | Required | Supported values | Default | Description |
| :- | :-: | :-: | :-: | :- |
| espectre_ref | no | valid ESPectre release tag | "3.0.0-rc3"  | ESPectre version control |
| bluetooth_proxy_active_scan | no | "false" or "true" | "false" | controls airtime share | 
<!-- markdownlint-enable MD013 MD033 -->

## Blueprint settings

In General Settings make sure that `relay 2` has a visible colour and icon - `cyan` and `mdi:motion-sensor` for example 

## ESPectre Manager app for Home Assistant

Head over to [ESPectre.dev](https://espectre.dev) for details.
Getting the balance right between traffic modes may take a bit of work.

> [!NOTE]
> If you have more than a few panels or other devices running ESPectre with default settings you will run the risk of bringing down your home wireless or even your whole home network.
> Install **ESPectre Manager app for Home Assistant** and use it to make necessary adjustments, setting traffic mode to External to use its multicast traffic generator for example.
> Properly configured, there is no problem using a dozen or more panels in combination with **ESPectre Manager**.



