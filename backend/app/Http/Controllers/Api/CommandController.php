<?php

namespace App\Http\Controllers\Api;

use App\Http\Controllers\Controller;
use App\Models\Device;
use App\Models\DeviceCommand;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Validator;

class CommandController extends Controller
{
    /**
     * Get pending commands for a device (polled by ESP8266)
     */
    public function pending($deviceId)
    {
        $device = Device::find($deviceId);

        if (!$device) {
            return response()->json(['error' => 'Device not found'], 404);
        }

        $commands = DeviceCommand::where('device_id', $deviceId)
            ->where('status', 'pending')
            ->orderBy('created_at', 'asc')
            ->get();

        // Update device last_seen_at
        $device->update(['last_seen_at' => now()]);

        return response()->json(['commands' => $commands]);
    }

    /**
     * Create a new command for a device (from web interface)
     */
    public function store(Request $request, $deviceId)
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

            // Check if user has control permissions (viewers can't send commands)
            if (!$user->canControl()) {
                return response()->json(['error' => 'You do not have permission to control devices'], 403);
            }
        }

        $validator = Validator::make($request->all(), [
            'type' => 'required|in:set_relay_mode,set_thresholds,set_frequency,set_unit,restart',
            'params' => 'required|array',
        ]);

        if ($validator->fails()) {
            return response()->json(['errors' => $validator->errors()], 422);
        }

        $command = DeviceCommand::create([
            'device_id' => $deviceId,
            'type' => $request->type,
            'params' => $request->params,
            'status' => 'pending',
        ]);

        return response()->json([
            'message' => 'Command created',
            'command_id' => $command->id,
        ], 201);
    }

    /**
     * Update command status (acknowledged/completed/failed by device)
     */
    public function update(Request $request, $deviceId, $commandId)
    {
        $command = DeviceCommand::where('device_id', $deviceId)
            ->where('id', $commandId)
            ->first();

        if (!$command) {
            return response()->json(['error' => 'Command not found'], 404);
        }

        $validator = Validator::make($request->all(), [
            'status' => 'required|in:acknowledged,completed,failed',
            'result' => 'nullable|array',
        ]);

        if ($validator->fails()) {
            return response()->json(['errors' => $validator->errors()], 422);
        }

        $updates = ['status' => $request->status];

        if ($request->status === 'acknowledged') {
            $updates['acknowledged_at'] = now();
        } elseif (in_array($request->status, ['completed', 'failed'])) {
            $updates['completed_at'] = now();
            if ($request->has('result')) {
                $updates['result'] = $request->result;
            }
        }

        $command->update($updates);

        return response()->json([
            'message' => 'Command status updated',
            'command' => $command,
        ]);
    }

    /**
     * Get all commands for a device
     */
    public function index($deviceId)
    {
        $device = Device::find($deviceId);

        if (!$device) {
            return response()->json(['error' => 'Device not found'], 404);
        }

        $commands = DeviceCommand::where('device_id', $deviceId)
            ->latest('created_at')
            ->limit(50)
            ->get();

        return response()->json($commands);
    }
}
