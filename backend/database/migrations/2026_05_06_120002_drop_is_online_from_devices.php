<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    public function up(): void
    {
        Schema::table('devices', function (Blueprint $table) {
            $table->dropIndex('devices_is_online_index');
            $table->dropColumn('is_online');
        });
    }

    public function down(): void
    {
        Schema::table('devices', function (Blueprint $table) {
            $table->boolean('is_online')->default(false)->after('last_seen_at');
            $table->index('is_online');
        });
    }
};
