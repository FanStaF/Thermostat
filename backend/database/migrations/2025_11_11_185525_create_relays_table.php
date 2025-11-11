<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    public function up(): void
    {
        Schema::create('relays', function (Blueprint $table) {
            $table->id();
            $table->foreignId('device_id')->constrained()->onDelete('cascade');
            $table->tinyInteger('relay_number');
            $table->string('name')->nullable();
            $table->timestamps();

            $table->unique(['device_id', 'relay_number']);
        });
    }

    public function down(): void
    {
        Schema::dropIfExists('relays');
    }
};
