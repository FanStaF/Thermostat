<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use App\Models\Device;
use App\Models\Relay;
use App\Models\RelayState;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Validator;

class RelayController extends Controller
{
    /**
     * Update relay state from device
     */
    public function updateState(Request $request, $deviceId)
    {
        $device = Device::find($deviceId);

        if (!$device) {
            return response()->json(['error' => 'Device not found'], 404);
        }

        $validator = Validator::make($request->all(), [
            'relay_number' => 'required|integer|between:1,4',
            'state' => 'required|boolean',
            'mode' => 'required|in:AUTO,MANUAL_ON,MANUAL_OFF',
            'temp_on' => 'required|numeric',
            'temp_off' => 'required|numeric',
            'name' => 'nullable|string|max:255',
        ]);

        if ($validator->fails()) {
            return response()->json(['errors' => $validator->errors()], 422);
        }

        // Get or create relay
        $relay = Relay::firstOrCreate(
            [
                'device_id' => $deviceId,
                'relay_number' => $request->relay_number,
            ],
            [
                'name' => $request->name ?? "Relay {$request->relay_number}",
            ]
        );

        // Create state record
        $state = RelayState::create([
            'relay_id' => $relay->id,
            'state' => $request->state,
            'mode' => $request->mode,
            'temp_on' => $request->temp_on,
            'temp_off' => $request->temp_off,
            'changed_at' => now(),
        ]);

        // Update device last_seen_at
        $device->update(['last_seen_at' => now()]);

        return response()->json([
            'message' => 'Relay state updated',
            'relay_id' => $relay->id,
            'state_id' => $state->id,
        ], 201);
    }

    /**
     * Get all relays for a device
     */
    public function index($deviceId)
    {
        $device = Device::find($deviceId);

        if (!$device) {
            return response()->json(['error' => 'Device not found'], 404);
        }

        $relays = Relay::where('device_id', $deviceId)
            ->with('currentState')
            ->get();

        return response()->json($relays);
    }

    /**
     * Get relay state history
     */
    public function history($deviceId, $relayNumber)
    {
        $relay = Relay::where('device_id', $deviceId)
            ->where('relay_number', $relayNumber)
            ->first();

        if (!$relay) {
            return response()->json(['error' => 'Relay not found'], 404);
        }

        $states = RelayState::where('relay_id', $relay->id)
            ->latest('changed_at')
            ->limit(100)
            ->get();

        return response()->json($states);
    }

    /**
     * Update relay name
     */
    public function update(Request $request, $deviceId, $relayId)
    {
        $relay = Relay::where('device_id', $deviceId)
            ->where('id', $relayId)
            ->first();

        if (!$relay) {
            return response()->json(['error' => 'Relay not found'], 404);
        }

        $validator = Validator::make($request->all(), [
            'name' => 'required|string|max:255',
        ]);

        if ($validator->fails()) {
            return response()->json(['errors' => $validator->errors()], 422);
        }

        $relay->update([
            'name' => $request->name,
        ]);

        return response()->json([
            'message' => 'Relay updated successfully',
            'relay' => $relay
        ], 200);
    }
}
