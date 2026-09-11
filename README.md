# NeopixelClock

ESP32 / PlatformIO implementation of the original ESP8266-controlled NeoPixel clock.

The ESP32 implementation is now the active `main` branch. The original ESP8266 implementation is retained on the `esp8266-dormant` branch for reference/rollback. The `platformio-esp32-migration` branch also remains available as the migration history/reference branch.

## Current status

The ESP32 migration is functional and the core clock, networking, configurable time zone, configuration portal, OTA, power-loss detection, outage behavior, and network-status indication have been validated on hardware.

## Hardware / platform

- ESP32 DevKit (`esp32dev`)
- PlatformIO / Arduino framework
- PlatformIO platform: `espressif32@6.13.0`
- 32x8 WS2812B NeoPixel matrix
- Matrix mapping preserved from the original project:
  `NEO_MATRIX_TOP + NEO_MATRIX_LEFT + NEO_MATRIX_COLUMNS + NEO_MATRIX_ZIGZAG`
- Main/USB power detection on GPIO34 through a **15k / 22k** voltage divider
- Battery-voltage input GPIO35 reserved for future implementation
- OTA enable input: GPIO32, internally pulled up, active LOW
- Configuration enable input: GPIO25, internally pulled up, active LOW
- Configuration access point: `NeopixelClock-Setup`
- Configuration AP address: `192.168.4.1`

## Software architecture

The current implementation is divided into several focused modules:

- `main.cpp` — application startup, main loop, WiFi/MQTT/OTA coordination, and display scheduling
- `display.cpp/.h` — NeoPixel matrix initialization, clock/message rendering, garage-door indicator, and network-status LEDs
- `timekeeper.cpp/.h` — ESP32 system time, SNTP state, AceTime time-zone conversion, and RTC-data backup timestamp
- `power_manager.cpp/.h` — main-power ADC sensing and outage state machine
- `device_config.cpp/.h` — persistent device configuration stored in ESP32 NVS
- `config_portal.cpp/.h` — boot-time WiFi/device configuration web portal
- `config.h` — compile-time hardware and application constants
- `secrets.h` — initial/default private configuration values

## Migrated and validated

- [x] ESP32 / PlatformIO build and runtime
- [x] NeoPixel display operation
- [x] Existing 32x8 matrix geometry and mapping preserved
- [x] Existing clock font/geometry retained
- [x] Timekeeper using native ESP32 `time_t`
- [x] SNTP time synchronization
- [x] AceTime time-zone conversion and DST handling
- [x] Configurable time zone through the configuration portal
- [x] Time-zone selection uses a defined list of valid zones rather than free-form text
- [x] Configuration stored in ESP32 NVS
- [x] MQTT using PubSubClient
- [x] OTA using ArduinoOTA
- [x] OTA enable control using a dedicated active-low input
- [x] Main-power detection using averaged raw ADC readings
- [x] Power-loss / power-restore confirmation with 1-second debounce
- [x] Outage display behavior
- [x] Network shutdown during outage
- [x] Network/MQTT recovery after power restoration
- [x] Continuous timekeeping while the display/network are power-saving during outage
- [x] WiFi/NTP/MQTT status indication on the rightmost display column

## Configuration mode

Configuration mode is selected **only at boot** using the dedicated configuration input.

```text
GPIO25 HIGH / open  -> normal startup
GPIO25 LOW / GND    -> configuration mode
```

When configuration mode is requested:

- The display shows `CONFIG`.
- The ESP32 starts the `NeopixelClock-Setup` access point.
- The configuration portal is available at `192.168.4.1`.
- Stored settings can be viewed and changed.
- Settings are saved to NVS when explicitly saved through the portal.
- The device restarts after saving and uses the stored settings during normal startup.

The configurable settings currently include WiFi credentials, MQTT settings, and time zone.

The time zone is resolved by AceTime from the selected valid zone name. Invalid stored values are rejected/fall back rather than being treated as arbitrary free-form input.

## Timekeeping / NTP

Normal operation uses the ESP32 system clock with native SNTP and AceTime for local time conversion.

Startup sequence:

1. Load device configuration from NVS.
2. Initialize the Timekeeper with the configured time zone.
3. Restore the last RTC-data backup timestamp when it is available and valid.
4. Start native SNTP after configuration/startup processing.
5. Convert the resulting UTC/system time to the configured local time using AceTime.

The SNTP service is configured with:

```text
pool.ntp.org
time.nist.gov
time.google.com
```

With the ESP-IDF configuration used by this PlatformIO environment, the default SNTP synchronization interval is approximately **1 hour**. There is no application-level periodic NTP timer in `main.cpp`.

The firmware does **not** periodically write the clock to NVS. The `RTC_DATA_ATTR` value is updated approximately once per minute as a lightweight backup timestamp and is not a substitute for NTP synchronization.

### RTC reset limitation

The current RTC backup relies on ESP32 RTC retained data. RTC retained data does not survive every reset condition. In particular, a hardware/power-on reset can clear the RTC domain, so the last timestamp cannot be guaranteed after such a reset. This is a hardware/reset behavior rather than an NVS timekeeping mechanism.

