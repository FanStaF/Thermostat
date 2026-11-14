<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use App\Models\Device;
use App\Models\DeviceSetting;
use App\Models\User;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Validator;
use Laravel\Sanctum\PersonalAccessToken;

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

        // The shared API key is compiled into every firmware image, so it can
        // only be trusted to create NEW devices. Re-registering an existing
        // MAC (which revokes its tokens and issues a new one) additionally
        // requires that device's current token, or a one-time admin approval.
        $existing = Device::where('mac_address', $request->mac_address)->first();
        if ($existing && !$existing->reregistration_allowed) {
            $presented = $request->bearerToken()
                ? PersonalAccessToken::findToken($request->bearerToken())
                : null;
            $ownsToken = $presented
                && $presented->tokenable instanceof Device
                && (int) $presented->tokenable->id === (int) $existing->id;

            if (!$ownsToken) {
                return response()->json([
                    'error' => 'Device already registered. Present the current device token, '
                        . 'or have an admin allow re-registration on the dashboard.',
                ], 403);
            }
        }

        $device = Device::updateOrCreate(
            ['mac_address' => $request->mac_address],
            [
                'hostname' => $request->hostname,
                'ip_address' => $request->ip_address,
                'firmware_version' => $request->firmware_version,
                'name' => $request->name ?? $request->hostname,
                'last_seen_at' => now(),
                // The device re-registers on every boot; any fault reported
                // before the reboot is stale. The first heartbeat (seconds
                // after registering) reports the current state.
                'sensor_fault' => false,
                'last_good_reading_at' => null,
                'reregistration_allowed' => false,
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
        $validator = Validator::make($request->all(), [
            'ip_address' => 'nullable|ip',
            'firmware_version' => 'nullable|string|max:50',
            'sensor_fault' => 'nullable|boolean',
            // Capped at the firmware's millis() wrap (~49.7 days) so the
            // derived timestamp always fits the TIMESTAMP column.
            'seconds_since_good_reading' => 'nullable|integer|min:0|max:4294967',
        ]);

        if ($validator->fails()) {
            return response()->json(['errors' => $validator->errors()], 422);
        }

        $validated = $validator->validated();
        $update = ['last_seen_at' => now()];

        // Plain fields: update only when sent with a value.
        foreach (['ip_address', 'firmware_version'] as $field) {
            if (isset($validated[$field]) && $validated[$field] !== '') {
                $update[$field] = $validated[$field];
            }
        }

        // Fault fields (firmware >= 2.2.0; older firmware omits them, which
        // leaves the stored values alone). sensor_fault: null is ignored.
        // seconds_since_good_reading: an explicit null means "no good reading
        // since boot" and clears the timestamp.
        if (isset($validated['sensor_fault'])) {
            $update['sensor_fault'] = (bool) $validated['sensor_fault'];
        }
        if (array_key_exists('seconds_since_good_reading', $validated)) {
            $seconds = $validated['seconds_since_good_reading'];
            $update['last_good_reading_at'] = $seconds === null ? null : now()->subSeconds((int) $seconds);
        }

        $device->update($update);

        return response()->json([
            'message' => 'Heartbeat updated',
            'pending_commands' => $device->pendingCommandCount(),
        ]);
    }

    /**
     * Get all devices
     */
    public function index()
    {
        $user = auth()->user();
        $query = Device::with(['settings', 'relays.currentState'])
            ->withCount('temperatureReadings');
        if (!$user->isAdmin()) {
            $query->whereIn('id', $user->devices()->pluck('devices.id'));
        }

        return response()->json($query->get());
    }

    /**
     * Admin: let an existing device register again without its current token
     * (e.g. after its flash, and stored token, was erased). One-time; cleared
     * by the next successful registration.
     */
    public function allowReregistration(Device $device)
    {
        $device->update(['reregistration_allowed' => true]);

        return response()->json(['message' => 'Device may re-register once']);
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
        // Reached by the device itself (device.scope) or by a logged-in user's
        // browser (web route); users must have access to this device.
        $user = auth()->user();
        if ($user instanceof User && !$user->canAccessDevice($device->id)) {
            return response()->json(['error' => 'You do not have permission to access this device'], 403);
        }

        $device->load([
            'relays.currentState',
            'temperatureReadings' => fn ($q) => $q->latest('recorded_at')->limit(1),
        ]);

        return response()->json([
            'version' => $device->dataVersion(),
            'lastSeenAt' => $device->last_seen_at?->toIso8601String(),
            'isOnline' => $device->is_online,
            'updateFrequency' => $device->updateFrequency(),
            'latestReading' => $device->temperatureReadings->first(),
            'relays' => $device->relays,
            'sensorFault' => $device->has_active_sensor_fault,
            'sensorFaultMessage' => $device->sensor_fault_message,
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
            'update_frequency' => 'nullable|integer|min:5|max:300',
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
