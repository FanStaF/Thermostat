<!DOCTYPE html>
<html lang="{{ str_replace('_', '-', app()->getLocale()) }}">
<head>
    <meta charset="utf-8">
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <meta name="csrf-token" content="{{ csrf_token() }}">
    <title>{{ config('app.name', 'LOTR Mobile Command') }}</title>
    <link rel="icon" type="image/svg+xml" href="{{ asset('icons/homehub.svg') }}">
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, sans-serif; background: #f5f5f5; overflow-x: hidden; }
        .container { max-width: 1200px; margin: 0 auto; padding: 20px; }
        @media (max-width: 768px) {
            .container { padding: 10px; }
        }
        .header { background: #2c3e50; color: white; padding: 20px 0; margin-bottom: 30px; }
        .header h1 { font-size: 24px; font-weight: 600; }
        .card { background: white; border-radius: 8px; padding: 20px; margin-bottom: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); }
        .card-title { font-size: 18px; font-weight: 600; margin-bottom: 15px; color: #2c3e50; }
        .device-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); gap: 20px; }
        @media (max-width: 768px) {
            .device-grid { grid-template-columns: 1fr; }
        }
        .device-card { background: white; border-radius: 8px; padding: 20px; box-shadow: 0 2px 4px rgba(0,0,0,0.1); transition: transform 0.2s; }
        .device-card:hover { transform: translateY(-2px); box-shadow: 0 4px 8px rgba(0,0,0,0.15); }
        .device-name { font-size: 20px; font-weight: 600; margin-bottom: 10px; color: #2c3e50; }
        .device-info { display: flex; flex-direction: column; gap: 8px; font-size: 14px; color: #666; }
        .status { display: inline-block; padding: 4px 12px; border-radius: 12px; font-size: 12px; font-weight: 600; }
        .status.online { background: #d4edda; color: #155724; }
        .status.offline { background: #f8d7da; color: #721c24; }
        .btn { display: inline-block; padding: 10px 20px; background: #3498db; color: white; text-decoration: none; border-radius: 5px; font-size: 14px; font-weight: 500; border: none; cursor: pointer; transition: background 0.2s; }
        .btn:hover { background: #2980b9; }
        .btn-secondary { background: #95a5a6; }
        .btn-secondary:hover { background: #7f8c8d; }
        .relay-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(250px, 1fr)); gap: 15px; }
        @media (max-width: 768px) {
            .relay-grid { grid-template-columns: 1fr; }
        }
        .relay-card { background: #f8f9fa; border-radius: 6px; padding: 15px; border: 2px solid #e9ecef; }
        .relay-name { font-weight: 600; margin-bottom: 10px; }
        .relay-status { display: flex; justify-content: space-between; align-items: center; margin-bottom: 8px; font-size: 14px; }
        .relay-badge { padding: 4px 8px; border-radius: 4px; font-size: 12px; font-weight: 600; }
        .relay-badge.on { background: #d4edda; color: #155724; }
        .relay-badge.off { background: #f8d7da; color: #721c24; }
        .mode-badge { padding: 4px 8px; border-radius: 4px; font-size: 12px; background: #e3f2fd; color: #1976d2; }
        .temp-display { font-size: 32px; font-weight: 600; color: #3498db; margin: 10px 0; }
        .chart-container { position: relative; height: 400px; }
        .back-link { display: inline-block; margin-bottom: 20px; color: #3498db; text-decoration: none; font-weight: 500; }
        .back-link:hover { text-decoration: underline; }
        .mode-btn { padding: 6px 12px; border: 2px solid #3498db; background: white; color: #3498db; border-radius: 4px; font-size: 12px; font-weight: 600; cursor: pointer; transition: all 0.2s; flex: 1; }
        .mode-btn:hover { background: #e3f2fd; }
        .mode-btn.active { background: #3498db; color: white; }
        .threshold-input { font-size: 14px; }
        .range-btn { padding: 5px 12px; border: 1px solid #ddd; background: white; color: #666; border-radius: 4px; font-size: 12px; font-weight: 500; cursor: pointer; transition: all 0.2s; }
        .range-btn:hover { background: #f8f9fa; border-color: #3498db; color: #3498db; }
        .range-btn.active { background: #3498db; color: white; border-color: #3498db; }

        /* Mobile styles */
        .header-content { display: flex; justify-content: space-between; align-items: center; }
        .header-left { display: flex; align-items: center; gap: 30px; }
        .header-nav { display: flex; gap: 20px; }
        .header-right { display: flex; align-items: center; gap: 15px; }

        @media (max-width: 768px) {
            .header { padding: 15px 0; }
            .header h1 { font-size: 18px !important; }
            .header h1 img { width: 24px !important; height: 24px !important; }
            .header-content { flex-direction: column; gap: 15px; align-items: flex-start; }
            .header-left { flex-direction: column; gap: 10px; align-items: flex-start; width: 100%; }
            .header-nav { gap: 15px; font-size: 14px; }
            .header-right { width: 100%; justify-content: space-between; flex-wrap: wrap; }
            .header-right span { font-size: 12px; }
            .card { padding: 15px; }
            .card-title { font-size: 16px; }
        }
    </style>
</head>
<body>
    <div class="header">
        <div class="container header-content">
            <div class="header-left">
                <h1 style="margin: 0; display: flex; align-items: center; gap: 10px;">
                    <img src="{{ asset('icons/homehub.svg') }}" alt="LOTR Mobile Command" style="width: 32px; height: 32px;">
                    <span>{{ config('app.name', 'LOTR Mobile Command') }}</span>
                </h1>
                <nav class="header-nav">
                    <a href="{{ route('dashboard.index') }}" style="color: white; text-decoration: none; font-weight: {{ request()->routeIs('dashboard.*') ? '600' : '400' }};">
                        Dashboard
                    </a>
                    @if(auth()->user()->isAdmin())
                        <a href="{{ route('users.index') }}" style="color: white; text-decoration: none; font-weight: {{ request()->routeIs('users.*') ? '600' : '400' }};">
                            Users
                        </a>
                    @endif
                    <a href="{{ route('alerts.index') }}" style="color: white; text-decoration: none; font-weight: {{ request()->routeIs('alerts.*') ? '600' : '400' }};">
                        Alerts
                    </a>
                    <a href="{{ route('profile.edit') }}" style="color: white; text-decoration: none; font-weight: {{ request()->routeIs('profile.*') ? '600' : '400' }};">
                        Profile
                    </a>
                </nav>
            </div>
            <div class="header-right">
                <span style="color: #ecf0f1; font-size: 14px;">{{ auth()->user()->name }} ({{ ucfirst(auth()->user()->role) }})</span>
                <form method="POST" action="{{ route('logout') }}">
                    @csrf
                    <button type="submit" class="btn btn-secondary" style="padding: 8px 16px; font-size: 14px;">
                        Logout
                    </button>
                </form>
            </div>
        </div>
    </div>

    <div class="container">
        @yield('content')
    </div>
</body>
</html>
