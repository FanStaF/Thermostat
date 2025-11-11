@extends('layouts.app')

@section('content')
<a href="{{ route('dashboard.index') }}" class="back-link">← Back to Dashboard</a>

<div class="card">
    <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px;">
        <div>
            <div class="device-name">{{ $device->name }}</div>
            <span class="status {{ $device->is_online ? 'online' : 'offline' }}">
                {{ $device->is_online ? 'Online' : 'Offline' }}
            </span>
        </div>
        <div style="text-align: right; font-size: 14px; color: #666;">
            <div><strong>IP:</strong> {{ $device->ip_address ?? 'N/A' }}</div>
            <div><strong>Firmware:</strong> {{ $device->firmware_version ?? 'Unknown' }}</div>
            <div><strong>Last Seen:</strong> {{ $device->last_seen_at ? $device->last_seen_at->diffForHumans() : 'Never' }}</div>
        </div>
    </div>
</div>

<div class="card">
    <div class="card-title">Temperature History (Last 24 Hours)</div>
    <div class="chart-container">
        <canvas id="temperatureChart"></canvas>
    </div>
</div>

<div class="card">
    <div class="card-title">Relay Controls</div>
    <div class="relay-grid">
        @forelse($device->relays as $relay)
            @php
                $currentState = $relay->currentState->first();
            @endphp
            <div class="relay-card">
                <div class="relay-name">{{ $relay->name ?? "Relay {$relay->relay_number}" }}</div>

                @if($currentState)
                    <div class="relay-status">
                        <span>State:</span>
                        <span class="relay-badge {{ $currentState->state ? 'on' : 'off' }}">
                            {{ $currentState->state ? 'ON' : 'OFF' }}
                        </span>
                    </div>

                    <div class="relay-status">
                        <span>Mode:</span>
                        <span class="mode-badge">{{ $currentState->mode }}</span>
                    </div>

                    <div class="relay-status">
                        <span>Thresholds:</span>
                        <span style="font-size: 12px;">
                            ON: {{ $currentState->temp_on }}°C / OFF: {{ $currentState->temp_off }}°C
                        </span>
                    </div>
                @else
                    <p style="color: #999; font-size: 14px;">No state data available</p>
                @endif
            </div>
        @empty
            <p style="color: #666;">No relays configured yet.</p>
        @endforelse
    </div>
</div>

<div class="card">
    <div class="card-title">Device Settings</div>
    @if($device->settings)
        <div style="display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 15px;">
            <div>
                <strong>Update Frequency:</strong><br>
                {{ $device->settings->update_frequency }} seconds
            </div>
            <div>
                <strong>Temperature Unit:</strong><br>
                {{ $device->settings->use_fahrenheit ? 'Fahrenheit' : 'Celsius' }}
            </div>
            <div>
                <strong>Timezone:</strong><br>
                {{ $device->settings->timezone }}
            </div>
        </div>
    @else
        <p style="color: #666;">No settings configured.</p>
    @endif
</div>

<script src="https://cdn.jsdelivr.net/npm/chart.js@4.4.0/dist/chart.umd.min.js"></script>
<script src="https://cdn.jsdelivr.net/npm/chartjs-adapter-date-fns@3.0.0/dist/chartjs-adapter-date-fns.bundle.min.js"></script>
<script>
    document.addEventListener('DOMContentLoaded', function() {
        const ctx = document.getElementById('temperatureChart').getContext('2d');

        const readings = @json($readings);

        const chartData = {
            labels: readings.map(r => r.recorded_at),
            datasets: [{
                label: 'Temperature (°C)',
                data: readings.map(r => ({
                    x: r.recorded_at,
                    y: parseFloat(r.temperature)
                })),
                borderColor: 'rgb(52, 152, 219)',
                backgroundColor: 'rgba(52, 152, 219, 0.1)',
                tension: 0.4,
                fill: true
            }]
        };

        const config = {
            type: 'line',
            data: chartData,
            options: {
                responsive: true,
                maintainAspectRatio: false,
                plugins: {
                    legend: {
                        display: true,
                        position: 'top'
                    },
                    tooltip: {
                        mode: 'index',
                        intersect: false
                    }
                },
                scales: {
                    x: {
                        type: 'time',
                        time: {
                            unit: 'hour',
                            displayFormats: {
                                hour: 'MMM d, HH:mm'
                            }
                        },
                        title: {
                            display: true,
                            text: 'Time'
                        }
                    },
                    y: {
                        title: {
                            display: true,
                            text: 'Temperature (°C)'
                        }
                    }
                },
                interaction: {
                    mode: 'nearest',
                    axis: 'x',
                    intersect: false
                }
            }
        };

        new Chart(ctx, config);

        // Auto-refresh every 30 seconds
        setInterval(() => {
            window.location.reload();
        }, 30000);
    });
</script>
@endsection
