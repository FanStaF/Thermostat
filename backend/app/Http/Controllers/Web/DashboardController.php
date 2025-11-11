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
        $devices = Device::with([
            'settings',
            'relays.currentState',
            'temperatureReadings' => function($query) {
                $query->latest('recorded_at')->limit(1);
            }
        ])
            ->withCount('temperatureReadings')
            ->get();

        // Add latest temperature to each device
        $devices->each(function($device) {
            $device->latest_temp = $device->temperatureReadings->first();

            // Get temperature trend (last 12 readings for sparkline)
            $device->temp_trend = $device->temperatureReadings()
                ->latest('recorded_at')
                ->limit(12)
                ->get()
                ->reverse()
                ->pluck('temperature');
        });

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
