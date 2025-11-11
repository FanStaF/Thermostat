<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use App\Models\Device;
use App\Models\TemperatureReading;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Validator;

class TemperatureController extends Controller
{
    /**
     * Store temperature reading from device
     */
    public function store(Request $request, $deviceId)
    {
        $device = Device::find($deviceId);

        if (!$device) {
            return response()->json(['error' => 'Device not found'], 404);
        }

        $validator = Validator::make($request->all(), [
            'temperature' => 'required|numeric|between:-50,150',
            'sensor_id' => 'nullable|integer|between:0,255',
        ]);

        if ($validator->fails()) {
            return response()->json(['errors' => $validator->errors()], 422);
        }

        $reading = TemperatureReading::create([
            'device_id' => $deviceId,
            'temperature' => $request->temperature,
            'sensor_id' => $request->sensor_id ?? 0,
            'recorded_at' => now(),
        ]);

        // Update device last_seen_at
        $device->update(['last_seen_at' => now()]);

        return response()->json([
            'message' => 'Temperature reading stored',
            'reading_id' => $reading->id,
        ], 201);
    }

    /**
     * Get temperature readings for a device
     */
    public function index(Request $request, $deviceId)
    {
        $device = Device::find($deviceId);

        if (!$device) {
            return response()->json(['error' => 'Device not found'], 404);
        }

        $limit = $request->input('limit', 100);
        $sensorId = $request->input('sensor_id');

        $query = TemperatureReading::where('device_id', $deviceId)
            ->latest('recorded_at');

        if ($sensorId !== null) {
            $query->where('sensor_id', $sensorId);
        }

        $readings = $query->limit($limit)->get();

        return response()->json($readings);
    }

    /**
     * Get temperature statistics for a device
     */
    public function stats($deviceId)
    {
        $device = Device::find($deviceId);

        if (!$device) {
            return response()->json(['error' => 'Device not found'], 404);
        }

        $last24Hours = TemperatureReading::where('device_id', $deviceId)
            ->where('recorded_at', '>=', now()->subDay())
            ->get();

        $stats = [
            'count' => $last24Hours->count(),
            'avg' => $last24Hours->avg('temperature'),
            'min' => $last24Hours->min('temperature'),
            'max' => $last24Hours->max('temperature'),
            'latest' => $last24Hours->sortByDesc('recorded_at')->first(),
        ];

        return response()->json($stats);
    }
}
