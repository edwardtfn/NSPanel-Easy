# Instructions for developers

## ESPHome

### Logging

Avoid excessive log, or at least avoid excessive log at DEBUG level,
but if you look around this project, many of the functionalities will log like this:

```yaml
sensor:
  - id: my_sensor
    ...
    on_change:
      - lambda: |-
          static const char *const TAG = "sensor.my_sensor";
          ESP_LOGD(TAG, "New value: %f", x);  // No need to log this, use only when needed
          if (id(is_uploading_tft)) {
            ESP_LOGD(TAG, "Skipping any action as a TFT upload is in progress");  // No need to log this, use only when needed
          } else {
            // Your code here
          }
```

### Page opened

When a new page is opened, a script name `page_changed` (whith a parameter `page` containing a
string with the page name) is called and that one will call a page specific script named `page_<page_name>`.
In addition, pages with multiple pages (entitypageXX, buttonpageXX and weatherXX) will also call
a generic page named `page_entitypage`, `page_buttonpage` or `page_weather` with a parameter
`page_number` containing the number of the page called.

If you want to execute expecific code when a page is opened, you can extend the functionality of that specific page:

```yaml
script:
  - id: !extend page_changed
    then:
      # Code to run when any page is opened
      - lambda: |-
          static const char *const TAG = "script.page_changed (custom)";
          ESP_LOGD(TAG, "Custom code for page changed");
          ESP_LOGD(TAG, "This is the new page: %s", page.c_str());

  - id: !extend page_buttonpage
    then:
      # Code to run when any buttonpage is openend

- id: !extend page_buttonpage02
    then:
      # Code to run when buttonpage02 is opened
```

### Managing conflicts with Upload TFT

The TFT upload is the most resource conmsuming task in this project and should have all the resources available,
so when creating something, please take in account:

#### Stop your scripts before the upload starts

If you create a new script that should be stopped previous a Upload TFT starts,
please add your script to the `stop_all` script like this:

```yaml
script:
  - id: !extend stop_all
    then:
      - script.stop: my_new_script_id
```

#### Check for upload status before starting any action

There is a global `is_uploading_tft` which will be true when the upload TFT process starts,
so you can use this in your code like this:

```yaml
sensor:
  - id: my_sensor
    ...
    on_change:
      - if:
          condition:
            - lambda: !lambda return (!id(is_uploading_tft))
          then:
            # your code here
```

or:

```yaml
sensor:
  - id: my_sensor
    ...
    on_change:
      - lambda: |-
          static const char *const TAG = "sensor.my_sensor";
          ESP_LOGD(TAG, "New value: %f", x);  // No need to log this, use only when needed
          if (id(is_uploading_tft)) {
            ESP_LOGD(TAG, "Skipping any action as a TFT upload is in progress");  // No need to log this, use only when needed
          } else {
            // Your code here
          }
```

or if you are setting up a script (largely used in this project) it can just kill itself if `is_uploading_tft`:

```yaml
scripts:
  - id: my_new_script_id
    ...
    then:
      - lambda: |-
          if (id(is_uploading_tft)) my_new_script_id->stop();
          // Your code here
```

## Nextion

### Source and build

