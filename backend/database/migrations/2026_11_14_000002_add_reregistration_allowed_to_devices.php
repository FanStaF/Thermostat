<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    public function up(): void
    {
        Schema::table('devices', function (Blueprint $table) {
            // Set by an admin to let an existing device register again without
            // presenting its current token (e.g. after a full flash erase).
            // Cleared automatically by the next successful registration.
            $table->boolean('reregistration_allowed')->default(false)->after('last_good_reading_at');
        });
    }

    public function down(): void
    {
        Schema::table('devices', function (Blueprint $table) {
            $table->dropColumn('reregistration_allowed');
        });
    }
};
