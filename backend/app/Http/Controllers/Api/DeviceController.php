<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use App\Models\Device;
use App\Models\DeviceSetting;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Validator;

class DeviceController extends Controller
{
    /**
     * Register a new device or update existing one
     */
    public function register(Request $request)
    {
        $validator = Validator::make($request->all(), [
            'hostname' => 'required|string|max:255',
            'mac_address' => 'required|string|max:17',
            'ip_address' => 'nullable|ip',
            'firmware_version' => 'nullable|string|max:50',
            'name' => 'nullable|string|max:255',
        ]);

        if ($validator->fails()) {
            return response()->json(['errors' => $validator->errors()], 422);
        }

        $device = Device::updateOrCreate(
            ['mac_address' => $request->mac_address],
            [
                'hostname' => $request->hostname,
                'ip_address' => $request->ip_address,
                'firmware_version' => $request->firmware_version,
                'name' => $request->name ?? $request->hostname,
                'last_seen_at' => now(),
                'is_online' => true,
            ]
        );

        // Create default settings if device is new
        if ($device->wasRecentlyCreated) {
            DeviceSetting::create([
                'device_id' => $device->id,
                'update_frequency' => 5,
                'use_fahrenheit' => false,
                'timezone' => 'UTC',
            ]);
        }

        // Revoke old tokens and create new one
        $device->tokens()->delete();
        $token = $device->createToken('device-token')->plainTextToken;

        return response()->json([
            'device_id' => $device->id,
            'token' => $token,
            'message' => 'Device registered successfully',
        ], 201);
    }

    /**
     * Update device heartbeat (keep-alive)
     */
    public function heartbeat(Request $request, $deviceId)
    {
        $device = Device::find($deviceId);

        if (!$device) {
            return response()->json(['error' => 'Device not found'], 404);
        }

        $device->update([
            'last_seen_at' => now(),
            'is_online' => true,
            'ip_address' => $request->ip_address ?? $device->ip_address,
        ]);

        return response()->json(['message' => 'Heartbeat updated'], 200);
    }

    /**
     * Get all devices
     */
    public function index()
    {
        $devices = Device::with(['settings', 'relays.currentState'])
            ->withCount('temperatureReadings')
            ->get();

        return response()->json($devices);
    }

    /**
     * Get a specific device
     */
    public function show($deviceId)
    {
        $device = Device::with([
            'settings',
            'relays.currentState',
            'temperatureReadings' => function ($query) {
                $query->latest('recorded_at')->limit(100);
            }
        ])->find($deviceId);

        if (!$device) {
            return response()->json(['error' => 'Device not found'], 404);
        }

        return response()->json($device);
    }

    /**
     * Update device name
     */
    public function update(Request $request, $deviceId)
    {
        $device = Device::find($deviceId);

        if (!$device) {
            return response()->json(['error' => 'Device not found'], 404);
        }

        // Check permissions when called from web (has auth user)
        if (auth()->check()) {
            $user = auth()->user();

            // Check if user has access to this device
            if (!$user->canAccessDevice($deviceId)) {
                return response()->json(['error' => 'You do not have permission to access this device'], 403);
            }

            // Check if user has control permissions (viewers can't edit)
            if (!$user->canControl()) {
                return response()->json(['error' => 'You do not have permission to edit devices'], 403);
            }
        }

        $validator = Validator::make($request->all(), [
            'name' => 'required|string|max:255',
        ]);

        if ($validator->fails()) {
            return response()->json(['errors' => $validator->errors()], 422);
        }

        $device->update([
            'name' => $request->name,
        ]);

        return response()->json([
            'message' => 'Device updated successfully',
            'device' => $device
        ], 200);
    }
}
