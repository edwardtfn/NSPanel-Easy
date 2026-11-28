# Nextion instructions used for screenshots

Mockups for the documentation screenshots, run in the Nextion simulator (Nextion Editor, Debug).

- Select "Instruction codes: utf-8" in the simulator, so the icons (MDI glyphs in the code below) are sent correctly.
- Load the HMI of the model to capture: `nspanel_landscape.hmi` for EU and US landscape, `nspanel_portrait.hmi` for US portrait.
- The scripts are the same for every model, as the page components are the same. For US landscape, run `display_mode=3` after the session setup (hides the hardware button labels on the home page).
- Run the session setup once after starting the simulator, then the block of each page.
- For the US models, run the "US units" block after the page block, where there is one (on the status page, "US portrait").

## Session setup

Without ESPHome, the display starts on a dimmed screensaver with the touch disabled, and opens the status page after about 20 seconds.
This tells the display that ESPHome and Home Assistant are connected, so the pages can be opened and stay bright.

```nextion
esphome=1
api=1
display_ack=1
tsw 255,1
dim=100
```

## Status page (boot)

The first line and the details are filled by the display itself.
The other lines are what the firmware sends when everything is fine.

```nextion
page boot
version.txt="2026.12.0"
nspanel_easy.txt="Living room panel"
mark2.txt=""
mark2.pco=15947
val2.txt="2026.12.0, ESPHome 2026.11.3"
mark3.txt=""
mark3.pco=15947
val3.txt="192.168.0.123 -58 dBm"
mark4.txt=""
mark4.pco=15947
val4.txt="connected 2h 15m"
mark5.txt=""
mark5.pco=15947
val5.txt="2026.12.0 complete"
mark6.txt=""
mark6.pco=15947
val6.txt="42"
mark7.txt=""
mark7.pco=15947
val7.txt="all 312"
```

**US portrait:**

```nextion
val2.txt="2026.12.0 / 2026.11.3"
```

![Status page (boot) EU](../../docs/pics/eu_boot.png)

![Status page (boot) US](../../docs/pics/us_boot.png)

## Home page

Indoor and outdoor temperature, three values, four chips, the hardware button labels and the navigation buttons (utilities, notification, QR code, entities, three custom buttons and the alarm).

```nextion
vis_home=123899
page home
time.txt="12:34"
meridiem.txt=" "
date.txt="Wednesday, 25.11"
weather.pic=11
outdoor_temp.txt="8.5 °C"
indr_temp_icon.txt=""
indr_temp.txt="21.6 °C"
value01_icon.txt=""
value01.txt="47 %"
value02_icon.txt=""
value02.txt="715 W"
value03_icon.txt="侀"
value03.txt="1.2 kW"
wifi_icon.txt=""
chip_relay1.txt=""
chip_relay1.pco=64704
chip01.txt=""
chip01.pco=52857
chip02.txt=""
chip02.pco=64164
chip03.txt=""
chip03.pco=1055
left_bt_text.txt="Ceiling lights"
right_bt_text.txt="Desk lamp"
button01.txt=""
button01.pco=64704
button02.txt=""
button03.txt=""
bt_utilities.txt=""
bt_notific.txt=""
bt_qrcode.txt=""
bt_entities.txt=""
button07.txt=""
button07.pco=19818
```

**US units:**

```nextion
time.txt="12:34"
meridiem.txt="PM"
date.txt="Wednesday, 11/25"
outdoor_temp.txt="47 °F"
indr_temp.txt="70.9 °F"
```

![Home page EU](../../docs/pics/eu_home.png)

![Home page US](../../docs/pics/us_home.png)

## Screensaver

The clock with the chips.
The default time colour (dark grey) is very dark in a screenshot, so a light grey is used here; it is one of the Blueprint's colour options.
`brightness_sleep` is set only because the screensaver applies it (the default is off).

```nextion
brightness_sleep=100
screensaver_font=6
screensaver_pco=52857
screensaver_bco=0
chips_screensaver=1
page screensaver

text.txt="12:34"
chip_relay1.txt=""
chip_relay1.pco=64704
chip01.txt=""
chip01.pco=52857
chip02.txt=""
chip02.pco=64164
chip03.txt=""
chip03.pco=1055
```

