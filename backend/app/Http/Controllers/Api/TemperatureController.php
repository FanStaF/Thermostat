<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use App\Models\Device;
use App\Models\TemperatureReading;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Validator;

class TemperatureController extends Controller
{
    private const MAX_BATCH = 100;
    private const MAX_READING_AGE_HOURS = 48;

    /**
     * Store temperature reading from device
     */
    public function store(Request $request, Device $device)
    {
        $validator = Validator::make($request->all(), [
            'temperature' => 'required|numeric|between:-50,150',
            'sensor_id' => 'nullable|integer|between:0,255',
        ]);

        if ($validator->fails()) {
            return response()->json(['errors' => $validator->errors()], 422);
        }

        $reading = TemperatureReading::create([
            'device_id' => $device->id,
            'temperature' => $request->temperature,
            'sensor_id' => $request->sensor_id ?? 0,
            'recorded_at' => now(),
        ]);

        $device->update(['last_seen_at' => now()]);

        return response()->json([
            'message' => 'Temperature reading stored',
            'reading_id' => $reading->id,
            'pending_commands' => $device->pendingCommandCount(),
        ], 201);
    }

    /**
     * Store a batch of readings with the device's own timestamps (firmware
     * >= 2.4.0 outbox). Readings measured while the server was unreachable
     * keep their real time. Idempotent: a retried batch (response lost) does
     * not create duplicates.
     */
    public function storeBatch(Request $request, Device $device)
    {
        $validator = Validator::make($request->all(), [
            'readings' => 'required|array|min:1|max:' . self::MAX_BATCH,
            'readings.*.temperature' => 'required|numeric|between:-50,150',
            'readings.*.sensor_id' => 'nullable|integer|between:0,255',
            // Unix seconds; omitted/0 when the device had no clock yet.
            'readings.*.recorded_at' => 'nullable|integer|min:0',
        ]);

        if ($validator->fails()) {
            return response()->json(['errors' => $validator->errors()], 422);
        }

        $now = now();
        // Older than the downsample window would be missed by hourly rollups;
        // future times mean a bad device clock.
        $oldest = $now->copy()->subHours(self::MAX_READING_AGE_HOURS);
        $newest = $now->copy()->addMinutes(5);

        $rows = [];
        $rejected = 0;
        foreach ($validator->validated()['readings'] as $r) {
            $ts = !empty($r['recorded_at'])
                ? \Carbon\Carbon::createFromTimestamp($r['recorded_at'], config('app.timezone'))
                : $now->copy();
            if ($ts->lt($oldest) || $ts->gt($newest)) {
                $rejected++;
                continue;
            }
            $rows[] = [
                'device_id' => $device->id,
                'temperature' => $r['temperature'],
                'sensor_id' => $r['sensor_id'] ?? 0,
                'recorded_at' => $ts->format('Y-m-d H:i:s'),
            ];
        }

        // Drop rows already stored (same sensor + second): makes retries safe.
        $duplicates = 0;
        if ($rows) {
            $times = array_column($rows, 'recorded_at');
            $existing = TemperatureReading::where('device_id', $device->id)
                ->whereBetween('recorded_at', [min($times), max($times)])
                ->get(['sensor_id', 'recorded_at'])
                ->map(fn ($e) => $e->sensor_id . '|' . $e->recorded_at->format('Y-m-d H:i:s'))
                ->flip();
            $fresh = [];
            foreach ($rows as $row) {
                $key = $row['sensor_id'] . '|' . $row['recorded_at'];
                if (isset($existing[$key])) {
                    $duplicates++;
                    continue;
                }
                $existing[$key] = true;
                $fresh[] = $row;
            }
            if ($fresh) {
                TemperatureReading::insert($fresh);
            }
            $rows = $fresh;
        }

        $device->update(['last_seen_at' => $now]);

        return response()->json([
            'stored' => count($rows),
            'duplicates' => $duplicates,
            'rejected' => $rejected,
            'pending_commands' => $device->pendingCommandCount(),
        ], 201);
    }

    /**
     * Get temperature readings for a device
     */
    public function index(Request $request, Device $device)
    {
        $limit = $request->input('limit', 100);
        $sensorId = $request->input('sensor_id');

        $query = TemperatureReading::where('device_id', $device->id)
            ->latest('recorded_at');

        if ($sensorId !== null) {
            $query->where('sensor_id', $sensorId);
        }

        return response()->json($query->limit($limit)->get());
    }

    /**
     * Get temperature statistics for a device
     */
    public function stats(Device $device)
    {
        $last24Hours = TemperatureReading::where('device_id', $device->id)
            ->where('recorded_at', '>=', now()->subDay())
            ->get();

        return response()->json([
            'count' => $last24Hours->count(),
            'avg' => $last24Hours->avg('temperature'),
            'min' => $last24Hours->min('temperature'),
            'max' => $last24Hours->max('temperature'),
            'latest' => $last24Hours->sortByDesc('recorded_at')->first(),
        ]);
    }
}
