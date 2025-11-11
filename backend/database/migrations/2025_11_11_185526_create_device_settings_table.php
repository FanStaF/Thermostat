<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    public function up(): void
    {
        Schema::create('device_settings', function (Blueprint $table) {
            $table->id();
            $table->foreignId('device_id')->unique()->constrained()->onDelete('cascade');
            $table->integer('update_frequency')->default(5);
            $table->boolean('use_fahrenheit')->default(false);
            $table->string('timezone')->default('UTC');
            $table->json('settings_json')->nullable();
            $table->timestamps();
        });
    }

    public function down(): void
    {
        Schema::dropIfExists('device_settings');
    }
};