**US units:**

```nextion
text.txt="12:34 PM"
```

## Buttons page

Eight buttons, four of them on.
`is_buttonpage02` only adds a second dot to the page index.

```nextion
is_buttonpage01=255
is_buttonpage02=255
page buttonpage01
page_label.txt="Living room"
button01pic.picc=47
button01text.picc=47
button01icon.picc=47
button01bri.picc=47
button01text.pco=10597
button01bri.pco=10597
button01icon.pco=64704
button01icon.txt=""
button01bri.txt="80 %"
button01text.txt="Ceiling\rlights"
button02pic.picc=47
button02text.picc=47
button02icon.picc=47
button02bri.picc=47
button02text.pco=10597
button02bri.pco=10597
button02icon.pco=64704
button02icon.txt=""
button02bri.txt="45 %"
button02text.txt="Floor lamp"
button03text.pco=65535
button03bri.pco=65535
button03icon.pco=52857
button03icon.txt=""
button03bri.txt=" "
button03text.txt="TV"
button04pic.picc=47
button04text.picc=47
button04icon.picc=47
button04bri.picc=47
button04text.pco=10597
button04bri.pco=10597
button04icon.pco=64704
button04icon.txt=""
button04bri.txt="60 %"
button04text.txt="Blinds"
button05text.pco=65535
button05bri.pco=65535
button05icon.pco=52857
button05icon.txt=""
button05bri.txt=" "
button05text.txt="Ceiling fan"
button06text.pco=65535
button06bri.pco=65535
button06icon.pco=52857
button06icon.txt=""
button06bri.txt=" "
button06text.txt="Coffee\rmaker"
button07pic.picc=47
button07text.picc=47
button07icon.picc=47
button07bri.picc=47
button07text.pco=10597
button07bri.pco=10597
button07icon.pco=64164
button07icon.txt=""
button07bri.txt="21.5 °C"
button07text.txt="Heating"
button08text.pco=65535
button08bri.pco=65535
button08icon.pco=52857
button08icon.txt=""
button08bri.txt=" "
button08text.txt="Movie time"
```

**US units:**

```nextion
button07bri.txt="71 °F"
```

![Buttons page EU](../../docs/pics/eu_buttonpages_icon_large.png)

## Entities page

Six entities with icons, labels and right-aligned values.

```nextion
is_entitypage01=1
is_entitypage02=1
page entitypage01
page_label.txt="Power monitoring"
value01_icon.txt=""
value01_label.txt="Kitchen"
value01.txt="1123.1 W"
value01.xcen=2
value02_icon.txt=""
value02_label.txt="Living room"
value02.txt="233.1 W"
value02.xcen=2
value03_icon.txt="車"
value03_label.txt="Total (entire home)"
value03.txt="2345.6 W"
value03.xcen=2
value04_icon.txt="侀"
value04_label.txt="Solar production"
value04.txt="1.8 kW"
value04.xcen=2
value05_icon.txt=""
value05_label.txt="Electricity price"
value05.txt="1.21 €/kWh"
value05.xcen=2
value06_icon.txt=""
value06_label.txt="Electricity cost rate"
value06.txt="1.84 €/h"
value06.xcen=2
```

**US units:**

```nextion
value03_label.txt="Total (home)"
value04_label.txt="Solar"
value05_icon.txt=""
value05_label.txt="Price"
value05.txt="0.21 $/kWh"
value06_icon.txt=""
value06_label.txt="Cost rate"
value06.txt="0.32 $/h"
```

![Entities page EU](../../docs/pics/eu_entities.png)

![Entities page US](../../docs/pics/us_entities.png)

## Climate page

A thermostat heating to 22 °C, with auto, heat and off modes.
The slider uses the firmware's scale: 7 °C to 35 °C in 0.5 °C steps (`temp_offset` 70, `temp_step` 5, 56 steps; for °F, 45 °F to 95 °F in 1 °F steps), so 22 °C is step 30.

