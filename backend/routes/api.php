<?php

use App\Http\Controllers\Api\CommandController;
use App\Http\Controllers\Api\DeviceController;
use App\Http\Controllers\Api\RelayController;
use App\Http\Controllers\Api\TemperatureController;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Route;

Route::get('/user', function (Request $request) {
    return $request->user();
})->middleware('auth:sanctum');

// Device Management
Route::post('/devices/register', [DeviceController::class, 'register']);
Route::post('/devices/{device}/heartbeat', [DeviceController::class, 'heartbeat']);
Route::get('/devices', [DeviceController::class, 'index']);
Route::get('/devices/{device}', [DeviceController::class, 'show']);
Route::patch('/devices/{device}', [DeviceController::class, 'update']);

// Temperature Readings
Route::post('/devices/{device}/temperature', [TemperatureController::class, 'store']);
Route::get('/devices/{device}/temperature', [TemperatureController::class, 'index']);
Route::get('/devices/{device}/temperature/stats', [TemperatureController::class, 'stats']);

// Relay Management
Route::post('/devices/{device}/relay-state', [RelayController::class, 'updateState']);
Route::get('/devices/{device}/relays', [RelayController::class, 'index']);
Route::get('/devices/{device}/relays/{relay}/history', [RelayController::class, 'history']);
Route::patch('/devices/{device}/relays/{relay}', [RelayController::class, 'update']);

// Device Commands
Route::get('/devices/{device}/commands/pending', [CommandController::class, 'pending']);
Route::post('/devices/{device}/commands', [CommandController::class, 'store']);
Route::put('/devices/{device}/commands/{command}', [CommandController::class, 'update']);
Route::get('/devices/{device}/commands', [CommandController::class, 'index']);
