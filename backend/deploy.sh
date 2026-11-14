#!/bin/bash

# Laravel Deployment Script for Hostinger
# This script should be run on the production server
#
# Hostinger's PHP has proc_open disabled, so nothing here may make PHP spawn
# a subprocess: Composer's scripts (post-autoload-dump -> package:discover)
# run through Symfony Process and fail with "relies on proc_open". Composer
# therefore runs with --no-scripts, and only when composer.lock changed; the
# artisan steps it would have triggered are run directly instead.

set -e  # Exit on error

APP_DIR=/home/u613295236/public_html/Thermostat/backend
LOCK_STAMP=storage/app/.deployed-composer-lock

echo "🚀 Starting deployment..."

cd "$APP_DIR" || exit 1

# Whatever happens below, never leave the site stuck in maintenance mode.
cleanup() {
    status=$?
    if [ $status -ne 0 ]; then
        echo "❌ Deployment failed (exit $status) - bringing the site back up"
    fi
    php artisan up >/dev/null 2>&1 || true
}
trap cleanup EXIT

# Enable maintenance mode
echo "📦 Enabling maintenance mode..."
php artisan down || true

# Pull latest changes from git
echo "📥 Pulling latest code..."
git fetch origin
git reset --hard origin/main

# Install composer dependencies only when composer.lock changed (or vendor/ is
# missing). --no-scripts avoids the proc_open-based Composer scripts.
LOCK_HASH=$(sha1sum composer.lock | cut -d' ' -f1)
if [ ! -d vendor ] || [ ! -f "$LOCK_STAMP" ] || [ "$(cat "$LOCK_STAMP")" != "$LOCK_HASH" ]; then
    echo "📦 composer.lock changed - installing dependencies..."
    composer install --no-dev --optimize-autoloader --no-interaction --no-scripts
    echo "$LOCK_HASH" > "$LOCK_STAMP"
else
    echo "📦 Dependencies unchanged - skipping composer install"
fi

# What Composer's post-autoload-dump script would have done, run in-process.
php artisan package:discover --ansi

# Clear and rebuild cache
echo "🧹 Clearing caches..."
php artisan cache:clear
php artisan config:clear
php artisan route:clear
php artisan view:clear

# Run database migrations (with --force for production)
echo "🗄️  Running database migrations..."
php artisan migrate --force

# Optimize for production
echo "⚡ Optimizing application..."
php artisan config:cache
php artisan route:cache
php artisan view:cache

# Set proper permissions
echo "🔒 Setting permissions..."
chmod -R 755 storage bootstrap/cache
chmod -R 775 storage/logs

echo "✅ Disabling maintenance mode..."
# (the EXIT trap runs `php artisan up`)

echo "🎉 Deployment completed successfully!"
