# Deployment Guide - Hostinger Production

This guide covers deploying the Thermostat Monitor application to Hostinger (lotr.lindstromontheroad.com).

## Prerequisites

- Hostinger hosting account with SSH access
- GitHub repository set up
- Domain configured: lotr.lindstromontheroad.com

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
ssh u491870959@lotr.lindstromontheroad.com -p 65002
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
cd /home/u491870959/domains/lotr.lindstromontheroad.com/public_html
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
| `SSH_HOST` | Your server hostname | `srv123.hostinger.com` or `lotr.lindstromontheroad.com` |
| `SSH_USERNAME` | Your SSH username | `u491870959` |
| `SSH_PASSWORD` | Your SSH password | `your_ssh_password` |
| `SSH_PORT` | SSH port (if not 22) | `65002` |

### 3.3 Make deploy script executable

On server via SSH:
```bash
cd /home/u491870959/domains/lotr.lindstromontheroad.com/public_html
chmod +x deploy.sh
```

## Step 4: Test Deployment

### 4.1 Manual deployment test

On your local machine:
```bash
git checkout main
git push origin main
```

This will trigger GitHub Actions automatically.

### 4.2 Monitor deployment

1. Go to GitHub repository → **Actions** tab
2. Watch the deployment progress
3. Check for any errors

### 4.3 Verify deployment

Visit: https://lotr.lindstromontheroad.com

You should see the login page.

## Step 5: Configure ESP8266 for Production

### 5.1 Update firmware credentials

Edit `firmware/Thermostat/Credentials.h`:

```cpp
#define API_URL "https://lotr.lindstromontheroad.com"
#define API_KEY "your_production_api_key_from_env"
```

### 5.2 Compile and upload firmware

```bash
cd firmware/Thermostat
arduino-cli compile --fqbn esp8266:esp8266:nodemcuv2 .
arduino-cli upload -p /dev/ttyUSB0 --fqbn esp8266:esp8266:nodemcuv2 .
```

## Troubleshooting

### Permission errors

```bash
cd /home/u491870959/domains/lotr.lindstromontheroad.com/public_html
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
git config --global --add safe.directory /home/u491870959/domains/lotr.lindstromontheroad.com/public_html
```

### Composer errors

Update composer:
```bash
composer self-update
composer install --no-dev --optimize-autoloader
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
cd /home/u491870959/domains/lotr.lindstromontheroad.com/public_html
bash deploy.sh
```

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
- [ ] HTTPS enabled (SSL certificate)
- [ ] storage/ and bootstrap/cache/ writable
- [ ] Admin user created with strong password
- [ ] Regular backups configured

## Post-Deployment

1. Test login at https://lotr.lindstromontheroad.com/login
2. Register ESP8266 device
3. Verify temperature readings
4. Test relay controls
5. Monitor logs for errors

## Future Deployments

After initial setup, deployments are automatic:

1. Make changes locally
2. Commit and push to main branch
3. GitHub Actions deploys automatically
4. Check deployment status in GitHub Actions tab

## Support Resources

- Laravel Docs: https://laravel.com/docs
- Hostinger Support: https://www.hostinger.com/tutorials
- GitHub Actions: https://docs.github.com/actions
