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

### Editor version

Nextion Editor v1.68.2 or later is required. The TFTs use macros (named global events called with `call <macro>`),
which older Editors cannot open or compile.

### Macros

Use a macro for code repeated across components or pages, so it is changed in one place.
`scripts/generate_nextion_txt.sh` exports the macros to `Macros.txt` next to `Program.s.txt`, so they show in reviews.

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
