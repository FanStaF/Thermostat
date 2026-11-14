# ESP8266 Thermostat Firmware

Arduino firmware for the ESP8266-based thermostat controller (current version: see
`FIRMWARE_VERSION` in `Config.h`).

## Hardware

- **Board:** LOLIN(WEMOS) D1 mini clone (ESP8266), FQBN `esp8266:esp8266:d1_mini_clone`
- **Sensor:** DS18B20 on D2, with a 4.7k pull-up to 3.3V and its own VDD wire (not parasite power)
- **Relays:** 4x relay module (active-LOW)
- **Power:** 12V → 5V buck into the D1 mini's 5V pin. Use solid connections (screw terminal or
  solder); a loose contact here caused brownout resets and sensor dropouts. A 470–1000 µF
  capacitor across the 5V input helps ride through relay/WiFi current spikes.

| Pin | Function |
|-----|----------|
| D1 | Relay 1 |
| D2 | DS18B20 data |
| D5 | Relay 2 |
| D6 | Relay 3 |
| D7 | Relay 4 |

## Features

- 4 relays, each with a **type** (HEATING, COOLING, GENERIC, MANUAL_ONLY) and **mode**
  (AUTO, MANUAL_ON, MANUAL_OFF)
- AUTO control with hysteresis, plus a **2-minute minimum on/off time** so a relay can't chatter
- **Sensor fail-safe:** if no good reading for 60 s (≥3 failures in a row), AUTO relays turn
  OFF; manual modes are untouched. Clears automatically on the next good reading. Hardware
  override switches, not the firmware, are the force-on path.
- **Readings outbox:** every reading is queued on flash with its timestamp and uploaded in
  batches; lines are deleted once the server confirms. Survives outages and reboots (~25 h at a
  60 s interval)
- **WiFi fallbacks** (see below): up to 5 saved networks, automatic rollback, and its own access
  point with a captive portal when no network is available
- **Device health** in every heartbeat: boot count, reset reason, uptime, WiFi signal, memory,
  upload backlog
- Local web interface that works without internet; settings persisted in LittleFS (written
  atomically)
- Password-protected OTA updates
- All timestamps kept in **UTC**; browsers show them in their local time

## Relay Types

| Type | ON when | OFF when | Threshold rule |
|------|---------|----------|----------------|
| HEATING | temp < ON | temp > OFF | ON must be below OFF |
| COOLING | temp > ON | temp < OFF | ON must be above OFF |
| GENERIC | temp > ON | temp < OFF | ON must be above OFF |
| MANUAL_ONLY | manual only | manual only | — |

Thresholds must be between −40 and 80 °C. Changing a relay's type swaps its thresholds if they
are in the wrong order for the new type.

## File Structure

```
Thermostat/
├── Thermostat.ino            # Setup, main loop, backend command handling
├── Config.h                  # Pins, intervals, limits
├── Credentials.h             # WiFi/API/OTA secrets (gitignored)
├── Credentials.h.example     # Template
├── NetManager.h/cpp          # WiFi networks, rollback, fallback AP + captive portal
├── TemperatureManager.h/cpp  # Non-blocking DS18B20 reads, local history log, diagnostics
├── RelayController.h/cpp     # Relay logic, validation, hold times, fail-safe
├── ConfigManager.h/cpp       # Settings persistence
├── ApiClient.h/cpp           # Backend communication + reading outbox
├── WebInterface.h/cpp        # Local web server and pages
├── SystemLogger.h/cpp        # In-memory ring buffer log (120 entries)
├── FileUtil.h/cpp            # Safe "keep the tail of a file" helper
├── build.sh / upload.sh      # Build and upload helpers
```

## Setup

### 1. Dependencies

ESP8266 core 3.1.2 and these libraries (the versions CI uses):

```bash
arduino-cli lib install "OneWire@2.3.8" "DallasTemperature@4.0.6" "ArduinoJson@7.4.3"
```

### 2. Credentials

```bash
cp Credentials.h.example Credentials.h
```

```cpp
#define WIFI_SSID "your-wifi-ssid"        // built-in network, always tried as a last resort
#define WIFI_PASSWORD "your-wifi-password"
#define API_URL "http://your-server.com"  // see "HTTPS" below
#define API_KEY "your-device-api-key"
#define OTA_PASSWORD "long-random-value"  // required: the build fails without it
```

### 3. Build & upload

```bash
./build.sh                 # compile only
./upload.sh                # compile + OTA upload; finds the board named "thermostat" on the LAN
./upload.sh 192.168.1.50  # OTA upload to a specific address
./upload.sh usb [/dev/ttyUSB0]   # USB upload (first flash, or recovery)
```

`upload.sh` reads the OTA password from `Credentials.h`. Keep the default board options:
the `mmu=4816` (16 KB cache) layout frees IRAM but breaks DS18B20 timing, because OneWire's
bit-banging code isn't placed in IRAM on the ESP8266.