No periodic NVS time writes are planned because they would add unnecessary flash wear and would not provide a desirable long-term clock backup strategy.

## Network status indicators

The bottom three LEDs of the rightmost display column are reserved for network diagnostics:

```text
x = 31

y=5  WiFi
y=6  NTP
y=7  MQTT
```

The indicators use the same three-state convention:

```text
Attempting  -> yellow
Failed      -> red
Connected   -> blue
```

The indicators are redrawn after normal display rendering so the clock refresh does not erase them.

### WiFi status

- Yellow while WiFi is being established.
- Blue when connected.
- Red for connection-failure/lost-network states detected by the application.

### NTP status

- Yellow while waiting for synchronization.
- Blue after SNTP reports synchronization complete.
- Red after the NTP timeout/failure conditions are reached.

### MQTT status

- Yellow before a successful MQTT connection.
- Blue while connected.
- Red after an MQTT connection attempt fails.

## OTA mode

OTA is controlled by a dedicated active-low digital input rather than by reset behavior.

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

## MQTT

MQTT uses `PubSubClient`.

Current behavior includes:

- MQTT server and credentials come from `DeviceConfig`.
- The MQTT connection is retried at a 5-second application interval after a failed/disconnected state.
- The garage-door command topic is subscribed to:

```text
cmnd/NeopixelClock/GarageDoorClosed
```

- The current time is published on:

```text
WatchBroom/Time
```

The existing garage-door status indicator remains part of the display behavior.

## Outage / power-management behavior

Main power is sensed on GPIO34 using averaged ADC readings. The current implementation uses a 15k / 22k resistor divider and a 1-second confirmation period for power-state changes.

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

During the display-off portion of an outage:

- The NeoPixel display is turned off.
- MQTT is disconnected.
- WiFi is turned off.
- Timekeeping continues.

When the main power returns:

- Normal display operation resumes.
- WiFi is restarted.
- MQTT is allowed to reconnect.
- The timekeeper continues using the running ESP32 system clock and SNTP can subsequently resynchronize.

The PowerManager also supports detecting a boot that starts while main power is already absent; this path skips normal WiFi/NTP/MQTT/OTA startup until power is restored.

Deep sleep is intentionally deferred until the remaining power-management behavior and battery characteristics are understood.

## Punch list / remaining work

### Priority 1 — Battery monitoring and power characterization

- [ ] Determine the actual battery hardware / voltage-divider arrangement for GPIO35.
- [ ] Implement battery-voltage measurement using the same raw-ADC approach used for main-power detection, once the hardware divider is confirmed.
- [ ] Establish useful battery-voltage thresholds and hysteresis.
- [ ] Measure/estimate current draw in the important operating states:
  - normal operation
  - outage, display ON
  - outage, display OFF with WiFi/MQTT disabled
- [ ] Use the measurements to estimate practical battery runtime.
- [ ] Decide whether low-battery behavior is needed before adding deep sleep.

### Priority 2 — Complete power-management edge cases

- [ ] Test repeated power-loss / power-restore cycles over longer periods.
- [ ] Verify recovery if WiFi is unavailable when main power returns.
- [ ] Verify MQTT recovery after a prolonged outage.
- [ ] Confirm OTA remains unavailable while the outage network shutdown is active.
- [ ] Characterize behavior for the various ESP32 reset causes encountered during power transitions.

### Priority 3 — Ambient-light / display brightness migration

- [ ] Identify the ESP32 hardware input for the ambient-light sensor.
- [ ] Migrate the original brightness/intensity behavior without assigning an arbitrary GPIO.
- [ ] Test brightness response over the intended light range.

### Priority 4 — Networking / timekeeping robustness

- [ ] Review WiFi startup/retry behavior and remove unnecessary blocking where appropriate.
- [ ] Review NTP retry behavior after long network outages.
- [ ] Verify timekeeping accuracy across long periods without network access.
- [ ] Decide whether a separate WiFi fallback/configuration mechanism is still wanted beyond the current boot-time portal.

### Priority 5 — ESP32 code cleanup / architecture

- [ ] Remove the duplicate legacy NeoPixel matrix implementation from `main.cpp` after confirming the modular `Display` implementation is fully equivalent.
- [ ] Continue moving responsibilities out of `main.cpp` only when there is a clear benefit and after preserving current behavior.
- [ ] Remove obsolete helper code and unnecessary debug output once field behavior is stable.

### Priority 6 — Optional features from the original project

These are not currently required for the ESP32 migration:

- [ ] Web control / additional configuration features
- [ ] Outside-temperature-based clock color
- [ ] Outside humidity indicator

## Deferred

- [ ] Deep sleep / wake strategy

Deep sleep is intentionally lower priority. It should be considered after battery sensing, power measurements, and the remaining power-management edge cases are understood.

## Repository branches

```text
main
  -> active ESP32 implementation

platformio-esp32-migration
  -> ESP32 migration/reference branch

esp8266-dormant
  -> original ESP8266 implementation retained for reference/rollback
```

## Original project notes

- Original project used an ESP8266 controller.
- MQTT-controlled additional indicators are retained where applicable.
- Original design included brightness control based on ambient light.
- Schematics: https://oshwlab.com/ggodkin/ws2812b-watch
