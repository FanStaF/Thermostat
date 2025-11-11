# Laravel Integration Plan for ESP8266 Thermostat

## Overview
Extend the thermostat system with a Laravel backend for better data persistence, visualization, and remote control capabilities.

## Architecture

```
┌─────────────────┐
│   ESP8266       │
│   Thermostat    │◄──── Local Network
│                 │
└────────┬────────┘
         │ HTTPS/REST API
         │
┌────────▼────────┐
│   Laravel       │
│   Backend       │
│   + Database    │
└────────┬────────┘
         │
┌────────▼────────┐
│   Web Dashboard │
│   (Laravel UI)  │◄──── Internet Access
└─────────────────┘
```

## Design Decisions

### Option 1: Hybrid Approach (Recommended)
- **ESP8266 keeps local web interface** for reliability
- **Laravel adds cloud features**: Historical data, remote access, advanced analytics
- **Benefits**: Works offline, cloud backup, best of both worlds

### Option 2: Full Laravel Migration
- **Remove ESP8266 web interface**, only API client
- **All control through Laravel**
- **Benefits**: Single source of truth, simpler maintenance
- **Drawbacks**: Requires internet, single point of failure

**Recommendation: Option 1** - Keep local interface for reliability, add Laravel for enhanced features.

---

## Database Schema

### 1. `devices` Table
```sql
- id (PK)
- name (e.g., "Living Room Thermostat")
- hostname (e.g., "thermostat")
- ip_address
- mac_address
- firmware_version
- last_seen_at
- is_online (boolean)
- created_at, updated_at
```

### 2. `temperature_readings` Table
```sql
- id (PK)
- device_id (FK)
- temperature (decimal 5,2)
- sensor_id (int, default 0)
- recorded_at (timestamp with precision)
- created_at
```
**Index:** (device_id, recorded_at)

### 3. `relays` Table
```sql
- id (PK)
- device_id (FK)
- relay_number (1-4)
- name (e.g., "Zone 1", "Basement")
- created_at, updated_at
```
**Unique:** (device_id, relay_number)

### 4. `relay_states` Table
```sql
- id (PK)
- relay_id (FK)
- state (boolean: ON/OFF)
- mode (enum: AUTO, MANUAL_ON, MANUAL_OFF)
- temp_on (decimal 5,2)
- temp_off (decimal 5,2)
- changed_at (timestamp)
- created_at
```
**Index:** (relay_id, changed_at)

### 5. `relay_events` Table (Optional - for detailed logs)
```sql
- id (PK)
- relay_id (FK)
- event_type (enum: state_change, mode_change, threshold_change)
- old_value (json)
- new_value (json)
- triggered_by (enum: auto, user, api, schedule)
- created_at
```

### 6. `device_settings` Table
```sql
- id (PK)
- device_id (FK)
- update_frequency (int, seconds)
- use_fahrenheit (boolean)
- timezone (string)
- settings_json (json, for flexible future settings)
- updated_at
```

### 7. `users` Table (Laravel default)
```sql
- id (PK)
- name
- email
- password
- remember_token
- created_at, updated_at
```

### 8. `device_user` Table (Many-to-many)
```sql
- device_id (FK)
- user_id (FK)
- role (enum: owner, admin, viewer)
- created_at
```

---

## API Endpoints

### ESP8266 → Laravel (Data Push)

#### POST `/api/devices/register`
Register/update device information
```json
{
  "hostname": "thermostat",
  "ip_address": "192.168.1.67",
  "mac_address": "AA:BB:CC:DD:EE:FF",
  "firmware_version": "1.0.0"
}
```
**Response:** `{ "device_id": 1, "api_token": "..." }`

#### POST `/api/devices/{device_id}/temperature`
Send temperature reading
```json
{
  "temperature": 23.5,
  "sensor_id": 0,
  "recorded_at": "2025-11-11T12:30:00Z"
}
```
**Response:** `{ "success": true }`

#### POST `/api/devices/{device_id}/relay-states`
Send current relay states
```json
{
  "relays": [
    { "relay_number": 1, "state": true, "mode": "AUTO", "temp_on": 25.0, "temp_off": 23.0 },
    { "relay_number": 2, "state": false, "mode": "MANUAL_OFF", "temp_on": 25.0, "temp_off": 23.0 },
    { "relay_number": 3, "state": false, "mode": "AUTO", "temp_on": 25.0, "temp_off": 23.0 },
    { "relay_number": 4, "state": false, "mode": "AUTO", "temp_on": 25.0, "temp_off": 23.0 }
  ],
  "timestamp": "2025-11-11T12:30:00Z"
}
```

#### POST `/api/devices/{device_id}/heartbeat`
Keep-alive signal
```json
{
  "uptime": 86400,
  "free_heap": 45000,
  "wifi_rssi": -45
}
```

