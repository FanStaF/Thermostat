<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    public function up(): void
    {
        // recorded_at and created_at are always set together by useCurrent()
        // — created_at carries no extra information.
        Schema::table('temperature_readings', function (Blueprint $table) {
            $table->dropColumn('created_at');
        });
    }

    public function down(): void
    {
        Schema::table('temperature_readings', function (Blueprint $table) {
            $table->timestamp('created_at')->useCurrent()->after('recorded_at');
        });
    }
};
