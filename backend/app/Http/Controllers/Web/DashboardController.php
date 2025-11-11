<?php

namespace App\Http\Controllers\Web;

use App\Http\Controllers\Controller;
use App\Models\Device;
use Illuminate\Http\Request;

class DashboardController extends Controller
{
    /**
     * Display dashboard with all devices
     */
    public function index()
    {
        $devices = Device::with(['settings', 'relays.currentState'])
            ->withCount('temperatureReadings')
            ->get();

        return view('dashboard.index', compact('devices'));
    }

    /**
     * Display device detail page with charts and controls
     */
    public function show($deviceId)
    {
        $device = Device::with([
            'settings',
            'relays.currentState',
        ])->findOrFail($deviceId);

        // Get latest temperature readings for the chart (last 24 hours)
        $readings = $device->temperatureReadings()
            ->where('recorded_at', '>=', now()->subDay())
            ->orderBy('recorded_at', 'asc')
            ->get();

        return view('dashboard.show', compact('device', 'readings'));
    }
}