### Laravel → ESP8266 (Command/Control)

#### GET `/api/devices/{device_id}/commands/pending`
ESP8266 polls for pending commands
**Response:**
```json
{
  "commands": [
    {
      "id": 123,
      "type": "set_relay_mode",
      "params": { "relay": 1, "mode": "AUTO" }
    },
    {
      "id": 124,
      "type": "set_thresholds",
      "params": { "relay": 2, "temp_on": 26.0, "temp_off": 24.0 }
    }
  ]
}
```

#### POST `/api/devices/{device_id}/commands/{command_id}/acknowledge`
ESP8266 confirms command execution
```json
{
  "status": "success",
  "result": { "relay": 1, "new_mode": "AUTO" }
}
```

### Web Dashboard → Laravel

#### GET `/api/devices`
List all devices for authenticated user

#### GET `/api/devices/{device_id}/temperature-history?from=...&to=...&interval=...`
Get temperature data for charts
**Query params:**
- `from`: Start timestamp
- `to`: End timestamp (default: now)
- `interval`: 1m, 5m, 15m, 1h, 6h, 1d (aggregation)

**Response:**
```json
{
  "data": [
    { "timestamp": "2025-11-11T12:00:00Z", "temperature": 23.5, "avg": 23.5, "min": 23.2, "max": 23.8 },
    { "timestamp": "2025-11-11T12:05:00Z", "temperature": 23.6, "avg": 23.6, "min": 23.3, "max": 23.9 }
  ]
}
```

#### POST `/api/devices/{device_id}/relays/{relay_number}/mode`
Change relay mode from dashboard
```json
{
  "mode": "MANUAL_ON"
}
```

#### PUT `/api/devices/{device_id}/relays/{relay_number}/thresholds`
Update temperature thresholds
```json
{
  "temp_on": 26.0,
  "temp_off": 24.0
}
```

---

## ESP8266 Firmware Changes

### New Module: `ApiClient.h/cpp`

**Responsibilities:**
- Send temperature readings to Laravel (configurable interval)
- Send relay state changes
- Poll for pending commands
- Handle command execution
- Manage API authentication token

**Configuration:**
```cpp
// Add to Credentials.h
#define LARAVEL_API_URL "https://your-domain.com/api"
#define LARAVEL_API_TOKEN "your-device-token"
#define API_SYNC_INTERVAL 60  // seconds
```

**Key Functions:**
```cpp
class ApiClient {
public:
  void begin();
  void loop();  // Called from main loop

  void sendTemperature(float temp, int sensorId);
  void sendRelayStates();
  void sendHeartbeat();

  bool pollCommands();
  void executeCommand(Command cmd);

private:
  unsigned long lastSync;
  HTTPClient http;
  String apiToken;
};
```

### Modified Files:
1. **Thermostat.ino** - Add ApiClient instance and loop call
2. **TemperatureManager.cpp** - Call `apiClient.sendTemperature()` after logging
3. **RelayController.cpp** - Call `apiClient.sendRelayStates()` on state change
4. **ConfigManager** - Add API configuration options

---

## Laravel Implementation Phases

### Phase 1: Core API (Week 1)
- [ ] Laravel project setup
- [ ] Database migrations
- [ ] Device registration endpoint
- [ ] Temperature data ingestion endpoint
- [ ] Basic authentication (API tokens)

### Phase 2: Command & Control (Week 2)
- [ ] Command queue system
- [ ] Relay control endpoints
- [ ] Settings management
- [ ] Command polling endpoint

### Phase 3: Dashboard UI (Week 3)
- [ ] User authentication (Laravel Breeze/Jetstream)
- [ ] Device management interface
- [ ] Real-time temperature display
- [ ] Relay control panel
- [ ] Basic charts (Chart.js or ApexCharts)

### Phase 4: Advanced Features (Week 4)
- [ ] Historical data visualization
- [ ] Date range selectors
- [ ] Data export (CSV/Excel)
- [ ] Alerts/notifications (email/SMS)
- [ ] Multi-device support
- [ ] Mobile-responsive design

### Phase 5: ESP8266 Integration (Week 5)
- [ ] Implement ApiClient module
- [ ] Test data synchronization
- [ ] Test command execution
- [ ] Handle offline scenarios
- [ ] Connection retry logic

---

## Technology Stack

### Laravel Backend
- **Framework:** Laravel 11.x
- **Database:** MySQL 8.0 or PostgreSQL
- **Cache:** Redis (for command queue)
- **Queue:** Laravel Queues + Redis
- **API:** RESTful with Laravel Sanctum for auth

### Frontend (Dashboard)
- **CSS Framework:** Tailwind CSS (Laravel default)
- **Charts:** ApexCharts or Chart.js
- **Real-time:** Laravel Echo + Pusher or Laravel Websockets
- **State Management:** Alpine.js (included with Breeze) or Vue.js

