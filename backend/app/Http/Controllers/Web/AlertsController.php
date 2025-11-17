<?php

namespace App\Http\Controllers\Web;

use App\AlertType;
use App\Http\Controllers\Controller;
use App\Models\AlertSubscription;
use App\Models\Device;
use App\Models\User;
use Illuminate\Http\Request;

class AlertsController extends Controller
{
    public function index(Request $request)
    {
        $user = auth()->user();

        // Get target user (admins can view others)
        $targetUserId = $request->input('user_id', $user->id);
        $targetUser = $user;

        if ($targetUserId != $user->id) {
            if ($user->role !== 'admin') {
                abort(403, 'Unauthorized');
            }
            $targetUser = User::findOrFail($targetUserId);
        }

        // Get all alert types grouped by category
        $alertTypes = collect(AlertType::cases())->map(function ($type) use ($targetUser) {
            return [
                'value' => $type->value,
                'label' => $type->label(),
                'description' => $type->description(),
                'category' => $type->category(),
                'requires_permission' => $type->requiresPermission(),
                'can_subscribe' => $this->canUserSubscribe($targetUser, $type),
            ];
        })->groupBy('category');

        // Get user's current subscriptions
        $subscriptions = AlertSubscription::with(['device'])
            ->where('user_id', $targetUser->id)
            ->get();

        // Get available devices
        $devices = Device::all();

        // Get all users (for admin dropdown)
        $users = $user->role === 'admin' ? User::all() : collect([$user]);

        return view('alerts.index', compact(
            'alertTypes',
            'subscriptions',
            'devices',
            'targetUser',
            'users'
        ));
    }

    private function canUserSubscribe(User $user, AlertType $alertType): bool
    {
        $required = $alertType->requiresPermission();

        if ($required === 'viewer') {
            return true; // All roles can subscribe
        }

        if ($required === 'user') {
            return in_array($user->role, ['user', 'admin']);
        }

        return false;
    }
}
