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
        'reregistration_allowed',
        'health',
    ];

    protected $casts = [
        'last_seen_at' => 'datetime',
        'sensor_fault' => 'boolean',
        'reregistration_allowed' => 'boolean',
        'health' => 'array',
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

    /**
     * Commands waiting for the device. Returned in device-facing responses so
     * the firmware fetches them right away instead of on a slow poll.
     */
    /**
     * Changes whenever there is something new to display (a new reading, a
     * relay state change, or a fault change). Dashboards poll this and skip
     * re-rendering when it hasn't changed. last_seen_at is deliberately not
     * part of it: the page counts "last seen" up locally between polls.
     */
    public function dataVersion(): string
    {
        $readingId = $this->temperatureReadings()->latest('recorded_at')->value('id') ?? 0;
        $relayStateId = RelayState::whereIn('relay_id', $this->relays()->pluck('id'))->max('id') ?? 0;

        return $readingId . '-' . $relayStateId . '-' . ($this->has_active_sensor_fault ? 1 : 0);
    }

    /**
     * Seconds between device readings, used as the dashboard poll interval.
     */
    public function updateFrequency(): int
    {
        return (int) ($this->settings?->update_frequency ?? 5);
    }

    public function pendingCommandCount(): int
    {
        return $this->commands()->where('status', 'pending')->count();
    }

    public function boots(): HasMany
    {
        return $this->hasMany(DeviceBoot::class);
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
