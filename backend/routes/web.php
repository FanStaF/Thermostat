<?php

use App\Http\Controllers\Web\AlertsController;
use App\Http\Controllers\Web\AuthController;
use App\Http\Controllers\Web\DashboardController;
use App\Http\Controllers\Web\ProfileController;
use App\Http\Controllers\Web\UsersController;
use App\Http\Controllers\Api\CommandController;
use App\Http\Controllers\Api\DeviceController;
use App\Http\Controllers\Api\RelayController;
use Illuminate\Support\Facades\Route;

// Authentication Routes
Route::get('/login', [AuthController::class, 'showLogin'])->name('login')->middleware('guest');
Route::post('/login', [AuthController::class, 'login'])->middleware('guest');
Route::post('/logout', [AuthController::class, 'logout'])->name('logout')->middleware('auth');

// Protected Dashboard Routes
Route::middleware('auth')->group(function () {
    Route::get('/', [DashboardController::class, 'index'])->name('dashboard.index');
    Route::get('/devices/{device}', [DashboardController::class, 'show'])->name('dashboard.show');

    // Profile Routes
    Route::get('/profile', [ProfileController::class, 'edit'])->name('profile.edit');
    Route::patch('/profile', [ProfileController::class, 'update'])->name('profile.update');
    Route::patch('/profile/password', [ProfileController::class, 'updatePassword'])->name('profile.password.update');

    // Alerts Routes
    Route::get('/alerts', [AlertsController::class, 'index'])->name('alerts.index');

    // Web API endpoints (use session auth instead of Sanctum)
    Route::post('/devices/{device}/commands', [CommandController::class, 'store']);
    Route::patch('/devices/{device}', [DeviceController::class, 'update']);
    Route::patch('/devices/{device}/settings', [DeviceController::class, 'updateSettings']);
    Route::patch('/devices/{device}/relays/{relay}', [RelayController::class, 'update']);
});

// Admin Only Routes
Route::middleware(['auth', 'admin'])->group(function () {
    Route::resource('users', UsersController::class);
});
