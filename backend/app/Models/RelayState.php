<?php

namespace App\Models;

use Illuminate\Database\Eloquent\Model;
use Illuminate\Database\Eloquent\Relations\BelongsTo;

class RelayState extends Model
{
    const UPDATED_AT = null;

    protected $fillable = [
        'relay_id',
        'state',
        'mode',
        'temp_on',
        'temp_off',
        'changed_at',
    ];

    protected $casts = [
        'state' => 'boolean',
        'temp_on' => 'decimal:2',
        'temp_off' => 'decimal:2',
        'changed_at' => 'datetime',
    ];

    public function relay(): BelongsTo
    {
        return $this->belongsTo(Relay::class);
    }
}
