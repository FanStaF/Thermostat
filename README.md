# ESP8266 Thermostat Controller

Multi-relay thermostat controller with web interface and temperature monitoring.

## Project Structure

```
Thermostat/
├── Config.h                  # Pin definitions and constants
├── SystemLogger.h/cpp        # Logging system
├── TemperatureManager.h/cpp  # DS18B20 sensor management
├── RelayController.h/cpp     # Relay control logic
├── ConfigManager.h/cpp       # Settings persistence
├── WebInterface.h/cpp        # Web UI and HTTP handlers
└── Thermostat.ino           # Main sketch
```

## Hardware

- **Board:** LOLIN(WEMOS) D1 mini (ESP8266)
- **Sensor:** DS18B20 temperature sensor on pin D2
- **Relays:** 4x relays on pins D1, D5, D6, D7 (active-LOW)

## Features

- 4 independent relay controllers
- AUTO mode with hysteresis (temperature-based control)
- MANUAL ON/OFF modes
- Web interface with live temperature monitoring
- Temperature data logging with charts
- Celsius/Fahrenheit support
- OTA firmware updates
- Persistent settings (saved to LittleFS)

## Building & Uploading

### Command Line

**Compile only:**
```bash
arduino-cli compile --fqbn esp8266:esp8266:d1_mini_clone .
```

**Upload via OTA:**
```bash
arduino-cli compile --fqbn esp8266:esp8266:d1_mini_clone . && \
arduino-cli upload --fqbn esp8266:esp8266:d1_mini_clone --port 192.168.1.67 .
```

**Upload via USB:**
```bash
arduino-cli compile --fqbn esp8266:esp8266:d1_mini_clone . && \
arduino-cli upload --fqbn esp8266:esp8266:d1_mini_clone --port /dev/ttyUSB0 .
```

### Using Build Scripts

**Compile:**
```bash
./build.sh
```

**Upload (OTA):**
```bash
./upload.sh
```

**Upload (USB):**
```bash
./upload.sh usb
./upload.sh usb /dev/ttyUSB1  # specify different port
```

### Neovim Integration

#### Option 1: Auto-load for this project (Lua)

Add to your `~/.config/nvim/init.lua`:
```lua
-- Auto-source project-specific config
vim.api.nvim_create_autocmd({'BufEnter'}, {
  pattern = '/home/fanstaf/Arduino/Thermostat/*',
  callback = function()
    vim.cmd('luafile /home/fanstaf/Arduino/Thermostat/.nvim.lua')
  end,
  once = true,
})
```

#### Option 2: Manual source (Lua)
```vim
:luafile .nvim.lua
```

#### Option 3: VimScript version
```vim
:source .nvimrc
```

#### Keyboard Shortcuts (Lua config)

- `<leader>ac` - Compile (opens terminal split)
- `<leader>au` - Upload via OTA (compile + upload)
- `<leader>aU` - Upload via USB (prompts for port)
- `<leader>ab` - Quick build (compile only, minimal output)
- `<leader>am` - Serial monitor (115200 baud)

Replace `<leader>` with your leader key (usually `\` or Space).

## Dependencies

### Arduino Libraries (already installed)
- ESP8266WiFi (included with ESP8266 core)
- ESP8266WebServer (included with ESP8266 core)
- ArduinoOTA (included with ESP8266 core)
- OneWire
- DallasTemperature
- LittleFS (included with ESP8266 core)
- ArduinoJson

### Board Support
- ESP8266 Arduino Core 3.1.2

## Configuration

### WiFi Credentials
**First time setup:**
1. Copy `Credentials.h.example` to `Credentials.h`
2. Edit `Credentials.h` with your WiFi credentials:
```cpp
#define WIFI_SSID "your-ssid"
#define WIFI_PASSWORD "your-password"
```
3. `Credentials.h` is in `.gitignore` and will never be committed

### Pin Assignments
Edit `Config.h` lines 8-9:
```cpp
#define ONE_WIRE_BUS D2
const int RELAY_PINS[4] = { D1, D5, D6, D7 };
```

## Usage

1. Upload firmware to ESP8266
2. Connect to WiFi network
3. Find IP address in serial monitor
4. Open web browser to `http://[IP-ADDRESS]`
5. Configure relay modes and temperature thresholds
6. Monitor temperature and relay states

## Web Interface

- **Main page:** Real-time temperature, relay controls, settings
- **Logs page:** System event logs
- **Data endpoint:** Temperature history (CSV format)

## Safety Features

- Temperature sensor validation (3 samples with consistency check)
- Sensor error handling (uses last known good value)
- Valid temperature range checking (-50°C to 85°C)
- Configuration persistence to survive power loss
- Watchdog feeding during long operations

## License

Project created with Claude Code.
