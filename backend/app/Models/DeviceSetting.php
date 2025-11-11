<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Model;
use Illuminate\Database\Eloquent\Relations\BelongsTo;

class DeviceSetting extends Model
{
    protected $fillable = [
        'device_id',
        'update_frequency',
        'use_fahrenheit',
        'timezone',
        'settings_json',
    ];

    protected $casts = [
        'update_frequency' => 'integer',
        'use_fahrenheit' => 'boolean',
        'settings_json' => 'array',
    ];

    public function device(): BelongsTo
    {
        return $this->belongsTo(Device::class);
    }
}
