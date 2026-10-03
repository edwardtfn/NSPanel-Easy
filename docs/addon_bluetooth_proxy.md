# Add-on: Bluetooth Proxy

## Description

This add-on turns your panel into an ESPHome [Bluetooth Proxy](https://esphome.io/components/bluetooth_proxy/),
extending the Bluetooth range of Home Assistant. It is useful for connecting Bluetooth devices
near the panel and for tracking BLE devices (e.g. phones or tags) from room to room.

To save resources, BLE scanning runs only while Home Assistant is connected to the panel.

> [!IMPORTANT]
> Bluetooth is memory intensive and reduces the heap available for the rest of the firmware.
> See [Tips for Managing Memory](customization.md#tips-for-managing-memory).

## Installation

Add the `nspanel_esphome_addon_bluetooth_proxy.yaml` file to the `files` list of your `remote_package`, after the basic package:

```yaml
packages:
  remote_package:
    url: https://github.com/edwardtfn/NSPanel-Easy
    ref: latest
    refresh: 300s
    files:
      - nspanel_esphome.yaml  # Basic package
      # Optional add-ons
      - esphome/nspanel_esphome_addon_bluetooth_proxy.yaml
```

### Settings

The following substitution can be set in your local yaml:

| Substitution | Default | Description |
| --- | --- | --- |
| `bluetooth_proxy_active_scan` | `"true"` | Request scan responses from advertising devices. Provides more data (e.g. device names), at the cost of more airtime. |

## Home Assistant

Once the panel is connected, Home Assistant discovers it as a Bluetooth adapter,
listed under **Settings** > **Devices & services** > **Bluetooth**.
No additional entities are created on the panel's device page.
