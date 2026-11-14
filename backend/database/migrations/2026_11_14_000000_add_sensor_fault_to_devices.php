<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    public function up(): void
    {
        Schema::table('devices', function (Blueprint $table) {
            // Reported by firmware >= 2.2.0 in every heartbeat.
            $table->boolean('sensor_fault')->default(false)->after('last_seen_at');
            $table->timestamp('last_good_reading_at')->nullable()->after('sensor_fault');
        });
    }

    public function down(): void
    {
        Schema::table('devices', function (Blueprint $table) {
            $table->dropColumn(['sensor_fault', 'last_good_reading_at']);
        });
    }
};
