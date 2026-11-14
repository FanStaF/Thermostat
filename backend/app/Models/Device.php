<?php

namespace App\Models;

use Illuminate\Foundation\Auth\User as Authenticatable;
use Illuminate\Database\Eloquent\Relations\HasMany;
use Illuminate\Database\Eloquent\Relations\HasOne;
use Illuminate\Database\Eloquent\Relations\BelongsToMany;
use Laravel\Sanctum\HasApiTokens;

class Device extends Authenticatable
{
    use HasApiTokens;
    protected $fillable = [
        'name',
        'hostname',
        'ip_address',
        'mac_address',
        'firmware_version',
        'last_seen_at',
        'sensor_fault',
        'last_good_reading_at',
    ];

    protected $casts = [
        'last_seen_at' => 'datetime',
        'sensor_fault' => 'boolean',
        'last_good_reading_at' => 'datetime',
    ];

    /**
     * Computed online flag — true if we've heard from the device in the last
     * 5 minutes. Replaces the dropped `is_online` column (which was set to
     * true on heartbeat but never flipped back).
     */
    public function getIsOnlineAttribute(): bool
    {
        return $this->last_seen_at !== null
            && $this->last_seen_at->isAfter(now()->subMinutes(5));
    }

    /**
     * A reported sensor fault only counts while the device is online: the flag
     * is cleared by the next heartbeat, so an offline device's last report is
     * stale and must not keep the fault banner/alert alive.
     */
    public function getHasActiveSensorFaultAttribute(): bool
    {
        return $this->sensor_fault && $this->is_online;
    }

    /**
     * Human-readable sensor fault description for the dashboard.
     */
    public function getSensorFaultMessageAttribute(): ?string
    {
        if (!$this->has_active_sensor_fault) {
            return null;
        }
        $since = $this->last_good_reading_at
            ? 'Last good reading ' . $this->last_good_reading_at->diffForHumans() . '.'
            : 'No good reading since the device started.';

        return 'SENSOR FAULT: the device has no valid temperature reading, so AUTO relays are OFF. '
            . $since . ' The temperature shown is stale.';
    }

    public function temperatureReadings(): HasMany
    {
        return $this->hasMany(TemperatureReading::class);
    }

    public function relays(): HasMany
    {
        return $this->hasMany(Relay::class);
    }

    public function settings(): HasOne
    {
        return $this->hasOne(DeviceSetting::class);
    }

    public function commands(): HasMany
    {
        return $this->hasMany(DeviceCommand::class);
    }

    public function users(): BelongsToMany
    {
        return $this->belongsToMany(User::class, 'device_user')
            ->withPivot('role')
            ->withTimestamps();
    }

    public function latestTemperatureReading(): HasMany
    {
        return $this->temperatureReadings()->latest('recorded_at')->limit(1);
    }
}
