# Thermostat Backend

Laravel backend for the ESP8266 Thermostat system.

## Requirements

- PHP 8.2+
- Composer
- MySQL 8.0+ or MariaDB
- Node.js 18+ (for frontend assets)

## Installation

```bash
cd backend
composer install
npm install
cp .env.example .env
php artisan key:generate
```

Configure database in `.env`:
```env
DB_CONNECTION=mysql
DB_HOST=127.0.0.1
DB_PORT=3306
DB_DATABASE=thermostat
DB_USERNAME=your_username
DB_PASSWORD=your_password
```

Run migrations:
```bash
php artisan migrate
```

Build frontend assets:
```bash
npm run build
```

## Development

```bash
php artisan serve
```

Access at http://localhost:8000

## Features

### Device Management
- Automatic device registration via API
- Token-based authentication for devices
- Online/offline status tracking
- Multi-device support

### Temperature Monitoring
- Store temperature readings from devices
- Historical data with configurable retention
- Temperature charts with date range selection

### Relay Control
- Remote relay mode control (AUTO/ON/OFF)
- Relay type configuration (HEATING/COOLING/GENERIC/MANUAL_ONLY)
- Temperature threshold management
- Command queue for device communication

### Dashboard
- Device overview with current status
- Real-time temperature display
- Relay control panel
- Temperature history charts
- Device logs viewer

### Alerts & Reports
- Email alerts for temperature thresholds
- Scheduled report emails with relay activity
- Configurable alert settings per device

## Database Schema

### Tables
- `devices` - Registered ESP8266 devices
- `temperature_readings` - Temperature history
- `relays` - Relay configuration (4 per device)
- `device_commands` - Command queue for devices
- `device_settings` - Per-device settings
- `alerts` - Alert configuration
- `users` - Dashboard users

## API Endpoints

### Device API (used by ESP8266)

| Method | Endpoint | Description |
|--------|----------|-------------|
| POST | `/api/devices/register` | Register device, get token |
| POST | `/api/devices/{id}/heartbeat` | Update online status |
| POST | `/api/devices/{id}/temperature` | Submit temperature reading |
| POST | `/api/devices/{id}/relay-state` | Update relay state |
| GET | `/api/devices/{id}/commands/pending` | Get pending commands |
| PUT | `/api/devices/{id}/commands/{cmd}` | Acknowledge command |

### Dashboard API

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/dashboard` | Device list |
| GET | `/dashboard/{id}` | Device detail |
| POST | `/api/devices/{id}/commands` | Queue command |
| PUT | `/api/relays/{id}` | Update relay settings |
| GET | `/api/devices/{id}/temperature-history` | Get chart data |

## Command Types

Commands queued for devices:

| Type | Params | Description |
|------|--------|-------------|
| `set_relay_mode` | `relay`, `mode` | Change relay mode |
| `set_relay_type` | `relay`, `type` | Change relay type |
| `set_thresholds` | `relay`, `temp_on`, `temp_off` | Set thresholds |
| `set_frequency` | `seconds` | Update frequency |
| `set_unit` | `fahrenheit` | Toggle temp unit |
| `restart` | - | Restart device |

## Configuration

### Environment Variables

```env
# App
APP_NAME="Thermostat Monitor"
APP_URL=https://your-domain.com

# Database
DB_CONNECTION=mysql
DB_DATABASE=thermostat

# Mail (for alerts/reports)
MAIL_MAILER=smtp
MAIL_HOST=smtp.example.com
MAIL_PORT=587
MAIL_USERNAME=your-email
MAIL_PASSWORD=your-password
MAIL_FROM_ADDRESS=thermostat@example.com

# Queue (for scheduled tasks)
QUEUE_CONNECTION=database
```

### Scheduled Tasks

Add to crontab for scheduled reports and alerts:
```bash
* * * * * cd /path/to/backend && php artisan schedule:run >> /dev/null 2>&1
```

## Deployment

### Production Setup

1. Configure web server (Nginx/Apache)
2. Set `APP_ENV=production` and `APP_DEBUG=false`
3. Run `php artisan config:cache`
4. Run `php artisan route:cache`
5. Set up SSL certificate
6. Configure queue worker for background jobs

### Example Nginx Config

```nginx
server {
    listen 443 ssl;
    server_name thermostat.example.com;
    root /var/www/thermostat/backend/public;

    ssl_certificate /etc/letsencrypt/live/thermostat.example.com/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/thermostat.example.com/privkey.pem;

    index index.php;

    location / {
        try_files $uri $uri/ /index.php?$query_string;
    }

    location ~ \.php$ {
        fastcgi_pass unix:/var/run/php/php8.2-fpm.sock;
        fastcgi_param SCRIPT_FILENAME $realpath_root$fastcgi_script_name;
        include fastcgi_params;
    }
}
```

## Testing

```bash
php artisan test
```

## Maintenance

### Clear old temperature data
```bash
php artisan thermostat:cleanup --days=90
```

### View device status
```bash
php artisan thermostat:status
```
