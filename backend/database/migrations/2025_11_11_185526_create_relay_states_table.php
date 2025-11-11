<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    public function up(): void
    {
        Schema::create('relay_states', function (Blueprint $table) {
            $table->id();
            $table->foreignId('relay_id')->constrained()->onDelete('cascade');
            $table->boolean('state');
            $table->enum('mode', ['AUTO', 'MANUAL_ON', 'MANUAL_OFF']);
            $table->decimal('temp_on', 5, 2);
            $table->decimal('temp_off', 5, 2);
            $table->timestamp('changed_at')->useCurrent();
            $table->timestamp('created_at')->useCurrent();

            $table->index(['relay_id', 'changed_at']);
        });
    }

    public function down(): void
    {
        Schema::dropIfExists('relay_states');
    }
};
