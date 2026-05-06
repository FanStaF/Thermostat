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
    public function pending(Device $device)
    {
        $commands = DeviceCommand::where('device_id', $device->id)
            ->where('status', 'pending')
            ->orderBy('created_at', 'asc')
            ->get();

        $device->update(['last_seen_at' => now()]);

        return response()->json(['commands' => $commands]);
    }

    /**
     * Create a new command for a device (from web interface)
     */
    public function store(Request $request, Device $device)
    {
        if (auth()->check()) {
            $user = auth()->user();
            if (!$user->canAccessDevice($device->id)) {
                return response()->json(['error' => 'You do not have permission to access this device'], 403);
            }
            if (!$user->canControl()) {
                return response()->json(['error' => 'You do not have permission to control devices'], 403);
            }
        }

        $validator = Validator::make($request->all(), [
            'type' => 'required|in:set_relay_mode,set_relay_type,set_thresholds,set_frequency,set_unit,restart',
            'params' => 'required|array',
        ]);

        if ($validator->fails()) {
            return response()->json(['errors' => $validator->errors()], 422);
        }

        $command = DeviceCommand::create([
            'device_id' => $device->id,
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
    public function update(Request $request, Device $device, DeviceCommand $command)
    {
        if ((int) $command->device_id !== (int) $device->id) {
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
    public function index(Device $device)
    {
        return response()->json(
            DeviceCommand::where('device_id', $device->id)
                ->latest('created_at')
                ->limit(50)
                ->get()
        );
    }
}
