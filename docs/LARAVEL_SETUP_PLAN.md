# Laravel Setup Plan - Detailed Steps

## Prerequisites Check

Before we begin, verify you have:
- [ ] PHP 8.2 or higher
- [ ] Composer installed
- [ ] MySQL/MariaDB or PostgreSQL installed
- [ ] Node.js and npm (for frontend assets)
- [ ] Git (already have)

**Quick Check Commands:**
```bash
php -v           # Should show 8.2+
composer -V      # Should show Composer version
mysql --version  # Or: psql --version
node -v          # Should show Node 18+
npm -v           # Should show npm version
```

---

## Step 1: Initialize Laravel Project

### 1.1 Installation Options

**Option A: Laravel Installer (Recommended)**
```bash
cd /home/fanstaf/Arduino/Thermostat
composer create-project laravel/laravel backend
```

**Option B: Composer Create-Project (Alternative)**
```bash
composer create-project --prefer-dist laravel/laravel backend
```

### 1.2 What Gets Installed

Laravel will create the following structure in `backend/`:
```
backend/
├── app/
│   ├── Console/
│   ├── Exceptions/
│   ├── Http/
│   │   ├── Controllers/
│   │   ├── Middleware/
│   │   └── Requests/
│   ├── Models/
│   └── Providers/
├── bootstrap/
├── config/
├── database/
│   ├── factories/
│   ├── migrations/
│   └── seeders/
├── public/
├── resources/
│   ├── css/
│   ├── js/
│   └── views/
├── routes/
│   ├── api.php       # API routes (we'll use this)
│   ├── web.php       # Web routes (dashboard)
│   ├── console.php
│   └── channels.php
├── storage/
├── tests/
├── .env.example
├── artisan           # CLI tool
├── composer.json
└── package.json
```

### 1.3 Initial Configuration

After installation, we'll configure:

#### Environment File (.env)
```bash
cp backend/.env.example backend/.env
```

Key settings we'll configure:
```env
APP_NAME="Thermostat Monitor"
APP_ENV=local
APP_DEBUG=true
APP_URL=http://localhost:8000

DB_CONNECTION=mysql           # or postgresql
DB_HOST=127.0.0.1
DB_PORT=3306                 # or 5432 for PostgreSQL
DB_DATABASE=thermostat
DB_USERNAME=your_username
DB_PASSWORD=your_password

CACHE_DRIVER=redis           # We'll use Redis for command queue
QUEUE_CONNECTION=redis
SESSION_DRIVER=redis
```

#### Generate Application Key
```bash
cd backend
php artisan key:generate
```

#### Install NPM Dependencies
```bash
npm install
npm run build
```

### 1.4 Install Additional Packages

We'll need some extra Laravel packages:

```bash
cd backend
composer require laravel/sanctum    # API authentication
composer require predis/predis      # Redis client for PHP
composer require --dev laravel/pint # Code formatting
```

**Optional but Recommended:**
```bash
composer require barryvdh/laravel-debugbar --dev  # Debug toolbar
composer require laravel/telescope --dev           # Application insights
```

---

## Step 2: Database Setup & Migrations

### 2.1 Database Creation

**For MySQL/MariaDB:**
```bash
mysql -u root -p
```
```sql
CREATE DATABASE thermostat CHARACTER SET utf8mb4 COLLATE utf8mb4_unicode_ci;
CREATE USER 'thermostat_user'@'localhost' IDENTIFIED BY 'secure_password_here';
GRANT ALL PRIVILEGES ON thermostat.* TO 'thermostat_user'@'localhost';
FLUSH PRIVILEGES;
EXIT;
```

**For PostgreSQL:**
```bash
sudo -u postgres psql
```
```sql
CREATE DATABASE thermostat;
CREATE USER thermostat_user WITH PASSWORD 'secure_password_here';
GRANT ALL PRIVILEGES ON DATABASE thermostat TO thermostat_user;
\q
```

### 2.2 Migration Files to Create

We'll create migrations in this order (dependencies first):

#### Migration 1: `create_devices_table`
```bash
php artisan make:migration create_devices_table
```

**Schema:**
```php
Schema::create('devices', function (Blueprint $table) {
    $table->id();
    $table->string('name');
    $table->string('hostname')->unique();
    $table->string('ip_address')->nullable();
    $table->string('mac_address')->unique();
    $table->string('firmware_version')->nullable();
    $table->timestamp('last_seen_at')->nullable();
    $table->boolean('is_online')->default(false);
    $table->timestamps();

    $table->index('hostname');
    $table->index('is_online');
});
```

**Purpose:** Store information about each ESP8266 device

---

#### Migration 2: `create_temperature_readings_table`
```bash
php artisan make:migration create_temperature_readings_table
```

