<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Model;
use Illuminate\Database\Eloquent\Relations\BelongsTo;
use Illuminate\Database\Eloquent\Relations\HasMany;

class Relay extends Model
{
    protected $fillable = [
        'device_id',
        'relay_number',
        'name',
        'relay_type',
    ];

    protected $casts = [
        'relay_number' => 'integer',
    ];

    public function device(): BelongsTo
    {
        return $this->belongsTo(Device::class);
    }

    public function states(): HasMany
    {
        return $this->hasMany(RelayState::class);
    }

    public function currentState(): HasMany
    {
        return $this->states()->latest('changed_at')->limit(1);
    }
}
