# ESP8266 Thermostat Firmware

Arduino firmware for the ESP8266-based thermostat controller.

## Hardware Requirements

- **Board:** LOLIN(WEMOS) D1 mini or compatible ESP8266
- **Sensor:** DS18B20 temperature sensor
- **Relays:** 4x relay module (active-LOW)

### Pin Configuration

| Pin | Function |
|-----|----------|
| D1 | Relay 1 |
| D2 | DS18B20 Data (with 4.7k pull-up) |
| D5 | Relay 2 |
| D6 | Relay 3 |
| D7 | Relay 4 |

## Features

- **4 Independent Relays** with individual control modes
- **Relay Types:** HEATING, COOLING, GENERIC, MANUAL_ONLY
- **Control Modes:** AUTO, MANUAL_ON, MANUAL_OFF
- **Temperature Monitoring** with DS18B20 sensor
- **Local Web Interface** (works without internet)
- **Backend Integration** via REST API
- **OTA Updates** for remote firmware updates
- **Persistent Settings** stored in LittleFS
- **System Logging** with filtering and remote access

## Relay Types

The relay type determines how temperature thresholds are interpreted:

### HEATING (e.g., floor heat, space heaters)
- **ON:** Temperature drops below ON threshold
- **OFF:** Temperature rises above OFF threshold
- Example: ON=65°F, OFF=70°F turns on heat when cold

### COOLING (e.g., AC, fans)
- **ON:** Temperature rises above ON threshold
- **OFF:** Temperature drops below OFF threshold
- Example: ON=75°F, OFF=70°F turns on cooling when hot

### GENERIC
- Same logic as COOLING (ON when temp > threshold)

### MANUAL_ONLY
- No automatic control, only responds to manual commands

## File Structure

```
Thermostat/
├── Thermostat.ino       # Main sketch
├── Config.h             # Pin definitions, constants
├── Credentials.h        # WiFi/API credentials (gitignored)
├── Credentials.h.example # Template for credentials
├── SystemLogger.h/cpp   # In-memory logging (500 entries)
├── TemperatureManager.h/cpp # DS18B20 sensor handling
├── RelayController.h/cpp # Relay logic with type support
├── ConfigManager.h/cpp  # Settings persistence
├── WebInterface.h/cpp   # Local web server
└── ApiClient.h/cpp      # Backend API communication
```

## Setup

### 1. Install Dependencies

Using Arduino IDE Library Manager or arduino-cli:

```bash
arduino-cli lib install "OneWire"
arduino-cli lib install "DallasTemperature"
arduino-cli lib install "ArduinoJson"
```

### 2. Configure Credentials

```bash
cp Credentials.h.example Credentials.h
```

Edit `Credentials.h`:
```cpp
#define WIFI_SSID "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"
#define API_URL "https://your-server.com"
#define API_KEY "your-api-key"
```

### 3. Build & Upload

```bash
# Compile
arduino-cli compile --fqbn esp8266:esp8266:d1_mini_clone .

# Upload via USB
arduino-cli upload --fqbn esp8266:esp8266:d1_mini_clone --port /dev/ttyUSB0 .

# Upload via OTA (after initial flash)
arduino-cli upload --fqbn esp8266:esp8266:d1_mini_clone --port 192.168.1.x .
```

## Web Interface

Access the local web interface at `http://<device-ip>/`

### Endpoints

| Endpoint | Method | Description |
|----------|--------|-------------|
| `/` | GET | Main control interface |
| `/status` | GET | JSON status of all relays |
| `/setmode` | GET | Set relay mode (AUTO/ON/OFF) |
| `/settype` | GET | Set relay type |
| `/setthresholds` | GET | Set temperature thresholds |
| `/setfreq` | GET | Set update frequency |
| `/setunit` | GET | Toggle Celsius/Fahrenheit |
| `/logs` | GET | System logs page |
| `/logs.json` | GET | System logs as JSON (CORS enabled) |
| `/data` | GET | Temperature history CSV |
| `/cleardata` | GET | Clear temperature history |

### Example API Calls

```bash
# Get status
curl http://192.168.1.x/status

# Set relay 1 to AUTO mode
curl "http://192.168.1.x/setmode?relay=0&mode=AUTO"

# Set relay 1 type to HEATING
curl "http://192.168.1.x/settype?relay=0&type=HEATING"

# Set thresholds (in Celsius)
curl "http://192.168.1.x/setthresholds?relay=0&on=18&off=21"

# Get logs as JSON
curl "http://192.168.1.x/logs.json?limit=100"
```

## Configuration Constants

Edit `Config.h` to customize:

```cpp
// Pins
constexpr uint8_t RELAY_PINS[] = {D1, D5, D6, D7};
constexpr uint8_t TEMP_SENSOR_PIN = D2;

// Defaults
constexpr float DEFAULT_TEMP_ON = 18.3;   // 65°F
constexpr float DEFAULT_TEMP_OFF = 21.1;  // 70°F
constexpr int DEFAULT_UPDATE_FREQUENCY = 5; // seconds

// Logging
constexpr int MAX_SYSTEM_LOGS = 500;

// API
constexpr int API_SYNC_INTERVAL = 60;     // seconds
constexpr int API_COMMAND_POLL_INTERVAL = 30; // seconds
```

## Backend Communication

The firmware communicates with the Laravel backend via REST API:

### Registration
On boot, registers with backend and receives auth token.

### Heartbeat
Sends periodic heartbeat to update online status.

### Temperature
Posts temperature readings at configured interval.

### Relay State
Reports relay state changes to backend.

### Commands
Polls for pending commands (mode changes, threshold updates, etc.)

## Logging

The firmware maintains an in-memory log of 500 entries. Logs are accessible via:

- **Local:** `http://<device-ip>/logs`
- **JSON API:** `http://<device-ip>/logs.json`
- **Server Dashboard:** Fetches logs via CORS-enabled endpoint

Log filtering hides repetitive messages (heartbeats, temp posts) by default.

## Memory Optimization

The web interface uses PROGMEM to store HTML/CSS/JS in flash memory, avoiding RAM allocation issues on the ESP8266's limited 80KB heap.

## Troubleshooting

### Web interface doesn't load
- Check free heap memory in logs
- Ensure only one client connects at a time
- Restart device if heap is fragmented

### Temperature reads -127°C
- Check DS18B20 wiring
- Verify 4.7k pull-up resistor on data line
- Ensure correct pin in Config.h

### API connection fails
- Verify WiFi connection
- Check API_URL in Credentials.h
- Ensure HTTPS certificate is valid (or use setInsecure())

### Relay cycles rapidly
- Increase hysteresis gap between ON and OFF thresholds
- Check relay type matches your use case (HEATING vs COOLING)