**Schema:**
```php
Schema::create('temperature_readings', function (Blueprint $table) {
    $table->id();
    $table->foreignId('device_id')->constrained()->onDelete('cascade');
    $table->decimal('temperature', 5, 2); // -999.99 to 999.99
    $table->tinyInteger('sensor_id')->default(0);
    $table->timestamp('recorded_at')->useCurrent();
    $table->timestamp('created_at')->useCurrent();

    // Composite index for efficient queries
    $table->index(['device_id', 'recorded_at']);
    $table->index('recorded_at'); // For time-based queries
});
```

**Purpose:** Store all temperature readings from sensors
**Data Volume:** High-frequency (every 60s) = ~1440 rows/device/day

---

#### Migration 3: `create_relays_table`
```bash
php artisan make:migration create_relays_table
```

**Schema:**
```php
Schema::create('relays', function (Blueprint $table) {
    $table->id();
    $table->foreignId('device_id')->constrained()->onDelete('cascade');
    $table->tinyInteger('relay_number'); // 1-4
    $table->string('name')->nullable(); // User-friendly name
    $table->timestamps();

    $table->unique(['device_id', 'relay_number']);
});
```

**Purpose:** Define the 4 relays for each device

---

#### Migration 4: `create_relay_states_table`
```bash
php artisan make:migration create_relay_states_table
```

**Schema:**
```php
Schema::create('relay_states', function (Blueprint $table) {
    $table->id();
    $table->foreignId('relay_id')->constrained()->onDelete('cascade');
    $table->boolean('state'); // ON/OFF
    $table->enum('mode', ['AUTO', 'MANUAL_ON', 'MANUAL_OFF']);
    $table->decimal('temp_on', 5, 2);
    $table->decimal('temp_off', 5, 2);
    $table->timestamp('changed_at')->useCurrent();
    $table->timestamp('created_at')->useCurrent();

    $table->index(['relay_id', 'changed_at']);
});
```

**Purpose:** Track relay state history
**Data Volume:** Medium (only on state changes)

---

#### Migration 5: `create_relay_events_table` (Optional - Detailed Logging)
```bash
php artisan make:migration create_relay_events_table
```

**Schema:**
```php
Schema::create('relay_events', function (Blueprint $table) {
    $table->id();
    $table->foreignId('relay_id')->constrained()->onDelete('cascade');
    $table->enum('event_type', ['state_change', 'mode_change', 'threshold_change']);
    $table->json('old_value')->nullable();
    $table->json('new_value');
    $table->enum('triggered_by', ['auto', 'user', 'api', 'schedule'])->default('auto');
    $table->timestamp('created_at')->useCurrent();

    $table->index(['relay_id', 'created_at']);
    $table->index('event_type');
});
```

**Purpose:** Detailed audit log of all relay changes
**Recommendation:** Skip this for now, add later if needed

---

#### Migration 6: `create_device_settings_table`
```bash
php artisan make:migration create_device_settings_table
```

**Schema:**
```php
Schema::create('device_settings', function (Blueprint $table) {
    $table->id();
    $table->foreignId('device_id')->unique()->constrained()->onDelete('cascade');
    $table->integer('update_frequency')->default(5); // seconds
    $table->boolean('use_fahrenheit')->default(false);
    $table->string('timezone')->default('UTC');
    $table->json('settings_json')->nullable(); // For future settings
    $table->timestamps();
});
```

**Purpose:** Store device-specific configuration

---

#### Migration 7: `create_device_commands_table`
```bash
php artisan make:migration create_device_commands_table
```

**Schema:**
```php
Schema::create('device_commands', function (Blueprint $table) {
    $table->id();
    $table->foreignId('device_id')->constrained()->onDelete('cascade');
    $table->enum('type', ['set_relay_mode', 'set_thresholds', 'set_frequency', 'set_unit', 'restart']);
    $table->json('params');
    $table->enum('status', ['pending', 'acknowledged', 'completed', 'failed'])->default('pending');
    $table->json('result')->nullable();
    $table->timestamp('created_at')->useCurrent();
    $table->timestamp('acknowledged_at')->nullable();
    $table->timestamp('completed_at')->nullable();

    $table->index(['device_id', 'status']);
    $table->index('status');
});
```

**Purpose:** Command queue for Laravel → ESP8266 communication

---

#### Migration 8: `create_users_table` (Already exists)

Laravel creates this by default, but we may want to customize:

```php
Schema::create('users', function (Blueprint $table) {
    $table->id();
    $table->string('name');
    $table->string('email')->unique();
    $table->timestamp('email_verified_at')->nullable();
    $table->string('password');
    $table->rememberToken();
    $table->timestamps();
});
```

---

#### Migration 9: `create_device_user_table` (Pivot for Many-to-Many)
```bash
php artisan make:migration create_device_user_table
```

**Schema:**
```php
Schema::create('device_user', function (Blueprint $table) {
    $table->foreignId('device_id')->constrained()->onDelete('cascade');
    $table->foreignId('user_id')->constrained()->onDelete('cascade');
    $table->enum('role', ['owner', 'admin', 'viewer'])->default('viewer');
    $table->timestamps();

    $table->primary(['device_id', 'user_id']);
});
```

