@extends('layouts.app')

@section('content')
<a href="{{ route('dashboard.index') }}" class="back-link">← Back to Dashboard</a>

<div class="card">
    <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px;">
        <div style="flex: 1;">
            <div style="display: flex; align-items: center; gap: 10px; margin-bottom: 10px;">
                <input type="text" id="deviceName" value="{{ $device->name }}"
                       style="font-size: 20px; font-weight: 600; padding: 5px 10px; border: 1px solid #ddd; border-radius: 4px; flex: 0 0 300px;">
                <button class="btn" style="padding: 5px 15px; font-size: 14px;" onclick="updateDeviceName()">
                    Save Name
                </button>
            </div>
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
    <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 15px;">
        <div class="card-title" style="margin: 0;">Temperature History</div>
        <div style="display: flex; gap: 5px;">
            <button class="range-btn {{ $range == '1h' ? 'active' : '' }}" onclick="changeRange('1h')">1H</button>
            <button class="range-btn {{ $range == '6h' ? 'active' : '' }}" onclick="changeRange('6h')">6H</button>
            <button class="range-btn {{ $range == '24h' ? 'active' : '' }}" onclick="changeRange('24h')">24H</button>
            <button class="range-btn {{ $range == '7d' ? 'active' : '' }}" onclick="changeRange('7d')">7D</button>
            <button class="range-btn {{ $range == '30d' ? 'active' : '' }}" onclick="changeRange('30d')">30D</button>
            <button class="range-btn {{ $range == 'all' ? 'active' : '' }}" onclick="changeRange('all')">ALL</button>
        </div>
    </div>
    <div class="chart-container">
        <canvas id="temperatureChart"></canvas>
    </div>
    <div style="margin-top: 10px; text-align: center; font-size: 13px; color: #666;">
        Showing {{ $readings->count() }} readings
    </div>
</div>