```nextion
page climate
page_label.txt="Living room"
temp_offset.val=70
temp_step.val=5
temp_unit.txt=" °C"
climateslider.maxval=56
climateslider.val=30
current_temp.txt="21.6 °C"
target_high.txt="22.0 °C"
target_icon.txt=""
target_icon.pco=64164
value01_icon.txt=""
value01.txt="47 %"
value02_icon.txt=""
value02.txt="35 %"
button01.pco=52857
button03.pco=64164
button07.pco=35921
vis current_temp,1
vis target_high,1
vis target_icon,1
vis climateslider,1
vis button01,1
vis button03,1
vis button07,1
```

**US units:**

```nextion
temp_offset.val=450
temp_step.val=10
temp_unit.txt=" °F"
climateslider.maxval=50
climateslider.val=27
current_temp.txt="71 °F"
target_high.txt="72 °F"
```

## Light page

A light with brightness, colour temperature, colour and effects (`light_features_mask=15`).
The temperature slider is inverted, as the firmware sends it:
`min + max - current`.

```nextion
page light
page_label.txt="Kitchen lights"
icon_state.txt=""
lightslider.val=75
light_value.txt="75 %"
light_value_2.txt="75 %"
power_button.pco=50712
tempslider.minval=2000
tempslider.maxval=6500
tempslider.val=5800
temp_value.txt="2700K"
temp_value_2.txt="2700K"
effect_label.txt="Candle"
light_features_mask=15
features.en=1
```

![Light page EU](../../docs/pics/eu_light.png)

![Light page US](../../docs/pics/us_light.png)

## Light effect selector

The effect picker over the light page, with eight effects and Candle selected.
Wait for the light page to be drawn before the second part, or run it as a separate block.

```nextion
page light
page_label.txt="Kitchen lights"
icon_state.txt=""
lightslider.val=75
light_value.txt="75 %"
light_value_2.txt="75 %"
power_button.pco=50712
tempslider.minval=2000
tempslider.maxval=6500
tempslider.val=5800
temp_value.txt="2700K"
temp_value_2.txt="2700K"
effect_label.txt="Candle"
light_features_mask=15
features.en=1

doevents
delay 1000
page popup_select
caller_page_id.val=20
select_mode.val=0
opt_count.val=8
total_count.val=8
selection_mask.val=32
lbl_title.txt="Select effect"
opt0.txt="None"
opt1.txt="Rainbow"
opt2.txt="Fireplace"
opt3.txt="Pulse"
opt4.txt="Strobe"
opt5.txt="Candle"
opt6.txt="Police"
opt7.txt="Party"
init.en=1
```

![Light effect selector EU](../../docs/pics/eu_light_effect_selector.png)

![Light effect selector US](../../docs/pics/us_light_effect_selector.png)

## Cover page

Blinds at 60 %, without tilt.

```nextion
page cover
page_label.txt="Living room blinds"
icon_state.txt=""
coverslider2.val=60
cover_value_2.txt="60 %"
vis coverslider2,1
vis cover_value_2,1
```

## Fan page

A fan at 50 % (speed 2 of 4) with oscillation.

```nextion
page fan
page_label.txt="Bedroom fan"
icon_state.txt=""
fanslider.maxval=4
fanslider.val=2
fan_value.txt="50 %"
button_up.pco=59164
button_down.pco=59164
power_button.pco=50712
bt_oscillate.txt="ﲐ"
bt_oscillate.pco=59164
vis fanslider,1
vis fan_value,1
vis button_up,1
vis button_down,1
vis bt_oscillate,1
```

![Fan page US](../../docs/pics/us_fan.png)

## Media player page

A speaker playing, with volume and progress.
The track and artist are made up.

```nextion
page media_player
prg_timer.en=0
page_label.txt="Living room speaker"
icon_state.txt=""
track.txt="Northern Lights"
artist.txt="The Easy Panels"
bt_on_off.pco=10597
bt_play_pause.txt=""
vol_slider.val=35
vol_text.txt="35%"
time_current.txt="1:42"
time_total.txt="4:05"
time_progress.val=42
vis bt_on_off,1
vis bt_play_pause,1
vis bt_prev,1
vis bt_next,1
vis bt_mute,1
vis bt_vol_down,1
vis bt_vol_up,1
vis vol_slider,1
vis vol_text,1
vis time_current,1
vis time_total,1
vis time_progress,1
```

## Alarm page

Armed away.

