<?php

namespace App\Http\Middleware;

use App\Models\Device;
use Closure;
use Illuminate\Http\Request;
use Symfony\Component\HttpFoundation\Response;

/**
 * Ensures the Sanctum-authenticated device matches the {device} route binding.
 * Without this, a leaked token from device A could call /devices/{B}/heartbeat
 * and impersonate device B.
 */
class EnsureDeviceTokenMatchesRoute
{
    public function handle(Request $request, Closure $next): Response
    {
        $authed = $request->user();
        $routeDevice = $request->route('device');

        if (!$authed instanceof Device) {
            return response()->json(['error' => 'Device authentication required'], 401);
        }

        // Resolve the bound device id whether the route gives us a model or raw id.
        $routeDeviceId = $routeDevice instanceof Device ? $routeDevice->id : (int) $routeDevice;

        if ((int) $authed->id !== $routeDeviceId) {
            return response()->json(['error' => 'Device token does not match the requested device'], 403);
        }

        return $next($request);
    }
}
