<?php

namespace App\Http\Middleware;

use App\Models\User;
use Closure;
use Illuminate\Http\Request;
use Symfony\Component\HttpFoundation\Response;

/**
 * Devices are also Sanctum-authenticatable, so a firmware token passes plain
 * auth:sanctum. User-facing endpoints must only accept real users.
 */
class EnsureAuthenticatedUser
{
    public function handle(Request $request, Closure $next): Response
    {
        if (!$request->user() instanceof User) {
            return response()->json(['error' => 'User authentication required'], 401);
        }

        return $next($request);
    }
}
