<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;
use Illuminate\Support\Facades\DB;

return new class extends Migration
{
    public function up(): void
    {
        // Modify enum to include set_relay_type
        DB::statement("ALTER TABLE device_commands MODIFY COLUMN type ENUM('set_relay_mode', 'set_relay_type', 'set_thresholds', 'set_frequency', 'set_unit', 'restart')");
    }

    public function down(): void
    {
        // Remove set_relay_type from enum (will fail if any rows have this value)
        DB::statement("ALTER TABLE device_commands MODIFY COLUMN type ENUM('set_relay_mode', 'set_thresholds', 'set_frequency', 'set_unit', 'restart')");
    }
};