## WiFi and recovery without USB

Open **`http://<device-ip>/wifi`** (also linked from the main page):

- Up to **5 networks**, joined by best signal. The compiled-in `WIFI_SSID` is always tried as
  a last resort, so editing the list can't remove the known-good network. Leave a password
  blank to keep the saved one; saved passwords are never shown.
- **Rollback:** settings saved while offline are reverted if no network connects within 3 min.
- **Fallback access point:** after 5 minutes without any network, the board starts its own
  WPA2 access point (default `thermostat-setup` / `thermostat`, changeable on the same page).
  Join it and the settings page opens automatically (captive portal); otherwise browse to
  `http://192.168.4.1`. OTA works over the AP too: `./upload.sh 192.168.4.1`.
- **Test access point** starts the AP for 3 minutes without dropping the current connection.

Temperature control keeps running throughout; only the network link changes.

## Local Web Interface

`http://<device-ip>/`. State-changing endpoints require **POST** plus the header
`X-Requested-With: thermostat`, so other websites can't drive the relays through a
browser on the LAN.

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Main control page |
| `/status` | GET | JSON: temperature (null if unknown), sensor fault, relays |
| `/setmode?relay=0-3&mode=AUTO\|ON\|OFF` | POST | Relay mode |
| `/settype?relay=0-3&type=HEATING\|COOLING\|GENERIC\|MANUAL_ONLY` | POST | Relay type |
| `/setthresholds?relay=0-3&on=°C&off=°C` | POST | Thresholds (validated) |
| `/setfreq?sec=5-300` | POST | Reading interval |
| `/setunit?fahrenheit=0\|1` | POST | Display unit |
| `/cleardata` | POST | Clear local temperature history |
| `/wifi` | GET / POST | WiFi settings page / save settings (JSON body) |
| `/wifi.json` | GET | WiFi status (no passwords) |
| `/wifi/testap` | POST | Start the access point for 3 minutes |
| `/logs` | GET | Log page (times in the browser's local time) |
| `/logs.json?limit=N&all=1` | GET | Logs as JSON: `epoch` (UTC seconds), `time` (UTC label), `msg`. CORS-enabled for the dashboard |
| `/data` | GET | Local temperature history (CSV, UTC epoch) |
| `/sensordiag` | GET | DS18B20 bus diagnostics (see Troubleshooting) |

```bash
curl http://192.168.1.50/status
curl -X POST -H 'X-Requested-With: thermostat' "http://192.168.1.50/setmode?relay=0&mode=AUTO"
```

## Backend Communication

| What | When | Endpoint |
|------|------|----------|
| Register | At boot, retried every 60 s until it succeeds. Re-registering an existing device sends its current token (the server refuses re-registration by MAC alone). | `POST /api/devices/register` |
| Heartbeat | Every 30 s: sensor fault, seconds since last good reading, firmware version, health | `POST /api/devices/{id}/heartbeat` |
| Readings | Batches of up to 30 from the outbox, with device timestamps | `POST /api/devices/{id}/temperature/batch` |
| Relay state | On change only (identical states are skipped) | `POST /api/devices/{id}/relay-state` |
| Commands | Fetched immediately when a server reply reports `pending_commands > 0`; safety poll every 5 min | `GET /api/devices/{id}/commands/pending` |

Against an older backend without the batch endpoint, the firmware falls back to single
readings and re-checks every 10 minutes.

### HTTPS

The production server doesn't support TLS max-fragment-length negotiation, so HTTPS would
need a ~17 KB receive buffer. The ESP8266 has ~22 KB free heap, which isn't enough to do that
safely, so `API_URL` uses `http://`. Options for the future: HMAC request signing, or moving
the server link to an ESP32 node. The backend can refuse plain-HTTP device calls with
`API_REQUIRE_HTTPS=true` once devices support it.

## Troubleshooting

### Sensor errors ("not-found", "no-response")

1. Open `/sensordiag`. It scans the bus and does one direct read, reporting: presence pulse,
   devices found, parasite-power mode, address/scratchpad CRCs and the temperature.
2. If `/sensordiag` reads fine but scheduled reads fail, it's a firmware read-path issue.
3. If `/sensordiag` fails too: check the sensor's VDD, ground and data wires and the 4.7k
   pull-up. If it reports `parasitePower: true`, the VDD wire is disconnected.
4. Check the Device Health panel on the dashboard: repeated "Power on" or "Reset button /
   power glitch" restarts point to the supply connection.

### Can't reach the device

- After 5 minutes offline it starts the `thermostat-setup` access point (see above).
- `arduino-cli board list` shows it as `thermostat at <ip>` when it's on the LAN.

### Relay doesn't switch immediately in AUTO

Expected: the 2-minute minimum on/off time. Changing the mode or thresholds applies at once.
