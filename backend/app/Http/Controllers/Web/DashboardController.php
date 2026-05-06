<?php

namespace App\Http\Controllers\Web;

use App\Http\Controllers\Controller;
use App\Models\Device;
use App\Models\TemperatureReadingHourly;
use Illuminate\Http\Request;
use Illuminate\Support\Collection;

class DashboardController extends Controller
{
    /**
     * Display dashboard with all devices
     */
    public function index()
    {
        $user = auth()->user();

        // Build query based on user permissions
        $query = Device::with([
            'settings',
            'relays.currentState',
            'temperatureReadings' => function($query) {
                $query->latest('recorded_at')->limit(1);
            }
        ])
            ->withCount('temperatureReadings');

        // Filter devices based on user role
        if (!$user->isAdmin()) {
            // Non-admin users only see their assigned devices
            $query->whereHas('users', function($q) use ($user) {
                $q->where('user_id', $user->id);
            });
        }

        $devices = $query->get();

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
    public function show(Request $request, Device $device)
    {
        $user = auth()->user();

        if (!$user->canAccessDevice($device->id)) {
            abort(403, 'You do not have permission to access this device.');
        }

        $device->load(['settings', 'relays.currentState']);

        $range = $request->get('range', '24h');
        $readings = $this->loadReadings($device, $range);

        return view('dashboard.show', compact('device', 'readings', 'range'));
    }

    /**
     * Pick the right data source per range:
     * - short ranges (≤7d) read raw temperature_readings
     * - long ranges (30d / all) read the hourly aggregate, plus any raw rows
     *   from the current month that haven't been downsampled yet.
     *
     * Returned collection is normalized so the chart blade can treat all rows
     * the same: each item has temperature + recorded_at.
     */
    private function loadReadings(Device $device, string $range): Collection
    {
        $rawCutoff = match ($range) {
            '1h'  => now()->subHour(),
            '6h'  => now()->subHours(6),
            '24h' => now()->subDay(),
            '7d'  => now()->subDays(7),
            default => null,
        };

        if ($rawCutoff !== null) {
            return $device->temperatureReadings()
                ->where('recorded_at', '>=', $rawCutoff)
                ->orderBy('recorded_at')
                ->get();
        }

        $longCutoff = $range === '30d' ? now()->subDays(30) : null;

        $hourlyQ = TemperatureReadingHourly::where('device_id', $device->id);
        if ($longCutoff) {
            $hourlyQ->where('bucket_start', '>=', $longCutoff);
        }
        $hourly = $hourlyQ->orderBy('bucket_start')->get()->map(fn ($h) => (object) [
            'temperature' => $h->avg_temp,
            'min_temp'    => $h->min_temp,
            'max_temp'    => $h->max_temp,
            'recorded_at' => $h->bucket_start,
            'sample_count'=> $h->sample_count,
        ]);

        // Include the recent raw rows that haven't been downsampled yet so the
        // chart doesn't have a gap between the latest hourly bucket and now.
        $rawTail = $device->temperatureReadings()
            ->when($longCutoff, fn ($q) => $q->where('recorded_at', '>=', $longCutoff))
            ->orderBy('recorded_at')
            ->get();

        return $hourly->concat($rawTail)->sortBy('recorded_at')->values();
    }
}
