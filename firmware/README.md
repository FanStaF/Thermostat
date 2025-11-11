# ESP8266 Thermostat System

Multi-relay thermostat controller with ESP8266 firmware and Laravel backend for advanced monitoring and control.

## Project Structure

This is a monorepo containing both the ESP8266 firmware and Laravel backend:

```
Thermostat/
├── firmware/              # ESP8266 firmware (Arduino/C++)
│   ├── Thermostat.ino    # Main sketch
│   ├── Config.h          # Pin definitions and constants
│   ├── Credentials.h     # WiFi credentials (gitignored)
│   ├── SystemLogger.*    # Logging system
│   ├── TemperatureManager.* # DS18B20 sensor management
│   ├── RelayController.* # Relay control logic
│   ├── ConfigManager.*   # Settings persistence
│   ├── WebInterface.*    # Local web UI
│   └── README.md         # Firmware-specific documentation
├── backend/              # Laravel backend (PHP)
│   ├── app/
│   ├── database/
│   ├── routes/
│   └── README.md         # Backend-specific documentation
├── docs/                 # Documentation
│   └── LARAVEL_INTEGRATION_PLAN.md
└── README.md            # This file
```

## Components

### Firmware (ESP8266)
Arduino-based firmware for the LOLIN(WEMOS) D1 mini with:
- 4-relay control (AUTO/MANUAL modes)
- DS18B20 temperature monitoring
- Local web interface
- OTA firmware updates
- LittleFS configuration storage
- **NEW:** API client for Laravel backend integration

**See:** [firmware/README.md](firmware/README.md)

### Backend (Laravel)
Laravel application providing:
- RESTful API for device communication
- Historical temperature data storage
- Advanced charting and analytics
- Remote device control
- Multi-user authentication
- Mobile-responsive dashboard

**See:** [backend/README.md](backend/README.md) *(Coming soon)*

## Hardware

- **Board:** LOLIN(WEMOS) D1 mini (ESP8266)
- **Sensor:** DS18B20 temperature sensor on pin D2
- **Relays:** 4x relays on pins D1, D5, D6, D7 (active-LOW)

## Features

### Current (Firmware Only)
- ✓ 4 independent relay controllers
- ✓ AUTO mode with hysteresis
- ✓ MANUAL ON/OFF modes
- ✓ Local web interface
- ✓ Temperature logging to LittleFS
- ✓ Basic charts
- ✓ Celsius/Fahrenheit support
- ✓ OTA updates
- ✓ Persistent settings

### Coming Soon (Laravel Integration)
- ⏳ Cloud data storage
- ⏳ Advanced historical charts
- ⏳ Remote access from anywhere
- ⏳ Multi-device support
- ⏳ User authentication
- ⏳ Email/SMS alerts
- ⏳ Scheduling and automation
- ⏳ Data export (CSV/Excel)
- ⏳ Mobile-responsive dashboard

## Quick Start

### 1. Firmware Setup

```bash
cd firmware
cp Credentials.h.example Credentials.h
# Edit Credentials.h with your WiFi details
./build.sh
./upload.sh
```

See [firmware/README.md](firmware/README.md) for detailed instructions.

### 2. Backend Setup (Coming Soon)

```bash
cd backend
composer install
cp .env.example .env
php artisan key:generate
php artisan migrate
php artisan serve
```

See [backend/README.md](backend/README.md) for detailed instructions.

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
│   Backend       │
│   + Database    │
└────────┬────────┘
         │
┌────────▼────────┐
│   Web Dashboard │◄──── Internet Access (Remote)
└─────────────────┘
```

**Hybrid Approach:**
- ESP8266 has local web interface (works offline)
- Laravel adds cloud features (historical data, remote access)
- Best of both worlds: reliability + advanced features

## Development

### Branch Strategy
- `main` - Stable releases
- `feature/*` - Feature branches (e.g., `feature/laravel-integration`)

### Firmware Development
```bash
cd firmware
# Make changes
arduino-cli compile --fqbn esp8266:esp8266:d1_mini_clone .
arduino-cli upload --fqbn esp8266:esp8266:d1_mini_clone --port 192.168.1.67 .
```

### Backend Development
```bash
cd backend
php artisan serve
php artisan test
```

## Contributing

This is a personal project, but feel free to fork and adapt for your own use.

## Documentation

- [Integration Plan](docs/LARAVEL_INTEGRATION_PLAN.md) - Complete Laravel integration architecture
- [Firmware README](firmware/README.md) - ESP8266 firmware details
- [Backend README](backend/README.md) - Laravel backend details *(Coming soon)*

## License

Project created with Claude Code.

## Changelog

### v2.0.0 (In Development) - Laravel Integration
- Reorganized into monorepo structure
- Laravel backend development in progress
- API client module for firmware

### v1.0.0 - Initial Release
- Modular ESP8266 firmware
- Local web interface
- 4-relay control
- Temperature monitoring and logging
