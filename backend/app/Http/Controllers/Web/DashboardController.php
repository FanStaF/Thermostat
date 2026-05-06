<?php

namespace App\Http\Controllers\Web;

use App\Http\Controllers\Controller;
use App\Models\Device;
use Carbon\Carbon;
use Illuminate\Http\Request;
use Illuminate\Support\Collection;
use Illuminate\Support\Facades\DB;

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
     * Cap the chart at ~hundreds-to-low-thousands of points by aggregating
     * inside the database. For longer ranges we'd otherwise pull every raw
     * row (e.g. ~150k for 30 days at 18s update freq) and let JavaScript
     * choke. Bucket size per range:
     *
     *   1h, 6h → raw (no aggregation; ranges are short enough)
     *   24h    → 1-min   ≈ 1440 points
     *   7d     → 5-min   ≈ 2016 points
     *   30d    → 1-hour  ≈  720 points
     *   all    → 1-day   (bounded by uptime in days)
     */
    private function loadReadings(Device $device, string $range): Collection
    {
        return match ($range) {
            '1h'  => $this->rawReadings($device, now()->subHour()),
            '6h'  => $this->rawReadings($device, now()->subHours(6)),
            '24h' => $this->bucketedReadings($device, now()->subDay(),    60),
            '7d'  => $this->bucketedReadings($device, now()->subDays(7),  300),
            '30d' => $this->bucketedReadings($device, now()->subDays(30), 3600),
            'all' => $this->bucketedReadings($device, null,               86400),
            default => $this->rawReadings($device, now()->subDay()),
        };
    }

    private function rawReadings(Device $device, Carbon $start): Collection
    {
        return $device->temperatureReadings()
            ->where('recorded_at', '>=', $start)
            ->orderBy('recorded_at')
            ->get(['recorded_at', 'temperature']);
    }

    /**
     * Bucket raw + hourly-aggregate rows into $bucketSeconds-wide windows in
     * a single DB round trip. UNION ALL lets a row come from either source —
     * after the downsample command runs, raw older than ~30 days is gone and
     * the hourly table is the only source for that span. Sample-weighted
     * average preserves accuracy at the boundary where both contribute.
     *
     * MySQL-only: uses UNIX_TIMESTAMP / FROM_UNIXTIME.
     */
    private function bucketedReadings(Device $device, ?Carbon $start, int $bucketSeconds): Collection
    {
        $startStr = $start?->format('Y-m-d H:i:s');
        $hourlyWhere = $startStr ? 'AND bucket_start >= ?' : '';
        $rawWhere    = $startStr ? 'AND recorded_at >= ?' : '';

        $sql = "
            SELECT
                FROM_UNIXTIME(FLOOR(UNIX_TIMESTAMP(t) / ?) * ?) AS recorded_at,
                SUM(temp * weight) / SUM(weight) AS temperature,
                MIN(min_t) AS min_temp,
                MAX(max_t) AS max_temp
            FROM (
                SELECT bucket_start AS t, avg_temp AS temp, min_temp AS min_t,
                       max_temp AS max_t, sample_count AS weight
                FROM temperature_readings_hourly
                WHERE device_id = ? {$hourlyWhere}
                UNION ALL
                SELECT recorded_at AS t, temperature AS temp, temperature AS min_t,
                       temperature AS max_t, 1 AS weight
                FROM temperature_readings
                WHERE device_id = ? {$rawWhere}
            ) AS combined
            GROUP BY FLOOR(UNIX_TIMESTAMP(t) / ?)
            ORDER BY recorded_at
        ";

        $params = [$bucketSeconds, $bucketSeconds, $device->id];
        if ($startStr) {
            $params[] = $startStr;
        }
        $params[] = $device->id;
        if ($startStr) {
            $params[] = $startStr;
        }
        $params[] = $bucketSeconds;

        $rows = DB::select($sql, $params);

        return collect($rows)->map(fn ($r) => (object) [
            'recorded_at' => Carbon::parse($r->recorded_at),
            'temperature' => (float) $r->temperature,
            'min_temp'    => (float) $r->min_temp,
            'max_temp'    => (float) $r->max_temp,
        ]);
    }
}
