<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    public function up(): void
    {
        Schema::create('temperature_readings_hourly', function (Blueprint $table) {
            $table->id();
            $table->foreignId('device_id')->constrained()->onDelete('cascade');
            $table->tinyInteger('sensor_id')->default(0);
            $table->timestamp('bucket_start'); // truncated to the hour
            $table->decimal('avg_temp', 5, 2);
            $table->decimal('min_temp', 5, 2);
            $table->decimal('max_temp', 5, 2);
            $table->unsignedInteger('sample_count');

            $table->unique(['device_id', 'sensor_id', 'bucket_start'], 'temp_hourly_unique');
            $table->index(['device_id', 'bucket_start']);
        });
    }

    public function down(): void
    {
        Schema::dropIfExists('temperature_readings_hourly');
    }
};
