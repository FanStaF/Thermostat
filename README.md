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
│   └── ApiClient.*        # Laravel backend communication
├── backend/               # Laravel backend (PHP)
│   ├── app/
│   ├── database/
│   ├── routes/
│   └── resources/views/
└── README.md              # This file
```

## Features

### Firmware
- 4 independent relay controllers
- **Relay types**: HEATING, COOLING, GENERIC, MANUAL_ONLY
- AUTO mode with hysteresis and type-aware temperature logic
- MANUAL ON/OFF modes
- Local web interface (works offline)
- Temperature logging to LittleFS
- Real-time temperature charts
- Celsius/Fahrenheit support
- OTA firmware updates
- Persistent settings
- System logs with filtering and pagination
- API client for backend communication

### Backend (Laravel)
- Device registration and authentication
- Temperature data storage and history
- Remote relay control via command queue
- Dashboard with real-time charts
- Device logs viewer (fetches from device)
- Email reports with relay activity statistics
- Alert notifications for temperature thresholds
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
         │ HTTPS REST API
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

### Device → Backend
- `POST /api/devices/register` - Register device, get auth token
- `POST /api/devices/{id}/heartbeat` - Keep-alive signal
- `POST /api/devices/{id}/temperature` - Send temperature reading
- `POST /api/devices/{id}/relay-state` - Send relay state update
- `GET /api/devices/{id}/commands/pending` - Poll for commands
- `PUT /api/devices/{id}/commands/{cmd}` - Acknowledge command

### Dashboard → Backend
- `GET /dashboard` - Device list
- `GET /dashboard/{id}` - Device detail with charts
- `POST /api/devices/{id}/commands` - Queue command for device
- `PUT /api/relays/{id}` - Update relay settings

## Configuration

### Firmware (`Credentials.h`)
```cpp
#define WIFI_SSID "your-wifi"
#define WIFI_PASSWORD "your-password"
#define API_URL "https://your-server.com"
#define API_KEY "your-api-key"
```

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
# Make changes
arduino-cli compile --fqbn esp8266:esp8266:d1_mini_clone .
arduino-cli upload --fqbn esp8266:esp8266:d1_mini_clone --port <ip-or-port> .
```

### Backend
```bash
cd backend
php artisan serve        # Start dev server
php artisan migrate      # Run migrations
php artisan test         # Run tests
```

## Changelog

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
