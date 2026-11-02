# Backing Up the Original Sonoff Firmware

This optional step saves a copy of the original Sonoff firmware from your panel's ESP32 before it is flashed with NSPanel Easy for the first time.
With this backup, you can return the panel to its original state later, for example to test a suspected hardware failure with Sonoff's own software,
to claim the warranty, or to sell or give the panel away.

## Summary

- [Why a Backup Is Needed](#why-a-backup-is-needed)
- [What Is and Isn't Included](#what-is-and-isnt-included)
- [Keep Your Backup Private](#keep-your-backup-private)
- [Creating the Backup](#creating-the-backup)
- [Restoring the Original Firmware](#restoring-the-original-firmware)
  - [Without a Backup](#without-a-backup)

## Why a Backup Is Needed

Each panel leaves the factory with its own device credentials stored in the ESP32 flash memory.
The eWeLink cloud uses these credentials to identify the device.
Flashing NSPanel Easy erases them, and they cannot be recovered or replaced afterwards.

Copies of the original firmware can be found online, but they carry the credentials of the panel they were copied from.
When flashed to a different panel, the eWeLink app rejects the device, so the panel cannot be paired or used with Sonoff's software.

<!-- markdownlint-disable MD028 -->
> [!IMPORTANT]
> A backup of your own panel is only possible **before** the first flash.
> Once NSPanel Easy (or any other firmware) is installed, the original firmware and its credentials are gone.

> [!IMPORTANT]
> If you have more than one panel, make a separate backup of each one and label it clearly (for example, `nspanel_living_room_stock.bin`).
> A backup only works on the panel it was taken from, as each one contains that panel's unique credentials.
<!-- markdownlint-enable MD028 -->

## What Is and Isn't Included

The panel has two independent controllers:

- **ESP32:** runs the firmware and holds the device credentials. This is what the backup covers.
- **Nextion display:** runs the `tft` file with the screen layout.
The display's memory cannot be read back, so the original Sonoff `tft` cannot be backed up with this procedure.

To fully restore the panel, you will also need the original Sonoff `tft` file.
This project does not distribute it, as it is Sonoff's proprietary software.

## Keep Your Backup Private

> [!CAUTION]
> The backup file contains your panel's unique credentials and may also contain your Wi-Fi network name and password.
> Store it safely and **never share it**, including as an attachment on GitHub issues or Discord.

## Creating the Backup

The backup is made over the same USB-to-Serial TTL connection used for the first flash.
Follow the [Drive Safe](install.md#drive-safe) advice and connect your panel as described in
[Connecting Your Device via USB-to-Serial TTL Interface](install.md#connecting-your-device-via-usb-to-serial-ttl-interface),
including the `IO0` to `GND` connection that puts the ESP32 into firmware transfer mode.

1. **Install `esptool`:**
   Espressif's official flashing tool requires [Python](https://www.python.org/downloads/). With Python installed, run:

   ```bash
   pip install esptool
   ```

2. **Identify the serial port** of your TTL adapter (for example, `COM3` on Windows or `/dev/ttyUSB0` on Linux).
3. **Read the full flash memory** (4 MB) into a file, replacing `<PORT>` with your serial port:

   ```bash
   esptool --port <PORT> --baud 115200 read-flash 0 0x400000 nspanel_stock.bin
   ```

   This takes a few minutes.
4. **Verify the backup** by comparing the file with the flash memory:

   ```bash
   esptool --port <PORT> --baud 115200 verify-flash 0 nspanel_stock.bin
   ```

   Only continue if the verification succeeds. Otherwise, repeat step 3.

> [!TIP]
> Older versions of `esptool` use `esptool.py` as the program name and `read_flash`, `verify_flash`, and `write_flash` (with underscores) as the command names.

Once the backup is verified, close the terminal so the serial port is released,
and continue with [Transferring the Firmware to Your Device](install.md#transferring-the-firmware-to-your-device).
The panel can stay connected as it is.

## Restoring the Original Firmware

Restore the display first, while NSPanel Easy is still running, as it can transfer a `tft` file from a URL:

1. **Restore the original `tft`:**
   Place the original Sonoff `tft` file on a local web server (for example, the `www` folder of Home Assistant),
   then send it to the panel with the [`upload_tft` action](api.md#tft-file-update-action-upload_tft).
2. **Restore the ESP32 firmware:**
   Open the panel, connect it as for the first flash, and write the backup taken from this panel to the start of the flash memory:

   ```bash
   esptool --port <PORT> --baud 115200 write-flash 0 nspanel_stock.bin
   ```

3. **Reassemble the panel** and power it up.
   It should start with the original Sonoff interface and can be paired with the eWeLink app.

> [!NOTE]
> Once the original `tft` is installed, the display no longer responds to NSPanel Easy.
> This is expected, and the panel will work again once the ESP32 firmware is restored.
> If you change your mind, use the [TFT Transfer Troubleshooting Guide](tft_upload.md) to return to an NSPanel Easy `tft`.

### Without a Backup

If no backup was made before the first flash, the panel cannot be fully restored to its original state.
Copies of the original firmware from other panels can be flashed, but the eWeLink app will reject the device, as explained in [Why a Backup Is Needed](#why-a-backup-is-needed).

The original Sonoff `tft` can still be installed on its own with step 1 above, for example to compare the display's behaviour with Sonoff's software.
Use the [TFT Transfer Troubleshooting Guide](tft_upload.md) to return to an NSPanel Easy `tft` afterwards.
