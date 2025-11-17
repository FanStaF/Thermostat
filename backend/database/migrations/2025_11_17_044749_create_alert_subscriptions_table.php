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
        Schema::create('alert_subscriptions', function (Blueprint $table) {
            $table->id();
            $table->foreignId('user_id')->constrained()->onDelete('cascade');
            $table->foreignId('device_id')->nullable()->constrained()->onDelete('cascade');
            $table->string('alert_type'); // temp_high, temp_low, device_offline, etc.
            $table->boolean('enabled')->default(true);
            $table->json('settings')->nullable(); // custom thresholds, frequency, etc.
            $table->timestamps();

            // Prevent duplicate subscriptions
            $table->unique(['user_id', 'device_id', 'alert_type']);
        });
    }

    /**
     * Reverse the migrations.
     */
    public function down(): void
    {
        Schema::dropIfExists('alert_subscriptions');
    }
};
