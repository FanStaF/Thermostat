<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Model;
use Illuminate\Database\Eloquent\Relations\BelongsTo;

class DeviceBoot extends Model
{
    protected $fillable = ['device_id', 'boot_count', 'reset_reason', 'booted_at'];

    protected $casts = [
        'booted_at' => 'datetime',
    ];

    public function device(): BelongsTo
    {
        return $this->belongsTo(Device::class);
    }

    /**
     * Plain-language meaning of the ESP8266 reset reason.
     */
    public function getReasonLabelAttribute(): string
    {
        $r = strtolower((string) $this->reset_reason);
        return match (true) {
            str_contains($r, 'power on') => 'Power on (power was lost or cycled)',
            str_contains($r, 'external') => 'Reset button / power glitch',
            str_contains($r, 'software') => 'Software restart (update or restart command)',
            str_contains($r, 'watchdog') => 'Watchdog (firmware hung or crashed)',
            str_contains($r, 'exception') => 'Crash (exception)',
            str_contains($r, 'deep-sleep') => 'Woke from deep sleep',
            default => $this->reset_reason ?: 'Unknown',
        };
    }
}
