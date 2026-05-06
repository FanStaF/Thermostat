<?php

use App\Http\Controllers\Api\AlertSubscriptionController;
use App\Http\Controllers\Api\CommandController;
use App\Http\Controllers\Api\DeviceController;
use App\Http\Controllers\Api\RelayController;
use App\Http\Controllers\Api\TemperatureController;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Route;

Route::get('/user', function (Request $request) {
    return $request->user();
})->middleware('auth:sanctum');

// Device Registration (requires API key, rate limited to 10 per minute)
Route::post('/devices/register', [DeviceController::class, 'register'])
    ->middleware(['api.key', 'throttle:10,1']);

// Device-token API: every endpoint here is called by the firmware itself, so
// we additionally require the token's owner device to match the {device} bind.
Route::middleware(['auth:sanctum', 'throttle:120,1', 'device.scope'])->group(function () {
    Route::post('/devices/{device}/heartbeat', [DeviceController::class, 'heartbeat']);
    Route::get('/devices/{device}', [DeviceController::class, 'show']);
    Route::get('/devices/{device}/dashboard-data', [DeviceController::class, 'dashboardData']);

    // Temperature Readings
    Route::post('/devices/{device}/temperature', [TemperatureController::class, 'store']);
    Route::get('/devices/{device}/temperature', [TemperatureController::class, 'index']);
    Route::get('/devices/{device}/temperature/stats', [TemperatureController::class, 'stats']);

    // Relay Management
    Route::post('/devices/{device}/relay-state', [RelayController::class, 'updateState']);
    Route::get('/devices/{device}/relays', [RelayController::class, 'index']);
    Route::get('/devices/{device}/relays/{relay}/history', [RelayController::class, 'history']);

    // Device Commands (device polls / acknowledges)
    Route::get('/devices/{device}/commands/pending', [CommandController::class, 'pending']);
    Route::put('/devices/{device}/commands/{command}', [CommandController::class, 'update']);
    Route::get('/devices/{device}/commands', [CommandController::class, 'index']);
});

// User-facing API (Sanctum + session for the SPA, no per-device scope check —
// authorization is handled in-controller via canAccessDevice).
Route::middleware(['auth:sanctum', 'throttle:120,1'])->group(function () {
    Route::get('/devices', [DeviceController::class, 'index']);
    Route::patch('/devices/{device}', [DeviceController::class, 'update']);

    Route::patch('/devices/{device}/relays/{relay}', [RelayController::class, 'update']);

    Route::post('/devices/{device}/commands', [CommandController::class, 'store']);

    // Alert Subscriptions
    Route::get('/alert-types', [AlertSubscriptionController::class, 'types']);
    Route::get('/alert-subscriptions', [AlertSubscriptionController::class, 'index']);
    Route::post('/alert-subscriptions', [AlertSubscriptionController::class, 'store']);
    Route::patch('/alert-subscriptions/{id}', [AlertSubscriptionController::class, 'update']);
    Route::delete('/alert-subscriptions/{id}', [AlertSubscriptionController::class, 'destroy']);
    Route::get('/alert-logs', [AlertSubscriptionController::class, 'logs']);
});
