<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    public function up(): void
    {
        Schema::create('device_commands', function (Blueprint $table) {
            $table->id();
            $table->foreignId('device_id')->constrained()->onDelete('cascade');
            $table->enum('type', ['set_relay_mode', 'set_thresholds', 'set_frequency', 'set_unit', 'restart']);
            $table->json('params');
            $table->enum('status', ['pending', 'acknowledged', 'completed', 'failed'])->default('pending');
            $table->json('result')->nullable();
            $table->timestamp('created_at')->useCurrent();
            $table->timestamp('acknowledged_at')->nullable();
            $table->timestamp('completed_at')->nullable();

            $table->index(['device_id', 'status']);
            $table->index('status');
        });
    }

    public function down(): void
    {
        Schema::dropIfExists('device_commands');
    }
};