The display projects live as text in `hmi/src/<project>/`, in the [nxwright](https://github.com/nixiewright/nxwright) source format.
That source is the source of truth: `hmi/<project>.hmi` and `hmi/<project>.tft` are built from it.

- Page code is in `pages/*.nx`, components in `pages/*.page.toml`, macros in `macros.nx` and global code in `program.nx`.
  Code is strict Nextion syntax.
- Pictures are PNG files in `pictures/`. Fonts are not stored per project: `fonts.sha256` lists them,
  and the build takes them from `hmi/dev/fonts` by checksum.
- The display firmware comes from `hmi/dev/firmware/`, a minimal TFT carrying the Nextion Editor 1.68.2 firmware,
  and every build checks that the TFT files carry it unchanged.

Scripts in `hmi/dev/scripts/` (run them with `bash`; they install the pinned nxwright on first use, which needs Python 3.11 or later):

- `hmi_build.sh [project ...]` builds the HMI and TFT files from the source.
- `hmi_import.sh [project ...]` brings changes made in the Nextion Editor back into the source.
- `hmi_sync.sh BASE` is what CI runs on pull requests: for each project, it imports the HMI file when the pull request
  changed it, then builds everything from the source.

What CI does with the built files (workflow "Build HMI"):

- **Pull requests:** the HMI and TFT files are built to check the source, and attached to the workflow run as the
  `display-files` artifact. They are not committed. To test a pull request on a panel, download the artifact,
  host the TFT file where the panel can reach it (e.g. the `www` folder of Home Assistant), and upload it from there.
- **`dev`:** after a merge that changes the source, the HMI and TFT files are built and committed to `dev`.

Working in the Nextion Editor is still supported: open the `.hmi` file from `dev` (Nextion Editor v1.68.2 or later,
since the projects use macros), make the change, save, and commit the `.hmi` file to your pull request.
CI imports it into the source and replaces the `.hmi` file in the pull request with the source change.
Do not change the same project in the Editor and in the source in the same pull request: CI refuses it,
since either change would overwrite the other.

### Macros

Use a macro for code repeated across components or pages, so it is changed in one place.
Macros are in `macros.nx` in each project's source folder.

Macros only reach global data: the system variables (`sys0` to `sys3`), the variables declared in `Program.s`
and global-scope components on the `home` page. Pass inputs in system variables, return results in a global-scope
`home` variable, and document both, plus any system variable the macro changes, at the top of the macro.

#### `format_tenths`

Formats a value stored in tenths (as used by the climate and water heater sliders) as text.
The decimal is only shown when the step is not a whole number, using the separator in `home.dec_sep`.

- Inputs: `sys0` (value in tenths) and `sys1` (step in tenths)
- Output: `home.tenths_txt.txt`
- Also changes: `sys0`, `sys2` and `home.tenths_tmp.txt`

```c
sys0=temp_number0.val
sys1=temp_step.val
call format_tenths
target.txt=home.tenths_txt.txt+temp_unit.txt
```

### Interface used by external tools

Community tools, like [sztupy/nextion-editor-docker](https://github.com/sztupy/nextion-editor-docker),
rebuild the TFT files with custom backgrounds by regenerating the pictures and re-importing them into the HMI files.
They rely on the following, so treat it as a stable interface and announce any change in the release notes:

- `hmi/nspanel_landscape.hmi` and `hmi/nspanel_portrait.hmi` as the HMI sources.
- `hmi/dev/pics/background/background_<theme>_<orientation>.png` as the background sources,
  with `<theme>` as `dark`, `light` or `splash` and `<orientation>` as `landscape` or `portrait`.
  Backgrounds are opaque PNG files, 480x320 for landscape and 320x480 for portrait.
- `hmi/dev/pics/generate_images.sh <orientation>` generating `hmi/dev/pics/<orientation>/<slot>.png`,
  where `<slot>` is the picture id in the HMI file, numbered without gaps from `0`.
  The script must run from a complete checkout, as it also reads `hmi/dev/pics/mapping.yaml` and the source images,
  and exits with a non-zero status if any picture fails to generate.
- `hmi/dev/pics/misc/placeholder.png` as the filler for unused slots.

### Reporting to ESPHome

Report events to ESPHome with the Nextion custom protocol, handled by ESPHome's `on_custom_*` triggers
(`on_custom_binary_sensor`, `on_custom_sensor`, `on_custom_switch` and `on_custom_text_sensor`).
Prefer these over the `localevent` text sensor, which is being phased out as it uses more memory.

For example, a custom binary sensor:

```c
printh 93
prints "api_unavailable",0
printh 00 01 FF FF FF
```

Handle each key in the ESPHome package that owns the concern, checking the key first:

```yaml
display:
  - id: !extend disp1
    on_custom_binary_sensor:
      then:
        - lambda: |-
            if (key != "api_unavailable" or !value) return;
            // Your code here
```

### Home Assistant unavailable

Pages that need Home Assistant must not open while it is disconnected (`api==0`).
When the display refuses an action for that reason, call the `report_api_unavailable` macro.
ESPHome then gives feedback on the home pages or falls back to the home page elsewhere.

If a page changes whether it works without Home Assistant, update `is_page_offline_capable()` in
`components/nspanel_easy/pages.h` as well.

### Home page

Never navigate with a literal `page home`. Use `page home_page_id`, which ESPHome sets to the page used as home.
