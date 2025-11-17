<?php

namespace App\Http\Controllers\Api;

use App\AlertType;
use App\Http\Controllers\Controller;
use App\Models\AlertSubscription;
use App\Models\User;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Validator;

class AlertSubscriptionController extends Controller
{
    /**
     * Get all available alert types
     */
    public function types()
    {
        $types = collect(AlertType::cases())->map(function ($type) {
            return [
                'value' => $type->value,
                'label' => $type->label(),
                'description' => $type->description(),
                'category' => $type->category(),
                'requires_permission' => $type->requiresPermission(),
            ];
        })->groupBy('category');

        return response()->json($types);
    }

    /**
     * Get subscriptions for a user
     * Admins can specify user_id, others get their own
     */
    public function index(Request $request)
    {
        $user = auth()->user();
        $targetUserId = $request->input('user_id', $user->id);

        // Only admins can view other users' subscriptions
        if ($targetUserId != $user->id && $user->role !== 'admin') {
            return response()->json(['error' => 'Unauthorized'], 403);
        }

        $subscriptions = AlertSubscription::with(['device'])
            ->where('user_id', $targetUserId)
            ->get()
            ->map(function ($sub) {
                return [
                    'id' => $sub->id,
                    'alert_type' => $sub->alert_type->value,
                    'alert_label' => $sub->alert_type->label(),
                    'device_id' => $sub->device_id,
                    'device_name' => $sub->device?->name ?? 'All Devices',
                    'enabled' => $sub->enabled,
                    'settings' => $sub->settings,
                    'created_at' => $sub->created_at,
                ];
            });

        return response()->json($subscriptions);
    }

    /**
     * Create a new alert subscription
     */
    public function store(Request $request)
    {
        $user = auth()->user();
        $targetUserId = $request->input('user_id', $user->id);

        // Only admins can create subscriptions for other users
        if ($targetUserId != $user->id && $user->role !== 'admin') {
            return response()->json(['error' => 'Unauthorized'], 403);
        }

        $validator = Validator::make($request->all(), [
            'user_id' => 'nullable|exists:users,id',
            'device_id' => 'nullable|exists:devices,id',
            'alert_type' => 'required|string',
            'enabled' => 'boolean',
            'settings' => 'nullable|array',
            'cooldown_minutes' => 'nullable|integer|min:1|max:1440',
            'scheduled_time' => 'nullable|date_format:H:i',
        ]);

        if ($validator->fails()) {
            return response()->json(['errors' => $validator->errors()], 422);
        }

        // Validate alert type
        $alertType = AlertType::tryFrom($request->alert_type);
        if (!$alertType) {
            return response()->json(['error' => 'Invalid alert type'], 422);
        }

        // Check if user has permission for this alert type
        $targetUser = User::find($targetUserId);
        $requiredPermission = $alertType->requiresPermission();

        if ($requiredPermission === 'user' && $targetUser->role === 'viewer') {
            return response()->json([
                'error' => 'This alert type requires user or admin role'
            ], 403);
        }

        // Check for existing subscription
        $existing = AlertSubscription::where('user_id', $targetUserId)
            ->where('device_id', $request->device_id)
            ->where('alert_type', $request->alert_type)
            ->first();

        if ($existing) {
            return response()->json([
                'error' => 'Subscription already exists',
                'subscription' => $existing
            ], 409);
        }

        $subscription = AlertSubscription::create([
            'user_id' => $targetUserId,
            'device_id' => $request->device_id,
            'alert_type' => $request->alert_type,
            'enabled' => $request->input('enabled', true),
            'settings' => $request->settings,
            'cooldown_minutes' => $request->input('cooldown_minutes', 30),
            'scheduled_time' => $request->scheduled_time,
        ]);

        return response()->json([
            'message' => 'Alert subscription created',
            'subscription' => $subscription
        ], 201);
    }

    /**
     * Update an alert subscription
     */
    public function update(Request $request, $id)
    {
        $subscription = AlertSubscription::find($id);

        if (!$subscription) {
            return response()->json(['error' => 'Subscription not found'], 404);
        }

        $user = auth()->user();

        // Check permissions: own subscription or admin
        if ($subscription->user_id != $user->id && $user->role !== 'admin') {
            return response()->json(['error' => 'Unauthorized'], 403);
        }

        $validator = Validator::make($request->all(), [
            'enabled' => 'boolean',
            'settings' => 'nullable|array',
        ]);

        if ($validator->fails()) {
            return response()->json(['errors' => $validator->errors()], 422);
        }

        $subscription->update($request->only(['enabled', 'settings']));

        return response()->json([
            'message' => 'Subscription updated',
            'subscription' => $subscription
        ]);
    }

    /**
     * Delete an alert subscription
     */
    public function destroy($id)
    {
        $subscription = AlertSubscription::find($id);

        if (!$subscription) {
            return response()->json(['error' => 'Subscription not found'], 404);
        }

        $user = auth()->user();

        // Check permissions: own subscription or admin
        if ($subscription->user_id != $user->id && $user->role !== 'admin') {
            return response()->json(['error' => 'Unauthorized'], 403);
        }

        $subscription->delete();

        return response()->json(['message' => 'Subscription deleted']);
    }

    /**
     * Get alert logs for user's subscriptions
     */
    public function logs(Request $request)
    {
        $user = auth()->user();
        $targetUserId = $request->input('user_id', $user->id);

        // Only admins can view other users' logs
        if ($targetUserId != $user->id && $user->role !== 'admin') {
            return response()->json(['error' => 'Unauthorized'], 403);
        }

        $subscriptionIds = AlertSubscription::where('user_id', $targetUserId)
            ->pluck('id');

        $logs = \App\Models\AlertLog::with(['subscription', 'device'])
            ->whereIn('alert_subscription_id', $subscriptionIds)
            ->latest('triggered_at')
            ->limit(100)
            ->get()
            ->map(function ($log) {
                return [
                    'id' => $log->id,
                    'alert_type' => $log->subscription->alert_type->value,
                    'alert_label' => $log->subscription->alert_type->label(),
                    'device_name' => $log->device->name,
                    'triggered_at' => $log->triggered_at,
                    'resolved_at' => $log->resolved_at,
                    'data' => $log->data,
                ];
            });

        return response()->json($logs);
    }
}
