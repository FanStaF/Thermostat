<?php

use App\Http\Controllers\Web\DashboardController;
use Illuminate\Support\Facades\Route;

Route::get('/', [DashboardController::class, 'index'])->name('dashboard.index');
Route::get('/devices/{device}', [DashboardController::class, 'show'])->name('dashboard.show');
