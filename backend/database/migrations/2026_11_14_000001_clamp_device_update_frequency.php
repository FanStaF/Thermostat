<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Support\Facades\DB;

return new class extends Migration
{
    /**
     * update_frequency is now limited to 5-300s (firmware 2.2.0 rejects
     * anything else). Bring existing rows into range so the settings form,
     * which validates the prefilled value, can still be saved.
     */
    public function up(): void
    {
        DB::table('device_settings')->where('update_frequency', '<', 5)->update(['update_frequency' => 5]);
        DB::table('device_settings')->where('update_frequency', '>', 300)->update(['update_frequency' => 300]);
    }

    public function down(): void
    {
        // Not reversible: the original out-of-range values are not kept.
    }
};
