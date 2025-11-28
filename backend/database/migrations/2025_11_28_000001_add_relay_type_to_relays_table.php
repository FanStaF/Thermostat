<?php

use Illuminate\Database\Migrations\Migration;
use Illuminate\Database\Schema\Blueprint;
use Illuminate\Support\Facades\Schema;

return new class extends Migration
{
    public function up(): void
    {
        Schema::table('relays', function (Blueprint $table) {
            $table->enum('relay_type', ['HEATING', 'COOLING', 'GENERIC', 'MANUAL_ONLY'])
                  ->default('HEATING')
                  ->after('name');
        });
    }

    public function down(): void
    {
        Schema::table('relays', function (Blueprint $table) {
            $table->dropColumn('relay_type');
        });
    }
};
