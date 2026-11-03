# Version compatibility

NSPanel Easy targets **Home Assistant** and **ESPHome** versions released no more than
one month before the NSPanel Easy release date.

For example, a firmware released in March 2026 will generally require Home Assistant and
ESPHome versions from February 2026 or later (i.e., `2026.2.x`).

> [!NOTE]
> This is a best-effort guideline, not a hard contract.
> Exceptions may occur if a dependency introduces a breaking change that requires a newer version,
> in which case the minimum version can be higher than the one described above.
> Releases before November 2026 targeted versions released no more than two months before the release date.

## Finding the exact minimum ESPHome version

The enforced minimum ESPHome version is declared in the firmware itself and checked at
compile time. You will get a clear error if your ESPHome installation is too old to
compile the firmware.
