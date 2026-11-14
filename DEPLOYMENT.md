# Deployment Guide - Hostinger Production

This guide covers deploying the Thermostat Monitor application to Hostinger (lotr.lindstromsontheroad.com).

> **Current production layout:** the repository is checked out at
> `/home/u613295236/public_html/Thermostat`, and the Laravel app (and `deploy.sh`) lives in its
> `backend/` folder. Paths in the initial-setup steps below are from the original setup; use
> `backend/deploy.sh` as the source of truth for the live path.
>
> **Hostinger's PHP has `proc_open` disabled.** Anything that makes PHP spawn a subprocess
> fails with "The Process class relies on proc_open". In particular, Composer's own scripts
> (`post-autoload-dump` → `package:discover`) fail, so `deploy.sh` runs Composer with
> `--no-scripts` and calls `php artisan package:discover` itself.

## Prerequisites

- Hostinger hosting account with SSH access
- GitHub repository set up
- Domain configured: lotr.lindstromsontheroad.com

## Step 1: Prepare Hostinger Environment

### 1.1 Access Hostinger via SSH

From Hostinger cPanel:
1. Go to **Advanced → SSH Access**
2. Enable SSH access
3. Note your SSH credentials:
   - Host: (usually your domain or server IP)
   - Username: u491870959
   - Port: (usually 22 or 65002 for Hostinger)

### 1.2 Connect via SSH

```bash
ssh u491870959@lotr.lindstromsontheroad.com -p 65002
```

### 1.3 Create Database

From Hostinger cPanel:
1. Go to **Databases → MySQL Databases**
2. Create new database: `u491870959_thermostat`
3. Create database user: `u491870959_admin`
4. Set strong password
5. Add user to database with ALL PRIVILEGES

## Step 2: Initial Server Setup

### 2.1 Navigate to web root

```bash
cd /home/u491870959/domains/lotr.lindstromsontheroad.com/public_html
```

### 2.2 Clone repository

```bash
# Remove default files
rm -rf *

# Clone your repository
git clone https://github.com/YOUR_USERNAME/Thermostat.git .

# Move Laravel files from backend to root
mv backend/* .
mv backend/.* . 2>/dev/null || true
rmdir backend
```

### 2.3 Install Composer dependencies

```bash
composer install --no-dev --optimize-autoloader
```

### 2.4 Set up environment file

```bash
# Copy production example
cp .env.production.example .env

# Edit with nano or vim
nano .env
```

Configure these values in `.env`:
```bash
APP_KEY=  # Will generate in next step
API_KEY=$(openssl rand -hex 32)  # Generate unique key
DB_DATABASE=u491870959_thermostat
DB_USERNAME=u491870959_admin
DB_PASSWORD=your_strong_password_here
```

### 2.5 Generate application key

```bash
php artisan key:generate
```

### 2.6 Run migrations

```bash
php artisan migrate --force
```

### 2.7 Create admin user

```bash
php artisan user:create
# Follow prompts to create admin account
```

### 2.8 Set permissions

```bash
chmod -R 755 storage bootstrap/cache
chmod -R 775 storage/logs
```

### 2.9 Configure web root

In Hostinger cPanel:
1. Go to **Advanced → PHP Configuration** or **Website → Manage**
2. Set document root to: `/public_html/public`
3. This ensures Laravel's public folder is the web root

## Step 3: Configure GitHub Actions

### 3.1 Get SSH credentials from Hostinger

From Hostinger cPanel → SSH Access, note:
- SSH Host
- SSH Username
- SSH Password
- SSH Port

### 3.2 Add GitHub Secrets

Go to your GitHub repository:
1. Click **Settings → Secrets and variables → Actions**
2. Click **New repository secret**
3. Add these secrets:

| Secret Name | Value | Example |
|------------|-------|---------|
| `SSH_HOST` | Your server hostname | `srv123.hostinger.com` or `lotr.lindstromsontheroad.com` |
| `SSH_USERNAME` | Your SSH username | `u491870959` |
| `SSH_PASSWORD` | Your SSH password | `your_ssh_password` |
| `SSH_PORT` | SSH port (if not 22) | `65002` |

### 3.3 Make deploy script executable

On server via SSH:
```bash
cd /home/u491870959/domains/lotr.lindstromsontheroad.com/public_html
chmod +x deploy.sh
```

## Step 4: Test Deployment

### 4.1 Run a deployment

Deployment is **manual**. Pushing to `main` only runs CI (backend tests + firmware build);
it does not deploy. Either:

- GitHub → **Actions** → **Deploy to Hostinger** → **Run workflow**, or
- on the server: see "Manual deployment" under Maintenance.

### 4.2 Monitor deployment

