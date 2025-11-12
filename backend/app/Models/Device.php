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
        'is_online',
    ];

    protected $casts = [
        'last_seen_at' => 'datetime',
        'is_online' => 'boolean',
    ];

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
