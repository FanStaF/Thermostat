<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Model;
use Illuminate\Database\Eloquent\Relations\BelongsTo;

class TemperatureReading extends Model
{
    const UPDATED_AT = null;

    protected $fillable = [
        'device_id',
        'temperature',
        'sensor_id',
        'recorded_at',
    ];

    protected $casts = [
        'temperature' => 'decimal:2',
        'sensor_id' => 'integer',
        'recorded_at' => 'datetime',
    ];

    public function device(): BelongsTo
    {
        return $this->belongsTo(Device::class);
    }
}
