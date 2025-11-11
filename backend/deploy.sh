#!/bin/bash

# Laravel Deployment Script for Hostinger
# This script should be run on the production server

set -e  # Exit on error

echo "🚀 Starting deployment..."

# Navigate to project directory (lotr-control, not the symlink)
cd /home/u613295236/domains/lindstromsontheroad.com/public_html/lotr-control || exit 1

# Enable maintenance mode
echo "📦 Enabling maintenance mode..."
php artisan down || true

# Pull latest changes from git
echo "📥 Pulling latest code..."
git fetch origin
git reset --hard origin/main

# Install/update composer dependencies (no dev dependencies in production)
echo "📦 Installing composer dependencies..."
composer install --no-dev --optimize-autoloader --no-interaction

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

# Disable maintenance mode
echo "✅ Disabling maintenance mode..."
php artisan up

echo "🎉 Deployment completed successfully!"
