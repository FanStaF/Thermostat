<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    public function up(): void
    {
        Schema::table('devices', function (Blueprint $table) {
            // Latest health snapshot from the heartbeat (firmware >= 2.6.0):
            // boot_count, reset_reason, uptime_s, wifi_ssid, rssi, free_heap,
            // max_free_block, heap_frag, outbox_bytes.
            $table->json('health')->nullable()->after('reregistration_allowed');
        });

        // One row per device restart, so repeated resets (e.g. a failing
        // power connection) are visible with their reasons.
        Schema::create('device_boots', function (Blueprint $table) {
            $table->id();
            $table->foreignId('device_id')->constrained()->cascadeOnDelete();
            $table->unsignedInteger('boot_count');
            $table->string('reset_reason', 100)->nullable();
            $table->timestamp('booted_at');
            $table->timestamps();
            $table->index(['device_id', 'booted_at']);
        });
    }

    public function down(): void
    {
        Schema::dropIfExists('device_boots');
        Schema::table('devices', function (Blueprint $table) {
            $table->dropColumn('health');
        });
    }
};
