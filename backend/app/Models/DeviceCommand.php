<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Model;
use Illuminate\Database\Eloquent\Relations\BelongsTo;

class DeviceCommand extends Model
{
    const UPDATED_AT = null;

    protected $fillable = [
        'device_id',
        'type',
        'params',
        'status',
        'result',
        'acknowledged_at',
        'completed_at',
    ];

    protected $casts = [
        'params' => 'array',
        'result' => 'array',
        'acknowledged_at' => 'datetime',
        'completed_at' => 'datetime',
    ];

    public function device(): BelongsTo
    {
        return $this->belongsTo(Device::class);
    }

    public function scopePending($query)
    {
        return $query->where('status', 'pending');
    }

    public function scopeForDevice($query, $deviceId)
    {
        return $query->where('device_id', $deviceId);
    }
}
