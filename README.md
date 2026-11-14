# ESP8266 Thermostat System

Multi-relay thermostat controller with ESP8266 firmware and Laravel backend for monitoring and control.

## Project Structure

```
Thermostat/
├── firmware/Thermostat/   # ESP8266 firmware (Arduino/C++)
│   ├── Thermostat.ino     # Main sketch
│   ├── Config.h           # Pin definitions and constants
│   ├── Credentials.h      # WiFi/API credentials (gitignored)
│   ├── SystemLogger.*     # In-memory logging system
│   ├── TemperatureManager.* # DS18B20 sensor management
│   ├── RelayController.*  # Relay control with type-aware logic
│   ├── ConfigManager.*    # Settings persistence (LittleFS)
│   ├── WebInterface.*     # Local web UI
│   ├── NetManager.*       # WiFi networks, fallback access point, captive portal
│   ├── ApiClient.*        # Backend communication + reading outbox
│   └── build.sh / upload.sh
├── backend/               # Laravel backend (PHP)
│   ├── app/
│   ├── database/
│   ├── routes/
│   ├── resources/views/
│   ├── tests/
│   └── deploy.sh          # Production deploy (Hostinger)
├── .github/workflows/     # CI (tests + firmware build) and manual deploy
└── README.md              # This file

See `firmware/README.md` for firmware details and `DEPLOYMENT.md` for production.
```

## Features

### Firmware
- 4 independent relay controllers
- **Relay types**: HEATING, COOLING, GENERIC, MANUAL_ONLY
- AUTO mode with hysteresis, type-aware logic and a 2-minute minimum on/off time
- MANUAL ON/OFF modes
- **Sensor fail-safe**: AUTO relays turn OFF when the sensor stops giving valid readings
- **Readings outbox**: readings queued on flash with timestamps, uploaded in batches, purged once confirmed
- **WiFi fallbacks**: up to 5 saved networks, rollback, own access point + captive portal
- Device health reporting (restarts and reasons, WiFi signal, memory, upload backlog)
- Local web interface (works offline) with a sensor diagnostics page
- Password-protected OTA firmware updates
- All times stored in UTC, shown in the browser's local time

