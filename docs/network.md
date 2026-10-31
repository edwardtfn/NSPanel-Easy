# Network

This page describes the network settings of an NSPanel running NSPanel Easy: how it connects to your Wi-Fi, how it gets
its IP address, gateway and DNS server, which connections it makes, why it may need internet access, and which firewall
rules allow it.

Table of contents:

- [Summary](#summary)
- [Wi-Fi](#wi-fi)
  - [Hidden Wi-Fi](#hidden-wi-fi)
  - [Connect to multiple networks](#connect-to-multiple-networks)
- [IP address](#ip-address)
  - [Automatic (DHCP)](#automatic-dhcp)
  - [Manual IP](#manual-ip)
  - [Changing the IP address](#changing-the-ip-address)
- [Gateway](#gateway)
- [DNS server](#dns-server)
- [Different subnets and VLANs](#different-subnets-and-vlans)
- [Connection to Home Assistant](#connection-to-home-assistant)
  - [Reboot when the connection fails](#reboot-when-the-connection-fails)
- [Internet access](#internet-access)
  - [Why the panel accesses the internet](#why-the-panel-accesses-the-internet)
  - [What is exchanged](#what-is-exchanged)
  - [Consequences of blocking internet access](#consequences-of-blocking-internet-access)
  - [Running without internet access](#running-without-internet-access)
- [Firewall rules](#firewall-rules)
- [Connections outside the panel](#connections-outside-the-panel)

## Summary

- The panel connects to a 2.4 GHz Wi-Fi network. 5 GHz networks are not supported by the ESP32.
- By default, the IP address, gateway and DNS server are set automatically by your router (DHCP).
- Home Assistant and the ESPHome Device Builder connect to the panel. By default, the panel gets its time from Home
  Assistant.
- With the default settings, the panel needs internet access for a single purpose: downloading the display (TFT) file
  from GitHub. Customizations connecting to the internet, such as public Network Time (SNTP) servers, need their own
  firewall rules.
- No telemetry, analytics, cloud service, or "phone home" exists. No information about your home, Home Assistant, or
  entities is sent anywhere outside your network.

All settings on this page are customizations of your panel YAML. Add them to the customization area, as described in
[Customization](customization.md#instructions). For the complete list of options, see the ESPHome
[Wi-Fi Component](https://esphome.io/components/wifi/) page.

## Wi-Fi

The network name and password are set with the `wifi_ssid` and `wifi_password` substitutions in your panel YAML.
To change them on a panel already installed, follow
[Updating Wi-Fi settings](howto.md#updating-wi-fi-settings-and-ota-passwords-in-esphome-with-this-project).

### Hidden Wi-Fi

Connect to a hidden Wi-Fi network:

```yaml
# Connect to a hidden Wi-Fi network
wifi:
  networks:
    - id: !extend wifi_default
      hidden: true
  fast_connect: true
```

### Connect to multiple networks

The panel connects to the network with the highest priority or, with the same priority, the strongest signal.
If that fails, it tries the next network:

```yaml
# Set dual network
wifi:
  networks:
    - id: !extend wifi_default
      priority: 10
    - ssid: !secret wifi_ssid_backup
      password: !secret wifi_password_backup
      priority: 0
```

## IP address

### Automatic (DHCP)

By default, the panel gets its IP address, subnet mask, gateway, and DNS server from the DHCP server of your network
(usually your router). No settings are needed.

> [!TIP]
> To keep the same IP address, prefer a DHCP reservation in your router over a manual IP.
> The panel keeps getting the gateway and DNS server automatically, and the address can be changed in the router
> without installing a new firmware.

### Manual IP

Set the IP address in the panel itself:

```yaml
# Set IP address manually
wifi:
  networks:
    - id: !extend wifi_default
      manual_ip:
        static_ip: 192.168.0.123  # The panel IP address, outside the DHCP range of your router
        gateway: 192.168.0.1      # Usually the IP address of your router
        subnet: 255.255.255.0     # The subnet mask of your network
        dns1: 192.168.0.1         # Required to download the TFT from GitHub, see the DNS server section
        dns2: 1.1.1.1             # Optional
```

When a manual IP is set, DHCP is not used at all, so every setting comes only from this block.
`static_ip`, `gateway` and `subnet` are required. `dns1` and `dns2` are optional for ESPHome, but without them the panel
has no DNS server (see [DNS server](#dns-server)).

### Changing the IP address

The ESPHome Device Builder finds the panel by its name (`<device_name>.local`). If that does not work in your network,
or while the IP address is being changed, set the address used for installing and viewing logs:

```yaml
# Address used by the ESPHome Device Builder to reach the panel
wifi:
  use_address: 192.168.0.100  # The current IP address of the panel
```

When changing a manual IP, set `use_address` to the old address, install once, then change it to the new address
(or remove it).

## Gateway

The gateway is the router that forwards the traffic from the panel to anything outside its own subnet. It is used to:

- Download the TFT file from GitHub (internet).
- Reach a DNS server outside the panel subnet, such as `1.1.1.1` or `8.8.8.8`.
- Reach a server in another subnet or VLAN, such as a local server hosting the TFT files.

When Home Assistant and the ESPHome Device Builder are in the same subnet as the panel, their communication does not use
the gateway. A wrong or missing gateway affects only the cases above.

## DNS server

The DNS server translates host names into IP addresses. The panel does not need it for the daily use, as Home Assistant
connects to the panel, and not the opposite. It is used only when the panel connects to a host name:

- During a TFT update, to find the server in the TFT URL (by default `raw.githubusercontent.com`).
- Any other host name you add to your customizations, such as Network Time (SNTP) servers set by name.

With DHCP, the DNS server is set by your router. With a [manual IP](#manual-ip), set it with `dns1` (and optionally
`dns2`). Use the DNS server of your router or any other DNS server reachable by the panel. A public DNS server, such as
`1.1.1.1`, also requires a working [gateway](#gateway).

Without a DNS server, the TFT cannot be downloaded from GitHub. In that case, serve the TFT files from a local server and
use its IP address in the URL (see [Running without internet access](#running-without-internet-access)).

> [!TIP]
> For a local server, prefer its IP address in the URL (for example, `http://192.168.0.100:8123/...`) over a name like
> `homeassistant.local`. Names ending in `.local` are not resolved by the DNS server, but by mDNS, which may not work
> across subnets.

## Different subnets and VLANs

When the panel is in a different subnet or VLAN than Home Assistant or the ESPHome Device Builder:

- Device discovery (mDNS) does not cross subnets, unless your router forwards it (mDNS reflector).
  Add the panel to the Home Assistant ESPHome integration by its IP address, and set [`use_address`](#changing-the-ip-address)
  for the ESPHome Device Builder.
- Give the panel a fixed IP address, either with a DHCP reservation or a [manual IP](#manual-ip).
- Allow the connections listed in [Firewall rules](#firewall-rules) between the subnets.

## Connection to Home Assistant

Home Assistant connects to the panel through the ESPHome Native API (TCP port 6053). This connection carries entities,
actions, Blueprint settings, and the time. The ESPHome Device Builder also uses it to show the logs, and installs
firmware updates over the air (OTA) on TCP port 3232.

To encrypt the API and OTA connections, see [API encryption](customization.md#api-encryption) and
[OTA encryption](customization.md#ota-encryption).

### Reboot when the connection fails

Sometimes the ESP reports that it is connected to the network when, in fact, it is not, and only a reboot fixes it.
For that reason, the panel reboots when:

- It is not connected to the Wi-Fi for 15 minutes (ESPHome default).
- Home Assistant is not connected for 60 minutes (set by this project).

To use another interval for Home Assistant (15 minutes in this example), or `0s` to disable it:

```yaml
# Reboot if Home Assistant is not connected for 15 minutes
api:
  reboot_timeout: 15min
```

The Wi-Fi interval can be changed in the same way with `wifi: reboot_timeout:`.

## Internet access

### Why the panel accesses the internet

The NSPanel has two processors: the ESP32, running the ESPHome firmware, and the Nextion display, running the TFT file.
Both must be on compatible versions. The firmware is installed from your ESPHome Device Builder over the local network,
but the TFT file is downloaded by the panel itself and transferred to the display through the
[Upload TFT add-on](addon_upload_tft.md), which is part of the default installation.

By default, the TFT file is downloaded from the project repository on GitHub:

```text
https://raw.githubusercontent.com/edwardtfn/NSPanel-Easy/v<version>/hmi/nspanel_<model>.tft
```

Where `<version>` is the firmware version and `<model>` is `landscape`, `portrait` or `blank`, according to the model
selected in the **Update TFT - Display Model** setting.

The download happens only when one of the following occurs:

- **Automatic update:** about 65 seconds after the display is ready at boot, when the TFT on the display is older than the version required by
  the firmware. This typically happens once, right after a firmware update. It can be disabled with
  `upload_tft_automatically: false`.
- **Manual update:** when the **Update TFT display** button is pressed in Home Assistant.
- **Action call:** when the `upload_tft` action is called (only available when `include_action_upload_tft: true`).
- **Baud rate scan:** when the **Scan baud rate** button finds a working baud rate (only available when
  `include_button_scan_baud_rate: true`).

Outside of a TFT update, the panel does not connect to the internet.

### What is exchanged

During a TFT update, the panel sends standard HTTPS requests (`HEAD` and `GET`) to the URL above, downloading the file in
chunks with HTTP `Range` headers. The requests contain only the file URL and generic HTTP headers. As with any web
request, the server can see your public IP address and, from the URL, the firmware version and display model.

No credentials, device identifiers, Home Assistant data, or user settings are sent. Nothing is uploaded.

> [!NOTE]
> The connection is encrypted, but the panel does not verify the server certificate (a limitation of the ESPHome Nextion
> component). Someone able to intercept the traffic between the panel and GitHub could serve a different file.
> If this is a concern, [serve the TFT files locally](#running-without-internet-access), so the download never leaves
> your local network.

Each download is about 0.7 MB for `blank` and 13.5 MB for `landscape` and `portrait`. Failed attempts are downloaded
again.

### Consequences of blocking internet access

Blocking internet access does not affect the daily use of the panel: the Home Assistant connection, pages, buttons,
relays, the embedded thermostat, and firmware updates keep working.

The only affected feature is the TFT download. Whenever a TFT update is required (usually after a firmware update) and
the download fails, the panel:

1. Shows the TFT upload screen.
2. Retries the download several times, restarting the display halfway through.
3. Restarts the panel when all attempts fail.

As the TFT is still outdated after the restart, the automatic update starts again on the next boot, so the cycle
repeats until the download succeeds. In the meantime, the panel is mostly unusable.

When automatic updates are disabled (`upload_tft_automatically: false`), the panel does not try to download the file
by itself, but it keeps running with an outdated TFT, which may cause missing or broken features until the TFT is
updated. A manual update still downloads the file and, if the download fails, it also retries and restarts the panel,
but without starting again on the next boot.

### Running without internet access

Choose one of the options below:

- **Allow access only during updates:** keep the panel blocked and open the [firewall rules](#firewall-rules) for the
  internet only while a firmware update (and the following TFT update) is running.
- **Serve the TFT files locally:** download the TFT files matching your firmware version from the
  [`hmi`](https://github.com/edwardtfn/NSPanel-Easy/tree/latest/hmi) folder, copy them to a local web server (for example,
  the `www` folder of Home Assistant, served as `/local/`), and set `nextion_update_base_url` to that folder. The model
  selection and the automatic updates keep working, but the URL does not include the version: replace the files in
  that folder with the ones matching your firmware version on every release, before updating the firmware. See
  [Using a local base URL](addon_upload_tft.md#example-using-a-local-base-url-with-automatic-model-selection).

## Firewall rules

Connections to the panel, required when the panel is in a different subnet or VLAN, or when your network filters
traffic between devices:

| Source | Destination | Protocol / Port | Purpose |
| :- | :- | :- | :- |
| Home Assistant | Panel | TCP 6053 | ESPHome Native API |
| ESPHome Device Builder | Panel | TCP 6053 | Logs |
| ESPHome Device Builder | Panel | TCP 3232 | Firmware updates over the air (OTA) |
| Any | Panel | UDP 5353 | Device discovery (mDNS), same subnet only |

Connections from the panel:

| Destination | Protocol / Port | Purpose |
| :- | :- | :- |
| DHCP server | UDP 67/68 | IP address, gateway and DNS server (not used with a manual IP) |
| DNS server | UDP/TCP 53 | Host name resolution, such as `raw.githubusercontent.com` |
| `raw.githubusercontent.com` | TCP 443 (HTTPS) | TFT download from GitHub |
| Local TFT server (optional) | The port in your URL, such as TCP 8123 | TFT download from a local server |

> [!TIP]
> Create the rule for GitHub based on the host name (FQDN) whenever your firewall supports it.
> `raw.githubusercontent.com` is served by a content delivery network (CDN) whose IP addresses are shared with other
> services and may change without notice, so rules based on IP addresses can break at any time.

If you change `nextion_update_base_url` or `nextion_update_url`, allow the host and port of that URL instead of GitHub.

## Connections outside the panel

Installing and updating NSPanel Easy also requires internet access on other devices. These are not made by the panel
and are not covered by the rules above:

- **ESPHome Device Builder:** downloads the firmware packages from `github.com` (the `remote_package` in your panel YAML,
  refreshed every 5 minutes by default) and the build tools and libraries used to compile the firmware.
- **Home Assistant:** downloads the Blueprint from `raw.githubusercontent.com` when it is imported or re-imported.