1. Go to GitHub repository → **Actions** tab
2. Check that **CI** passed for the commit you are deploying
3. Watch the deployment progress and check for errors

### 4.3 Verify deployment

Visit: https://lotr.lindstromsontheroad.com

You should see the login page.

## Step 5: Configure ESP8266 for Production

### 5.1 Update firmware credentials

Edit `firmware/Thermostat/Credentials.h`:

```cpp
#define API_URL "http://lotr.lindstromsontheroad.com"   // ESP8266 can't do HTTPS here, see firmware/README.md
#define API_KEY "your_production_api_key_from_env"
#define OTA_PASSWORD "long-random-value"
```

### 5.2 Compile and upload firmware

```bash
cd firmware/Thermostat
./upload.sh usb /dev/ttyUSB0   # first flash
./upload.sh                    # later updates over WiFi
```

### 5.3 Device registration rules

A new device registers with the API key alone. An **existing** device (same MAC) must present
its current token to re-register; the firmware does this automatically. If a device lost its
stored token (for example after a full flash erase) and is refused, an admin clicks **Allow
re-registration** on its device page; this works once.

## Troubleshooting

### Permission errors

```bash
cd /home/u491870959/domains/lotr.lindstromsontheroad.com/public_html
chmod -R 755 storage bootstrap/cache
chmod -R 775 storage/logs
```

### 500 Internal Server Error

Check Laravel logs:
```bash
tail -f storage/logs/laravel.log
```

Common causes:
- Missing `.env` file
- Wrong database credentials
- Missing APP_KEY
- Wrong file permissions

### Database connection errors

Verify database credentials in cPanel and `.env` match exactly.

### Git permission errors

```bash
git config --global --add safe.directory /home/u491870959/domains/lotr.lindstromsontheroad.com/public_html
```

### Composer errors / "relies on proc_open"

`proc_open` is disabled on Hostinger, so Composer scripts can't run there. `deploy.sh` only
runs `composer install --no-dev --optimize-autoloader --no-scripts` when `composer.lock`
changed (it records the lock's hash in `storage/app/.deployed-composer-lock`). If Composer
itself fails when installing new packages, run `composer install --no-dev` locally and
upload the `vendor/` folder.

If `vendor/` already matches `composer.lock` (e.g. after a deploy that failed at the
Composer-scripts step), record that so the next deploy skips Composer:
```bash
sha1sum composer.lock | cut -d' ' -f1 > storage/app/.deployed-composer-lock
```

## Maintenance

### View logs

```bash
tail -f storage/logs/laravel.log
```

### Clear caches

```bash
php artisan cache:clear
php artisan config:clear
php artisan route:clear
php artisan view:clear
```

### Manual deployment

```bash
cd /home/u613295236/public_html/Thermostat/backend
git fetch origin && git reset --hard origin/main   # picks up the latest deploy.sh too
bash deploy.sh
```

`deploy.sh` puts the site in maintenance mode, pulls `origin/main`, installs Composer
packages only if `composer.lock` changed, runs migrations, rebuilds caches and fixes
permissions. If any step fails, it still brings the site back up. Afterwards `git status`
should be clean.

### Optional: require HTTPS for the device API

Set `API_REQUIRE_HTTPS=true` in `.env` to refuse plain-HTTP device calls. **Only** do this once
every device's `API_URL` uses `https://`; the current ESP8266 firmware can't, so leave it off.

### Backup database

From cPanel:
1. Go to **Databases → phpMyAdmin**
2. Select database
3. Click **Export**
4. Download backup

## Security Checklist

- [ ] APP_DEBUG=false in production .env
- [ ] Unique API_KEY generated
- [ ] Strong database password set
- [ ] HTTPS enabled for the website (SSL certificate)
- [ ] Each device has a unique OTA_PASSWORD in its Credentials.h
- [ ] storage/ and bootstrap/cache/ writable
- [ ] Admin user created with strong password
- [ ] Regular backups configured

## Post-Deployment

1. Test login at https://lotr.lindstromsontheroad.com/login
2. Register ESP8266 device
3. Verify temperature readings
4. Test relay controls
5. Monitor logs for errors

## Future Deployments

1. Make changes locally; run `php artisan test` in `backend/`
2. Commit and push to `main`. CI runs the tests and builds the firmware.
3. Once CI is green, deploy manually (Actions → Deploy to Hostinger, or `deploy.sh` on the server)
4. Firmware changes are flashed separately with `firmware/Thermostat/upload.sh`

## Support Resources

- Laravel Docs: https://laravel.com/docs
- Hostinger Support: https://www.hostinger.com/tutorials
- GitHub Actions: https://docs.github.com/actions
