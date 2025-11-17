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
        Schema::create('alert_logs', function (Blueprint $table) {
            $table->id();
            $table->foreignId('alert_subscription_id')->constrained()->onDelete('cascade');
            $table->foreignId('device_id')->constrained()->onDelete('cascade');
            $table->timestamp('triggered_at');
            $table->timestamp('resolved_at')->nullable();
            $table->json('data')->nullable(); // details about what triggered the alert
            $table->timestamps();

            // Index for efficient queries
            $table->index(['alert_subscription_id', 'triggered_at']);
            $table->index(['device_id', 'triggered_at']);
        });
    }

    /**
     * Reverse the migrations.
     */
    public function down(): void
    {
        Schema::dropIfExists('alert_logs');
    }
};
