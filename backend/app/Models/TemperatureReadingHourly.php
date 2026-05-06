<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Model;
use Illuminate\Database\Eloquent\Relations\BelongsTo;

class TemperatureReadingHourly extends Model
{
    protected $table = 'temperature_readings_hourly';

    public $timestamps = false;

    protected $fillable = [
        'device_id',
        'sensor_id',
        'bucket_start',
        'avg_temp',
        'min_temp',
        'max_temp',
        'sample_count',
    ];

    protected $casts = [
        'sensor_id'    => 'integer',
        'bucket_start' => 'datetime',
        'avg_temp'     => 'decimal:2',
        'min_temp'     => 'decimal:2',
        'max_temp'     => 'decimal:2',
        'sample_count' => 'integer',
    ];

    public function device(): BelongsTo
    {
        return $this->belongsTo(Device::class);
    }
}