<div class="card">
    <div class="card-title">Relay Controls</div>
    <div id="statusMessage" style="display: none; padding: 10px; margin-bottom: 15px; border-radius: 5px;"></div>
    <div class="relay-grid">
        @forelse($device->relays as $relay)
            @php
                $currentState = $relay->currentState->first();
            @endphp
            <div class="relay-card" data-relay-id="{{ $relay->id }}" data-relay-number="{{ $relay->relay_number }}">
                <div style="display: flex; align-items: center; gap: 8px; margin-bottom: 10px;">
                    <input type="text" id="relayName-{{ $relay->relay_number }}"
                           value="{{ $relay->name ?? "Relay {$relay->relay_number}" }}"
                           style="flex: 1; padding: 5px 8px; border: 1px solid #ddd; border-radius: 3px; font-weight: 600;">
                    <button class="btn" style="padding: 4px 10px; font-size: 11px;"
                            onclick="updateRelayName({{ $relay->relay_number }})">
                        Save
                    </button>
                </div>

                @if($currentState)
                    <div class="relay-status">
                        <span>State:</span>
                        <span class="relay-badge {{ $currentState->state ? 'on' : 'off' }}" id="state-{{ $relay->relay_number }}">
                            {{ $currentState->state ? 'ON' : 'OFF' }}
                        </span>
                    </div>

                    <div style="margin: 10px 0;">
                        <strong>Mode:</strong>
                        <div style="display: flex; gap: 5px; margin-top: 5px;">
                            <button class="mode-btn {{ $currentState->mode == 'AUTO' ? 'active' : '' }}"
                                    onclick="setRelayMode({{ $relay->relay_number }}, 'AUTO')"
                                    data-mode="AUTO">AUTO</button>
                            <button class="mode-btn {{ $currentState->mode == 'MANUAL_ON' ? 'active' : '' }}"
                                    onclick="setRelayMode({{ $relay->relay_number }}, 'MANUAL_ON')"
                                    data-mode="MANUAL_ON">ON</button>
                            <button class="mode-btn {{ $currentState->mode == 'MANUAL_OFF' ? 'active' : '' }}"
                                    onclick="setRelayMode({{ $relay->relay_number }}, 'MANUAL_OFF')"
                                    data-mode="MANUAL_OFF">OFF</button>
                        </div>
                    </div>

                    <div style="margin: 10px 0;">
                        <strong>Thresholds:</strong>
                        <div style="margin-top: 5px;">
                            <label style="font-size: 12px; display: block; margin-bottom: 3px;">ON Temp (°C):</label>
                            <input type="number" step="0.5" class="threshold-input"
                                   id="tempOn-{{ $relay->relay_number }}"
                                   value="{{ $currentState->temp_on }}"
                                   style="width: 100%; padding: 5px; border: 1px solid #ddd; border-radius: 3px;">
                        </div>
                        <div style="margin-top: 5px;">
                            <label style="font-size: 12px; display: block; margin-bottom: 3px;">OFF Temp (°C):</label>
                            <input type="number" step="0.5" class="threshold-input"
                                   id="tempOff-{{ $relay->relay_number }}"
                                   value="{{ $currentState->temp_off }}"
                                   style="width: 100%; padding: 5px; border: 1px solid #ddd; border-radius: 3px;">
                        </div>
                        <button class="btn" style="width: 100%; margin-top: 8px; padding: 8px; font-size: 13px;"
                                onclick="setThresholds({{ $relay->relay_number }})">
                            Update Thresholds
                        </button>
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
                <label style="font-weight: 600; display: block; margin-bottom: 5px;">Update Frequency:</label>
                <input type="number" id="updateFrequency" value="{{ $device->settings->update_frequency }}"
                       min="1" max="60"
                       style="width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px;">
                <small style="color: #666;">seconds</small>
            </div>
            <div>
                <label style="font-weight: 600; display: block; margin-bottom: 5px;">Temperature Unit:</label>
                <select id="useFahrenheit"
                        style="width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px;">
                    <option value="0" {{ !$device->settings->use_fahrenheit ? 'selected' : '' }}>Celsius</option>
                    <option value="1" {{ $device->settings->use_fahrenheit ? 'selected' : '' }}>Fahrenheit</option>
                </select>
            </div>
            <div>
                <label style="font-weight: 600; display: block; margin-bottom: 5px;">Timezone:</label>
                <input type="text" id="timezone" value="{{ $device->settings->timezone }}"
                       style="width: 100%; padding: 8px; border: 1px solid #ddd; border-radius: 4px;">
            </div>
        </div>
        <button class="btn" style="margin-top: 15px;" onclick="updateSettings()">
            Save Settings
        </button>
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
        const dataCount = readings.length;

        // Adjust display based on data density
        const showPoints = dataCount < 100;
        const pointRadius = dataCount < 20 ? 4 : (dataCount < 50 ? 3 : 0);
        const tension = dataCount < 50 ? 0.1 : 0.4;

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
                tension: tension,
                fill: true,
                pointRadius: pointRadius,
                pointHoverRadius: pointRadius + 2,
                pointBackgroundColor: 'rgb(52, 152, 219)',
                pointBorderColor: '#fff',
                pointBorderWidth: 2
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
                        intersect: false,
                        callbacks: {
                            label: function(context) {
                                return context.dataset.label + ': ' + context.parsed.y.toFixed(1) + '°C';
                            }
                        }
                    }
                },
                scales: {
                    x: {
                        type: 'time',
                        time: {
                            @if($range == '1h' || $range == '6h')
                                unit: 'minute',
                                stepSize: {{ $range == '1h' ? 10 : 60 }},
                                displayFormats: {
                                    minute: 'HH:mm'
                                }
                            @elseif($range == '24h')
                                unit: 'hour',
                                stepSize: 3,
                                displayFormats: {
                                    hour: 'HH:mm'
                                }
                            @elseif($range == '7d')
                                unit: 'day',
                                displayFormats: {
                                    day: 'MMM d'
                                }
                            @elseif($range == '30d')
                                unit: 'day',
                                stepSize: 5,
                                displayFormats: {
                                    day: 'MMM d'
                                }
                            @else
                                unit: 'week',
                                displayFormats: {
                                    week: 'MMM d'
                                }
                            @endif
                        },
                        ticks: {
                            autoSkip: false,
                            maxRotation: 45,
                            minRotation: 0,
                            font: { size: 11 },
                            color: '#666'
                        },
                        title: {
                            display: true,
                            text: 'Time',
                            font: { size: 12, weight: 'bold' }
                        },
                        grid: {
                            display: true,
                            color: 'rgba(0, 0, 0, 0.08)'
                        }
                    },
                    y: {
                        title: {
                            display: true,
                            text: 'Temperature (°C)'
                        },
                        ticks: {
                            callback: function(value) {
                                return value.toFixed(1);
                            }
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

    // Command sending functions
    const deviceId = {{ $device->id }};

    function showMessage(message, isError = false) {
        const msgEl = document.getElementById('statusMessage');
        msgEl.textContent = message;
        msgEl.style.display = 'block';
        msgEl.style.backgroundColor = isError ? '#f8d7da' : '#d4edda';
        msgEl.style.color = isError ? '#721c24' : '#155724';
        msgEl.style.border = isError ? '1px solid #f5c6cb' : '1px solid #c3e6cb';

        setTimeout(() => {
            msgEl.style.display = 'none';
        }, 5000);
    }

    function sendCommand(type, params) {
        fetch(`/devices/${deviceId}/commands`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'X-CSRF-TOKEN': document.querySelector('meta[name="csrf-token"]').content
            },
            body: JSON.stringify({ type, params })
        })
        .then(response => {
            if (!response.ok) {
                return response.text().then(text => {
                    throw new Error(`HTTP ${response.status}: ${text.substring(0, 100)}`);
                });
            }
            return response.json();
        })
        .then(data => {
            showMessage('Command sent successfully! Device will apply changes shortly.');
        })
        .catch(error => {
            showMessage('Error sending command: ' + error.message, true);
        });
    }

    function setRelayMode(relayNumber, mode) {
        sendCommand('set_relay_mode', {
            relay_number: relayNumber,
            mode: mode
        });

        // Update UI immediately
        const card = document.querySelector(`[data-relay-number="${relayNumber}"]`);
        card.querySelectorAll('.mode-btn').forEach(btn => {
            btn.classList.remove('active');
            if (btn.dataset.mode === mode) {
                btn.classList.add('active');
            }
        });
    }

    function setThresholds(relayNumber) {
        const tempOn = parseFloat(document.getElementById(`tempOn-${relayNumber}`).value);
        const tempOff = parseFloat(document.getElementById(`tempOff-${relayNumber}`).value);

        if (isNaN(tempOn) || isNaN(tempOff)) {
            showMessage('Please enter valid temperature values', true);
            return;
        }

        if (tempOn <= tempOff) {
            showMessage('ON temperature must be higher than OFF temperature', true);
            return;
        }

        sendCommand('set_thresholds', {
            relay_number: relayNumber,
            temp_on: tempOn,
            temp_off: tempOff
        });
    }

    function updateSettings() {
        const updateFrequency = parseInt(document.getElementById('updateFrequency').value);
        const useFahrenheit = document.getElementById('useFahrenheit').value === '1';

        if (isNaN(updateFrequency) || updateFrequency < 1 || updateFrequency > 60) {
            showMessage('Update frequency must be between 1 and 60 seconds', true);
            return;
        }

        sendCommand('set_frequency', {
            frequency: updateFrequency
        });

        if (useFahrenheit !== {{ $device->settings->use_fahrenheit ? 'true' : 'false' }}) {
            sendCommand('set_unit', {
                use_fahrenheit: useFahrenheit
            });
        }
    }

    function updateDeviceName() {
        const newName = document.getElementById('deviceName').value.trim();

        if (!newName) {
            showMessage('Device name cannot be empty', true);
            return;
        }

        fetch(`/devices/${deviceId}`, {
            method: 'PATCH',
            headers: {
                'Content-Type': 'application/json',
                'X-CSRF-TOKEN': document.querySelector('meta[name="csrf-token"]').content
            },
            body: JSON.stringify({ name: newName })
        })
        .then(response => response.json())
        .then(data => {
            showMessage('Device name updated successfully!');
        })
        .catch(error => {
            showMessage('Error updating device name: ' + error.message, true);
        });
    }

    function updateRelayName(relayNumber) {
        const newName = document.getElementById(`relayName-${relayNumber}`).value.trim();

        if (!newName) {
            showMessage('Relay name cannot be empty', true);
            return;
        }

        // Find relay ID from the card
        const card = document.querySelector(`[data-relay-number="${relayNumber}"]`);
        const relayId = card.dataset.relayId;

        fetch(`/devices/${deviceId}/relays/${relayId}`, {
            method: 'PATCH',
            headers: {
                'Content-Type': 'application/json',
                'X-CSRF-TOKEN': document.querySelector('meta[name="csrf-token"]').content
            },
            body: JSON.stringify({ name: newName })
        })
        .then(response => response.json())
        .then(data => {
            showMessage('Relay name updated successfully!');
        })
        .catch(error => {
            showMessage('Error updating relay name: ' + error.message, true);
        });
    }

    function changeRange(range) {
        window.location.href = `{{ route('dashboard.show', $device->id) }}?range=${range}`;
    }
</script>
@endsection
