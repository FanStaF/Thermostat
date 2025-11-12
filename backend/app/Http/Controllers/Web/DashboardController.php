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

            // Get temperature trend (last 8 hours, sampled to ~16 datapoints)
            $readings = $device->temperatureReadings()
                ->where('recorded_at', '>=', now()->subHours(8))
                ->orderBy('recorded_at', 'asc')
                ->get();

            // Sample to ~16 datapoints
            $totalReadings = $readings->count();
            if ($totalReadings > 16) {
                $step = (int) ceil($totalReadings / 16);
                $sampledReadings = $readings->filter(function($reading, $index) use ($step) {
                    return $index % $step === 0;
                });
                $device->temp_trend_data = $sampledReadings->values();
            } else {
                $device->temp_trend_data = $readings;
            }
        });

        return view('dashboard.index', compact('devices'));
    }

    /**
     * Display device detail page with charts and controls
     */
    public function show(Request $request, $deviceId)
    {
        $device = Device::with([
            'settings',
            'relays.currentState',
        ])->findOrFail($deviceId);

        // Get time range from request, default to 24h
        $range = $request->get('range', '24h');

        // Calculate the start time based on range
        $query = $device->temperatureReadings();

        switch ($range) {
            case '1h':
                $query->where('recorded_at', '>=', now()->subHour());
                break;
            case '6h':
                $query->where('recorded_at', '>=', now()->subHours(6));
                break;
            case '24h':
                $query->where('recorded_at', '>=', now()->subDay());
                break;
            case '7d':
                $query->where('recorded_at', '>=', now()->subDays(7));
                break;
            case '30d':
                $query->where('recorded_at', '>=', now()->subDays(30));
                break;
            case 'all':
                // No time filter - get all readings
                break;
        }

        $readings = $query->orderBy('recorded_at', 'asc')->get();

        return view('dashboard.show', compact('device', 'readings', 'range'));
    }
}