### Backend (Laravel)
- Device registration and token authentication (re-registration requires the device's token or admin approval)
- Temperature history with batched, timestamped, idempotent uploads
- Remote relay control via command queue (devices are told in each reply when commands are waiting)
- Dashboard that refreshes at the device's update interval and only when data changed; live "last seen" timers
- Sensor-fault banner and Device Health panel
- Device logs viewer (fetched from the device by the browser)
- Email reports and alerts (incl. immediate sensor-fault alerts)
- Per-user device access, login rate limiting
- Multi-device support

## Hardware

- **Board:** LOLIN(WEMOS) D1 mini (ESP8266)
- **Sensor:** DS18B20 temperature sensor on pin D2
- **Relays:** 4x relays on pins D1, D5, D6, D7 (active-LOW)

## Relay Types

| Type | Description | ON Condition | OFF Condition |
|------|-------------|--------------|---------------|
| HEATING | Floor heat, heaters | Temp < ON threshold | Temp > OFF threshold |
| COOLING | AC, fans | Temp > ON threshold | Temp < OFF threshold |
| GENERIC | General purpose | Temp > ON threshold | Temp < OFF threshold |
| MANUAL_ONLY | No auto control | Manual only | Manual only |

**Example (HEATING):** ON=65°F, OFF=70°F
- Turns ON when temp drops below 65°F
- Turns OFF when temp rises above 70°F

## Quick Start

### 1. Firmware Setup

```bash
cd firmware/Thermostat
cp Credentials.h.example Credentials.h
# Edit Credentials.h with your WiFi and API details
```

Build and upload using Arduino IDE or CLI:
```bash
arduino-cli compile --fqbn esp8266:esp8266:d1_mini_clone .
arduino-cli upload --fqbn esp8266:esp8266:d1_mini_clone --port /dev/ttyUSB0 .
```

Or use OTA upload (after initial flash):
```bash
arduino-cli upload --fqbn esp8266:esp8266:d1_mini_clone --port <device-ip> .
```

### 2. Backend Setup

```bash
cd backend
composer install
cp .env.example .env
php artisan key:generate
# Configure database in .env
php artisan migrate
php artisan serve
```

## Architecture

```
┌─────────────────┐
│   ESP8266       │
│   Thermostat    │◄──── Local Network (Direct Access)
│                 │
└────────┬────────┘
         │ HTTP REST API (see "HTTPS" in firmware/README.md)
         │
┌────────▼────────┐
│   Laravel       │
│   Backend       │◄──── Internet Access (Dashboard)
│   + MySQL       │
└─────────────────┘
```

**Hybrid Approach:**
- ESP8266 has local web interface (works offline)
- Laravel adds cloud features (historical data, remote access, alerts)
- Best of both worlds: reliability + advanced features

## API Endpoints

### Device → Backend (device token, scoped to its own device)
- `POST /api/devices/register` - Register (API key; existing devices must also present their current token)
- `POST /api/devices/{id}/heartbeat` - Keep-alive + sensor fault + health; reply includes `pending_commands`
- `POST /api/devices/{id}/temperature/batch` - Up to 100 readings with device timestamps (idempotent)
- `POST /api/devices/{id}/temperature` - Single reading (older firmware)
- `POST /api/devices/{id}/relay-state` - Relay state update
- `GET /api/devices/{id}/commands/pending` - Fetch commands
- `PUT /api/devices/{id}/commands/{cmd}` - Report command status

### Dashboard (web session)
- `GET /` - Device list; `GET /dashboard/summary` - its live-refresh data
- `GET /devices/{id}` - Device detail; `GET /devices/{id}/dashboard-data` - its live-refresh data
- `POST /devices/{id}/commands` - Queue a command for the device
- `PATCH /devices/{id}/settings` - Update interval (5–300 s), unit, timezone
- `POST /devices/{id}/allow-reregistration` - Admin: let a device that lost its token register once

The user-facing `/api` routes accept user tokens only (device tokens are rejected).

## Configuration

### Firmware (`Credentials.h`, gitignored)
```cpp
#define WIFI_SSID "your-wifi"
#define WIFI_PASSWORD "your-password"
#define API_URL "http://your-server.com"
#define API_KEY "your-api-key"
#define OTA_PASSWORD "long-random-value"   // required
```
Additional WiFi networks and the fallback access point are configured on the device at `/wifi`.

### Backend (`.env`)
```env
DB_CONNECTION=mysql
DB_DATABASE=thermostat
MAIL_MAILER=smtp
# ... standard Laravel config
```

## Development

### Firmware
```bash
cd firmware/Thermostat
./build.sh               # compile
./upload.sh              # compile + OTA upload to the board named "thermostat"
./upload.sh <ip>         # OTA to a specific address; ./upload.sh usb for USB
```

### Backend
```bash
cd backend
php artisan serve        # Start dev server
php artisan migrate      # Run migrations
php artisan test         # Run tests (SQLite in memory)
```

### CI
Every push and pull request runs the backend tests and compiles the firmware
(`.github/workflows/ci.yml`). Deployment is manual (see `DEPLOYMENT.md`).

## Changelog

### Firmware v2.2 – v2.6 (November 2026)
- **Robustness:** sensor fail-safe; no invented temperatures; 85 °C power-on value rejected;
  validated thresholds/modes/config; atomic settings writes; capped temperature log; fixed-size
  log buffer; non-blocking sensor reads; sensor read by cached address with CRC retries;
  registration retries; 2-minute relay minimum on/off time
- **Communication:** reading outbox with batched, timestamped uploads; commands signalled in
  server replies; device health and sensor fault in heartbeats; re-registration requires the
  device token
- **Recovery:** up to 5 WiFi networks, settings rollback, fallback access point with captive
  portal, `/sensordiag`
- **Security:** POST + custom header for local controls, escaped log output, required OTA password
- Times kept in UTC and shown in the browser's local time

### Backend (November 2026)
- Security fixes from a full review: registration takeover, device tokens on the user API,
  alert subscriptions vs. device access, stored XSS in the log panel, login throttling
- Batch temperature endpoint, pending-command signalling, optional `API_REQUIRE_HTTPS`
- Live dashboard refresh (follows the device's update interval), sensor-fault banner,
  Device Health panel with restart history
- Fixes: alert check memory use, summary schedule times, missing device settings,
  relay on-time drift, live relay badges
- `deploy.sh` works without `proc_open`; CI workflow; test suite (39 tests)

### v2.1.0 - Relay Types & Logging Improvements
- Added relay types (HEATING, COOLING, GENERIC, MANUAL_ONLY)
- Type-aware temperature control logic with hysteresis
- Improved logging with filtering and pagination
- Device logs accessible from server dashboard
- CORS support for cross-origin log fetching
- Reduced verbose logging for cleaner output
- Memory optimization using PROGMEM for web interface

### v2.0.0 - Laravel Integration
- Laravel backend with MySQL database
- Device registration and authentication
- Remote relay control via command queue
- Dashboard with temperature charts
- Email reports and alerts
- Multi-device support

### v1.0.0 - Initial Release
- Modular ESP8266 firmware
- Local web interface
- 4-relay control
- Temperature monitoring and logging

## License

MIT License
