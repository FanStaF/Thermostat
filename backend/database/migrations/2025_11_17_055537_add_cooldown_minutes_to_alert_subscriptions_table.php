<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    /**
     * Run the migrations.
     */
    public function up(): void
    {
        Schema::table('alert_subscriptions', function (Blueprint $table) {
            $table->integer('cooldown_minutes')->default(30)->after('enabled');
            $table->time('scheduled_time')->nullable()->after('cooldown_minutes');
        });
    }

    /**
     * Reverse the migrations.
     */
    public function down(): void
    {
        Schema::table('alert_subscriptions', function (Blueprint $table) {
            $table->dropColumn(['cooldown_minutes', 'scheduled_time']);
        });
    }
};