### ESP8266
- **HTTP Client:** ESP8266HTTPClient library
- **JSON:** ArduinoJson (already installed)
- **SSL/TLS:** WiFiClientSecure for HTTPS

---

## Data Flow Examples

### Temperature Reading Flow
```
1. ESP8266 reads temperature (every 5s)
2. ESP8266 logs to local LittleFS
3. ESP8266 sends to Laravel API (every 60s)
4. Laravel stores in database
5. Dashboard polls/subscribes for updates
6. Chart updates in real-time
```

### Relay Control Flow
```
1. User clicks "Turn On" in dashboard
2. Laravel creates command in queue
3. ESP8266 polls for commands (every 30s)
4. ESP8266 receives command
5. ESP8266 executes command (turns on relay)
6. ESP8266 sends acknowledgment + new state
7. Laravel updates database
8. Dashboard refreshes state
```

---

## Security Considerations

1. **HTTPS Only:** All API communication over TLS
2. **API Token Auth:** Device-specific tokens (not user passwords)
3. **Rate Limiting:** Prevent API abuse
4. **Input Validation:** Sanitize all inputs
5. **CORS:** Configure properly for dashboard access
6. **Token Rotation:** Support token refresh/revocation
7. **User Permissions:** Role-based access (owner/admin/viewer)

---

## Offline Handling

### ESP8266 Strategy
- Continue operating normally if API unavailable
- Queue failed API calls (limited buffer)
- Retry with exponential backoff
- Log sync status in system logs
- Keep local web interface functional

### Laravel Strategy
- Show "last seen" timestamp for devices
- Mark devices as offline after timeout (5 minutes)
- Store last known state
- Queue commands for offline devices

---

## Configuration File Changes

### Add to `Config.h`
```cpp
// ---- API Configuration ----
constexpr int API_SYNC_INTERVAL = 60;        // seconds
constexpr int API_COMMAND_POLL_INTERVAL = 30; // seconds
constexpr int API_RETRY_DELAY = 5000;        // ms
constexpr int API_MAX_RETRIES = 3;
```

### Add to `Credentials.h`
```cpp
// Laravel API Configuration
#define LARAVEL_API_URL "https://thermostat.yourdomain.com/api"
#define LARAVEL_API_TOKEN "your-secure-device-token-here"
#define LARAVEL_API_ENABLED true  // Toggle API integration on/off
```

---

## Testing Strategy

### Unit Tests (Laravel)
- API endpoint tests
- Database model tests
- Command execution tests

### Integration Tests
- ESP8266 → Laravel data flow
- Laravel → ESP8266 commands
- Offline recovery scenarios

### Load Tests
- Multiple devices sending data
- High-frequency temperature readings
- Concurrent dashboard users

---

## Deployment

### Laravel Hosting Options
1. **Shared Hosting:** Not recommended (limited control)
2. **VPS:** DigitalOcean, Linode, Vultr ($5-10/month)
3. **Platform as a Service:** Laravel Forge, Ploi, Heroku
4. **Serverless:** Laravel Vapor (AWS Lambda)

### Database Hosting
- Same VPS as Laravel
- Managed database (AWS RDS, DigitalOcean Managed DB)

### Domain & SSL
- Purchase domain
- Use Let's Encrypt for free SSL
- Configure DNS A record

---

## Estimated Timeline

| Phase | Duration | Deliverable |
|-------|----------|-------------|
| Planning & Setup | 2 days | Architecture finalized, Laravel installed |
| Database & Models | 2 days | Migrations, models, seeders |
| API Endpoints | 3 days | All endpoints functional |
| Dashboard UI | 5 days | Basic dashboard with charts |
| ESP8266 Integration | 3 days | ApiClient module complete |
| Testing & Debugging | 3 days | End-to-end tests passing |
| Deployment | 2 days | Production deployment |
| **Total** | **~3 weeks** | Full integration complete |

---

## Next Steps

1. **Review this plan** and decide on options (Hybrid vs Full migration)
2. **Set up Laravel project** locally
3. **Create database schema** (migrations)
4. **Implement basic API endpoints** for testing
5. **Test with Postman** before ESP8266 integration
6. **Implement ApiClient module** on ESP8266
7. **Build dashboard** incrementally

---

## Questions to Consider

1. **Single vs Multi-device?** Will you monitor multiple thermostats?
2. **User authentication?** Need login or just single-user?
3. **Real-time updates?** WebSockets or polling?
4. **Mobile app?** Future consideration (React Native, Flutter)
5. **Scheduling?** Time-based temperature programs?
6. **Alerts?** Email/SMS notifications for temperature thresholds?
7. **Historical data retention?** How long to keep data (90 days, 1 year, forever)?

Let me know your preferences and we can start implementation!
