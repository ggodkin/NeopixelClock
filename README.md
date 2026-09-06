# NeopixelClock

ESP32 / PlatformIO migration of the original ESP8266-controlled clock.

## Current status

The ESP32 migration is functional and the core clock/outage behavior has now been validated on hardware.

### Hardware / platform

- ESP32 DevKit (`esp32dev`) with PlatformIO / Arduino
- 32x8 WS2812B NeoPixel matrix
- Matrix mapping preserved from the original project:
  `NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG`
- Main/USB power detection on GPIO34 through a 12k/22k voltage divider
- Battery sense GPIO35 reserved for future implementation
- OTA enable input: GPIO32, internally pulled up, active LOW
- Time zone: `America/Denver`

### Migrated and validated

- [x] ESP32 / PlatformIO build and basic runtime
- [x] NeoPixel display operation
- [x] Timekeeper using native ESP32 `time_t`
- [x] SNTP time synchronization
- [x] AceTime time-zone conversion / DST handling
- [x] MQTT using PubSubClient
- [x] OTA using ArduinoOTA
- [x] Main power detection using averaged raw ADC readings
- [x] Power-loss / power-restore confirmation with 1-second debounce
- [x] Outage display behavior
- [x] Network shutdown during outage
- [x] Network/MQTT recovery after power restoration
- [x] Continuous timekeeping while the display/network are power-saving during outage

## OTA mode

OTA is now controlled by a dedicated active-low digital input rather than by reset behavior.

```text
GPIO32 HIGH / open  -> normal clock operation
GPIO32 LOW / GND    -> OTA mode
```

When OTA mode is active:

- The display shows `OTA` instead of the current time.
- Normal clock display updates are suppressed.
- WiFi remains enabled.
- `ArduinoOTA.handle()` is serviced.
- MQTT remains available.
- Releasing the OTA input immediately returns the display to normal clock operation.

OTA is unavailable during an outage because WiFi is intentionally shut down for battery conservation.

**Note:** GPIO32 is the currently selected OTA-enable input and should be confirmed against the final physical wiring before installation.

## Outage behavior

The current fixed outage profile is:

```text
Main power lost
      |
      v
  Display ON 2 s
      |
      v
  Display OFF 30 s + WiFi/MQTT OFF
      |
      v
  Display ON 2 s
      |
      v
  Display OFF 30 s
      |
      +---- repeat until main power is restored
```

Timekeeping continues throughout the outage.

When main power is restored, the display resumes immediately and WiFi/MQTT are restarted.

Deep sleep is intentionally deferred until the higher-priority power-management work is complete and characterized.

## Punch list / remaining work

### Priority 1 — Validate physical OTA activation

- [ ] Connect the OTA enable switch/input to GPIO32 and ground.
- [ ] Verify HIGH/open gives normal clock display.
- [ ] Verify LOW/ground changes the display to `OTA`.
- [ ] Perform an OTA firmware update while the input is grounded.
- [ ] Release the input and verify immediate return to the normal clock display.
- [ ] Verify OTA mode does not interfere with normal MQTT operation.

### Priority 2 — Battery power monitoring and power characterization

- [ ] Determine the actual battery hardware / voltage-divider arrangement for GPIO35.
- [ ] Implement battery-voltage measurement using the same raw-ADC approach used for main-power detection, once the hardware divider is confirmed.
- [ ] Establish useful battery-voltage thresholds and hysteresis.
- [ ] Measure/estimate current draw in the important operating states:
  - normal operation
  - outage, display ON
  - outage, display OFF with WiFi/MQTT disabled
- [ ] Use the measurements to estimate practical battery runtime.
- [ ] Decide whether low-battery behavior is needed before adding deep sleep.

### Priority 3 — Complete power-management edge cases

- [ ] Define and test behavior when the ESP32 boots while main power is already absent.
- [ ] Verify repeated power-loss / power-restore cycles.
- [ ] Verify recovery if WiFi is unavailable when main power returns.
- [ ] Verify MQTT recovery after a prolonged outage.
- [ ] Confirm OTA remains unavailable while the outage network shutdown is active.

### Priority 4 — Ambient-light / display brightness migration

- [ ] Identify the ESP32 hardware input for the ambient-light sensor.
- [ ] Migrate the original brightness/intensity behavior without assigning an arbitrary GPIO.
- [ ] Test brightness response over the intended light range.

### Priority 5 — Networking / timekeeping robustness

- [ ] Review WiFi startup/retry behavior and remove unnecessary blocking where appropriate.
- [ ] Add/verify explicit NTP retry policy and behavior after long network outages.
- [ ] Decide whether a separate WiFi setup/configuration mechanism is still wanted.
- [ ] Decide whether an alternative WiFi/fallback mechanism is needed.

### Priority 6 — ESP32 code cleanup / architecture

- [ ] Remove the duplicate legacy NeoPixel matrix implementation from `main.cpp` after confirming the modular `Display` implementation is fully equivalent.
- [ ] Continue moving responsibilities out of `main.cpp` only when there is a clear benefit and after preserving current behavior.
- [ ] Reduce unnecessary debug output once field behavior is stable.

### Priority 7 — Optional features from the original TODO list

These are not currently required for the ESP32 migration:

- [ ] Web control / configuration (time zone, WiFi credentials, Node-RED integration, etc.)
- [ ] RTC evaluation
- [ ] Outside-temperature-based clock color
- [ ] Outside humidity indicator

### Deferred

- [ ] Deep sleep / wake strategy

Deep sleep is intentionally lower priority. It should be considered after battery sensing, power measurements, and the remaining power-management edge cases are understood.

## Original project notes

- MQTT-controlled additional indicators
- Original design included brightness control based on ambient light
- Schematics: https://oshwlab.com/ggodkin/ws2812b-watch
