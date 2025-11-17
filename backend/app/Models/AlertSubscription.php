<?php

namespace App\Models;

use App\AlertType;
use Illuminate\Database\Eloquent\Model;
use Illuminate\Database\Eloquent\Relations\BelongsTo;
use Illuminate\Database\Eloquent\Relations\HasMany;

class AlertSubscription extends Model
{
    protected $fillable = [
        'user_id',
        'device_id',
        'alert_type',
        'enabled',
        'settings',
    ];

    protected $casts = [
        'enabled' => 'boolean',
        'settings' => 'array',
        'alert_type' => AlertType::class,
    ];

    public function user(): BelongsTo
    {
        return $this->belongsTo(User::class);
    }

    public function device(): BelongsTo
    {
        return $this->belongsTo(Device::class);
    }

    public function logs(): HasMany
    {
        return $this->hasMany(AlertLog::class);
    }

    public function scopeEnabled($query)
    {
        return $query->where('enabled', true);
    }

    public function scopeForDevice($query, int $deviceId)
    {
        return $query->where(function ($q) use ($deviceId) {
            $q->where('device_id', $deviceId)
              ->orWhereNull('device_id'); // Include global subscriptions
        });
    }
}