```nextion
page alarm
page_label.txt="Home alarm"
icon_state.txt=""
icon_state.pco=19818
bt_home_pic.pic=42
bt_home_text.bco=52857
bt_home_icon.bco=52857
bt_home_text.pco=0
bt_home_icon.pco=0
bt_away_pic.pic=43
bt_away_text.bco=19818
bt_away_icon.bco=19818
bt_away_text.pco=65535
bt_away_icon.pco=65535
bt_night_pic.pic=42
bt_night_text.bco=52857
bt_night_icon.bco=52857
bt_night_text.pco=0
bt_night_icon.pco=0
bt_vacat_pic.pic=42
bt_vacat_text.bco=52857
bt_vacat_icon.bco=52857
bt_vacat_text.pco=0
bt_vacat_icon.pco=0
bt_bypass_pic.pic=42
bt_bypass_text.bco=52857
bt_bypass_icon.bco=52857
bt_bypass_text.pco=0
bt_bypass_icon.pco=0
bt_disarm_pic.pic=42
bt_disarm_text.bco=52857
bt_disarm_icon.bco=52857
bt_disarm_text.pco=0
bt_disarm_icon.pco=0
```

## Weather page

Today's forecast.

```nextion
page weather01
weather_icon.pic=11
day.txt="Today"
date.txt="Wednesday, 25.11"
temperature.txt="3 °C / 9 °C"
value01_icon.txt=""
value01.txt="Partly cloudy"
value02_icon.txt=""
value02.txt="20 %"
value03_icon.txt=""
value03.txt="12 km/h"
value04_icon.txt=""
value04.txt="65 %"
value05_icon.txt=""
value05.txt="1013 hPa"
```

**US units:**

```nextion
date.txt="Wednesday, 11/25"
temperature.txt="37 °F / 48 °F"
value03.txt="7 mph"
value05.txt="29.9 inHg"
```

## Utilities page

Home, grid and four groups, with the flow lines moving.
Add `tm_movement.en=0` at the end to freeze them.

```nextion
page utilities
title.txt="Energy"
title_icon.txt=""
home_label.txt="Home"
home.txt="1.2 kW"
homeb.txt=" "
grid_label.txt="Grid"
grid.txt="0.4 kW"
gridb.txt="1.21 €/kWh"
vis home_icon,1
vis home_label,1
vis home,1
vis homeb,1
vis home_line,1
vis grid_icon,1
vis grid_label,1
vis grid,1
vis gridb,1
group01_icon.txt="侀"
group01_label.txt="Solar"
group01.txt="1.8 kW"
group01b.txt=" "
vis group01_icon,1
vis group01_label,1
vis group01,1
vis group01b,1
vis group01_line,1
group02_icon.txt=""
group02_label.txt="Battery"
group02.txt="0.5 kW"
group02b.txt=" "
vis group02_icon,1
vis group02_label,1
vis group02,1
vis group02b,1
vis group02_line,1
group03_icon.txt=""
group03_label.txt="Car"
group03.txt="0.7 kW"
group03b.txt=" "
vis group03_icon,1
vis group03_label,1
vis group03,1
vis group03b,1
vis group03_line,1
group04_icon.txt="既"
group04_label.txt="Heat pump"
group04.txt="0.4 kW"
group04b.txt=" "
vis group04_icon,1
vis group04_label,1
vis group04,1
vis group04b,1
vis group04_line,1
```

**US units:**

```nextion
gridb.txt="0.21 $/kWh"
```

![Utilities page EU](../../docs/pics/eu_page_utilities.png)

![Utilities page US](../../docs/pics/us_page_utilities.png)

## QR code page

A guest Wi-Fi QR code.
The network and password are made up.

```nextion
page qrcode
page_label.txt="Guest Wi-Fi"
icon_state.txt=""
qr.txt="WIFI:T:WPA;S:Guests;P:easy-panel;;"
```

## Notification page

A notification with the accept and clear buttons.

```nextion
page notification
notifi_label.txt="Laundry"
notifi_text01.txt="The washing machine has finished."
```

## Settings page

The brightness sliders read `brightness` and `brightness_dim`.

```nextion
brightness=80
brightness_dim=20
page settings
```

![Settings page EU](../../docs/pics/eu_settings.png)

![Settings page US](../../docs/pics/us_settings.png)