**Purpose:** Allow multiple users to access devices with different permission levels

---

### 2.3 Model Creation

For each migration, we'll create corresponding Eloquent models:

```bash
php artisan make:model Device
php artisan make:model TemperatureReading
php artisan make:model Relay
php artisan make:model RelayState
php artisan make:model DeviceSetting
php artisan make:model DeviceCommand
# User model already exists
```

### 2.4 Model Relationships

We'll define relationships in each model:

**Device Model:**
```php
class Device extends Model {
    public function temperatureReadings() {
        return $this->hasMany(TemperatureReading::class);
    }

    public function relays() {
        return $this->hasMany(Relay::class);
    }

    public function settings() {
        return $this->hasOne(DeviceSetting::class);
    }

    public function commands() {
        return $this->hasMany(DeviceCommand::class);
    }

    public function users() {
        return $this->belongsToMany(User::class)
                    ->withPivot('role')
                    ->withTimestamps();
    }
}
```

Similar relationships for other models...

### 2.5 Seeders (Optional Development Data)

We'll create seeders for development:

```bash
php artisan make:seeder DeviceSeeder
php artisan make:seeder UserSeeder
```

This will create fake data for testing without a real ESP8266.

---

## Step 3: Running Migrations

### 3.1 Run All Migrations
```bash
cd backend
php artisan migrate
```

This will create all tables in the correct order.

### 3.2 Verify Migrations
```bash
php artisan migrate:status
```

Shows which migrations have run.

### 3.3 Rollback (If Needed)
```bash
php artisan migrate:rollback     # Rollback last batch
php artisan migrate:fresh        # Drop all tables and re-run
php artisan migrate:fresh --seed # Fresh + seeders
```

---

## Step 4: Post-Migration Setup

### 4.1 Configure Laravel Sanctum
```bash
php artisan vendor:publish --provider="Laravel\Sanctum\SanctumServiceProvider"
php artisan migrate  # Creates personal_access_tokens table
```

### 4.2 Set Up Redis (For Command Queue)
```bash
# Install Redis if not already installed
sudo apt install redis-server
sudo systemctl start redis
sudo systemctl enable redis
```

**Configure in .env:**
```env
REDIS_HOST=127.0.0.1
REDIS_PASSWORD=null
REDIS_PORT=6379
```

### 4.3 Test Database Connection
```bash
php artisan tinker
```
```php
DB::connection()->getPdo();  // Should connect without errors
exit
```

---

## Migration Execution Order Summary

The migrations will be created and run in this order:

1. `create_devices_table` - Base device table
2. `create_temperature_readings_table` - Depends on devices
3. `create_relays_table` - Depends on devices
4. `create_relay_states_table` - Depends on relays
5. `create_device_settings_table` - Depends on devices
6. `create_device_commands_table` - Depends on devices
7. `create_users_table` - Independent (Laravel default)
8. `create_device_user_table` - Depends on devices and users

**Optional:**
9. `create_relay_events_table` - Detailed logging (skip for MVP)

---

## Validation Checklist

After completing Steps 1 & 2, verify:

- [ ] Laravel installed in `backend/` directory
- [ ] `.env` file configured with database credentials
- [ ] Application key generated
- [ ] Database created
- [ ] All migrations run successfully (`php artisan migrate:status` shows all green)
- [ ] Models created for each table
- [ ] Sanctum installed and configured
- [ ] Redis installed and configured
- [ ] `php artisan serve` starts development server
- [ ] Can access http://localhost:8000 (shows Laravel welcome page)
- [ ] Can run `php artisan tinker` and query database

---

## Estimated Time

- **Laravel Installation:** 5-10 minutes
- **Database Creation:** 5 minutes
- **Writing Migrations:** 30-45 minutes (we'll do this together)
- **Creating Models:** 15 minutes
- **Testing & Verification:** 10 minutes

**Total:** ~1-1.5 hours

---

## Next Steps After This

Once Laravel and database are set up, we'll proceed to:

1. **API Endpoints** - Create controllers and routes
2. **Device Registration** - First API endpoint for ESP8266
3. **Data Ingestion** - Temperature and relay state endpoints
4. **Dashboard UI** - Web interface with charts
5. **ESP8266 Integration** - Firmware modifications

---

## Questions to Answer Before Starting

1. **Database Choice:** MySQL/MariaDB or PostgreSQL?
   - Recommendation: MySQL (more common for shared hosting)

2. **User Authentication:** Single user or multi-user?
   - Recommendation: Start with single user, easy to expand later

3. **Redis vs Database Queue:** Use Redis for commands?
   - Recommendation: Redis (better performance for real-time commands)

4. **Development vs Production:** Local development first?
   - Recommendation: Yes, deploy to production later

5. **Skip Optional Tables:** Relay events table?
   - Recommendation: Skip for now, add if needed

---

Let me know your preferences for these questions, and I'll proceed with the implementation!
