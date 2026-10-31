# Security Policy

## Supported Versions

Only the latest release receives security fixes. Fixes are not backported to older releases.

To stay protected, keep all components (ESPHome firmware, TFT file, and Blueprint) updated to the latest release.

## Reporting a Vulnerability

Do not report security vulnerabilities through public issues, discussions, or Discord.

Report them privately using GitHub's
[private vulnerability reporting](https://github.com/edwardtfn/NSPanel-Easy/security/advisories/new).
Please include:

- A description of the vulnerability and its impact
- The affected component (ESPHome firmware, TFT file, or Blueprint) and version
- Steps to reproduce, or a proof of concept

This is a volunteer-maintained project, so responses are handled on a best-effort basis.
Once a fix is released, a security advisory is published, crediting the reporter if they wish.

Vulnerability reports are handled without AI assistance and are never shared with AI tools.

## Scope

In scope:

- The ESPHome configuration and custom components in this repository
- The Nextion TFT files
- The Home Assistant Blueprint

Out of scope:

- Vulnerabilities in ESPHome, Home Assistant, or the Nextion display firmware itself.
  Report those to the respective project, such as [ESPHome](https://github.com/esphome/esphome/security)
  or [Home Assistant](https://github.com/home-assistant/core/security).
- Issues caused by a user's own configuration, such as disabling API encryption or using a weak OTA password.
