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
    public function updateState(Request $request, Device $device)
    {
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

        $relay = Relay::firstOrCreate(
            [
                'device_id' => $device->id,
                'relay_number' => $request->relay_number,
            ],
            [
                'name' => $request->name ?? "Relay {$request->relay_number}",
            ]
        );

        $state = RelayState::create([
            'relay_id' => $relay->id,
            'state' => $request->state,
            'mode' => $request->mode,
            'temp_on' => $request->temp_on,
            'temp_off' => $request->temp_off,
            'changed_at' => now(),
        ]);

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
    public function index(Device $device)
    {
        return response()->json(
            Relay::where('device_id', $device->id)->with('currentState')->get()
        );
    }

    /**
     * Get relay state history
     */
    public function history(Device $device, $relayNumber)
    {
        $relay = Relay::where('device_id', $device->id)
            ->where('relay_number', $relayNumber)
            ->first();

        if (!$relay) {
            return response()->json(['error' => 'Relay not found'], 404);
        }

        return response()->json(
            RelayState::where('relay_id', $relay->id)
                ->latest('changed_at')
                ->limit(100)
                ->get()
        );
    }

    /**
     * Update relay name / type (web)
     */
    public function update(Request $request, Device $device, Relay $relay)
    {
        if ((int) $relay->device_id !== (int) $device->id) {
            return response()->json(['error' => 'Relay not found'], 404);
        }

        if (auth()->check()) {
            $user = auth()->user();
            if (!$user->canAccessDevice($device->id)) {
                return response()->json(['error' => 'You do not have permission to access this device'], 403);
            }
            if (!$user->canControl()) {
                return response()->json(['error' => 'You do not have permission to edit relays'], 403);
            }
        }

        $validator = Validator::make($request->all(), [
            'name' => 'sometimes|string|max:255',
            'relay_type' => 'sometimes|in:HEATING,COOLING,GENERIC,MANUAL_ONLY',
        ]);

        if ($validator->fails()) {
            return response()->json(['errors' => $validator->errors()], 422);
        }

        $relay->update($request->only(['name', 'relay_type']));

        return response()->json([
            'message' => 'Relay updated successfully',
            'relay' => $relay,
        ]);
    }
}
