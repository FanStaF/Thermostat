@extends('layouts.app')

@section('content')
<div class="card">
    <div class="card-title">Your Devices</div>

    @if($devices->isEmpty())
        <p style="color: #666; text-align: center; padding: 40px 0;">
            No devices registered yet. Please register your ESP8266 device to get started.
        </p>
    @else
        <div class="device-grid">
            @foreach($devices as $device)
                <div class="device-card">
                    <div class="device-name">{{ $device->name }}</div>

                    <div class="device-info">
                        <div>
                            <span class="status {{ $device->is_online ? 'online' : 'offline' }}">
                                {{ $device->is_online ? 'Online' : 'Offline' }}
                            </span>
                        </div>

                        <div><strong>Hostname:</strong> {{ $device->hostname }}</div>
                        <div><strong>IP:</strong> {{ $device->ip_address ?? 'N/A' }}</div>
                        <div><strong>Firmware:</strong> {{ $device->firmware_version ?? 'Unknown' }}</div>
                        <div><strong>Readings:</strong> {{ $device->temperature_readings_count }}</div>
                        <div><strong>Last Seen:</strong> {{ $device->last_seen_at ? $device->last_seen_at->diffForHumans() : 'Never' }}</div>
                    </div>

                    <div style="margin-top: 15px;">
                        <a href="{{ route('dashboard.show', $device->id) }}" class="btn">
                            View Details
                        </a>
                    </div>
                </div>
            @endforeach
        </div>
    @endif
</div>

<div class="card">
    <div class="card-title">API Information</div>
    <p style="color: #666; margin-bottom: 10px;">
        To connect your ESP8266 device, use the following API endpoint:
    </p>
    <code style="background: #f8f9fa; padding: 10px; display: block; border-radius: 4px;">
        POST {{ url('/api/devices/register') }}
    </code>
</div>
@endsection
