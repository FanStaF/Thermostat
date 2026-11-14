<?php

namespace App\Http\Middleware;

use Closure;
use Illuminate\Http\Request;
use Symfony\Component\HttpFoundation\Response;

/**
 * Refuses device-API requests made over plain HTTP, where the API key and
 * bearer token would cross the internet in cleartext. Off unless
 * API_REQUIRE_HTTPS=true, so it can be enabled only after every device is
 * confirmed to use an https:// API_URL. X-Forwarded-Proto covers TLS that is
 * terminated by a proxy in front of PHP.
 */
class RequireHttpsForApi
{
    public function handle(Request $request, Closure $next): Response
    {
        if (config('app.api_require_https')
            && !$request->secure()
            && strtolower((string) $request->header('X-Forwarded-Proto')) !== 'https') {
            return response()->json(['error' => 'HTTPS required'], 403);
        }

        return $next($request);
    }
}
