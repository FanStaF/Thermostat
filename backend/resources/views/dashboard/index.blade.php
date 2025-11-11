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
                    <div style="display: flex; justify-content: space-between; align-items: start; margin-bottom: 10px;">
                        <div class="device-name">{{ $device->name }}</div>
                        <span class="status {{ $device->is_online ? 'online' : 'offline' }}">
                            {{ $device->is_online ? 'Online' : 'Offline' }}
                        </span>
                    </div>

                    @if($device->latest_temp)
                        <div style="background: #e3f2fd; padding: 15px; border-radius: 6px; margin-bottom: 15px; text-align: center;">
                            <div style="font-size: 36px; font-weight: 700; color: #1976d2;">
                                {{ number_format($device->latest_temp->temperature, 1) }}°C
                            </div>
                            <div style="font-size: 12px; color: #666; margin-top: 5px;">
                                {{ $device->latest_temp->recorded_at->diffForHumans() }}
                            </div>
                        </div>
                    @else
                        <div style="background: #f5f5f5; padding: 15px; border-radius: 6px; margin-bottom: 15px; text-align: center; color: #999;">
                            No temperature data
                        </div>
                    @endif

                    @if($device->relays->isNotEmpty())
                        <div style="margin-bottom: 15px;">
                            <strong style="display: block; margin-bottom: 8px; font-size: 13px;">Relays:</strong>
                            <div style="display: grid; grid-template-columns: repeat(2, 1fr); gap: 6px;">
                                @foreach($device->relays as $relay)
                                    @php $state = $relay->currentState->first(); @endphp
                                    <div style="background: #f8f9fa; padding: 8px; border-radius: 4px; text-align: center; border: 2px solid {{ $state && $state->state ? '#d4edda' : '#e9ecef' }};">
                                        <div style="font-size: 11px; color: #666;">R{{ $relay->relay_number }}</div>
                                        <div style="font-size: 12px; font-weight: 600; color: {{ $state && $state->state ? '#155724' : '#666' }};">
                                            {{ $state ? ($state->state ? 'ON' : 'OFF') : 'N/A' }}
                                        </div>
                                        <div style="font-size: 10px; color: #999;">
                                            {{ $state ? $state->mode : 'N/A' }}
                                        </div>
                                    </div>
                                @endforeach
                            </div>
                        </div>
                    @endif

                    <div style="display: flex; gap: 8px; margin-top: 15px;">
                        <a href="{{ route('dashboard.show', $device->id) }}" class="btn" style="flex: 1; text-align: center;">
                            Full Control
                        </a>
                    </div>

                    <div style="margin-top: 10px; padding-top: 10px; border-top: 1px solid #e9ecef; font-size: 12px; color: #999;">
                        <div>{{ $device->ip_address ?? 'No IP' }} • {{ $device->temperature_readings_count }} readings</div>
                        <div>Last seen: {{ $device->last_seen_at ? $device->last_seen_at->diffForHumans() : 'Never' }}</div>
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

<script>
    // Auto-refresh every 15 seconds for live updates
    setInterval(() => {
        window.location.reload();
    }, 15000);
</script>
@endsection
