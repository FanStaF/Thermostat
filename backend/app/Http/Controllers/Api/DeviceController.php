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
            ]
        );

        if ($device->wasRecentlyCreated) {
            DeviceSetting::create([
                'device_id' => $device->id,
                'update_frequency' => 5,
                'use_fahrenheit' => false,
                'timezone' => 'UTC',
            ]);
        }

        // Revoke old tokens and create a new one. The firmware re-registers on
        // every boot, so this happens often — fine for now, the table is small.
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
    public function heartbeat(Request $request, Device $device)
    {
        $device->update([
            'last_seen_at' => now(),
            'ip_address' => $request->ip_address ?? $device->ip_address,
        ]);

        return response()->json(['message' => 'Heartbeat updated']);
    }

    /**
     * Get all devices
     */
    public function index()
    {
        return response()->json(
            Device::with(['settings', 'relays.currentState'])
                ->withCount('temperatureReadings')
                ->get()
        );
    }

    /**
     * Get a specific device
     */
    public function show(Device $device)
    {
        $device->load([
            'settings',
            'relays.currentState',
            'temperatureReadings' => fn ($q) => $q->latest('recorded_at')->limit(100),
        ]);

        return response()->json($device);
    }

    /**
     * Update device name
     */
    public function update(Request $request, Device $device)
    {
        if (auth()->check()) {
            $user = auth()->user();
            if (!$user->canAccessDevice($device->id)) {
                return response()->json(['error' => 'You do not have permission to access this device'], 403);
            }
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

        $device->update(['name' => $request->name]);

        return response()->json([
            'message' => 'Device updated successfully',
            'device' => $device,
        ]);
    }

    /**
     * Get dashboard data for AJAX polling
     */
    public function dashboardData(Device $device)
    {
        $device->load([
            'relays.currentState',
            'temperatureReadings' => fn ($q) => $q->latest('recorded_at')->limit(1),
        ]);

        return response()->json([
            'latestReading' => $device->temperatureReadings->first(),
            'relays' => $device->relays,
        ]);
    }

    /**
     * Update device settings
     */
    public function updateSettings(Request $request, Device $device)
    {
        $device->load('settings');

        if (auth()->check()) {
            $user = auth()->user();
            if (!$user->canAccessDevice($device->id)) {
                return response()->json(['error' => 'You do not have permission to access this device'], 403);
            }
            if (!$user->canControl()) {
                return response()->json(['error' => 'You do not have permission to edit device settings'], 403);
            }
        }

        $validator = Validator::make($request->all(), [
            'update_frequency' => 'nullable|integer|min:1|max:60',
            'use_fahrenheit' => 'nullable|boolean',
            'timezone' => 'nullable|string|max:50',
        ]);

        if ($validator->fails()) {
            return response()->json(['errors' => $validator->errors()], 422);
        }

        $payload = $request->only(['update_frequency', 'use_fahrenheit', 'timezone']);
        if ($device->settings) {
            $device->settings->update($payload);
        } else {
            $device->settings()->create($payload);
        }

        return response()->json([
            'message' => 'Settings updated successfully',
            'settings' => $device->settings,
        ]);
    }
}
