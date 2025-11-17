@extends('layouts.app')

@section('content')
<a href="{{ route('dashboard.index') }}" class="back-link">← Back to Dashboard</a>

<div class="card">
    <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px;">
        <h1 style="margin: 0;">Alert Subscriptions</h1>
        
        @if(auth()->user()->role === 'admin' && $users->count() > 1)
            <select id="userSelect" onchange="changeUser()" style="padding: 8px; border-radius: 4px; border: 1px solid #ddd;">
                @foreach($users as $u)
                    <option value="{{ $u->id }}" {{ $u->id == $targetUser->id ? 'selected' : '' }}>
                        {{ $u->name }} ({{ $u->role }})
                    </option>
                @endforeach
            </select>
        @endif
    </div>

    <p style="color: #666; margin-bottom: 20px;">
        Managing alerts for: <strong>{{ $targetUser->name }}</strong> 
        @if($targetUser->email)
            ({{ $targetUser->email }})
        @else
            <span style="color: #e74c3c;">(No email set - alerts won't be sent)</span>
        @endif
    </p>
</div>

<div id="statusMessage" style="display: none; padding: 15px; margin-bottom: 20px; border-radius: 5px;"></div>

<!-- Current Subscriptions -->
@if($subscriptions->count() > 0)
<div class="card">
    <h2 class="card-title">Active Subscriptions ({{ $subscriptions->count() }})</h2>
    <div style="overflow-x: auto;">
        <table style="width: 100%; border-collapse: collapse;">
            <thead>
                <tr style="background: #f5f5f5;">
                    <th style="padding: 12px; text-align: left;">Alert Type</th>
                    <th style="padding: 12px; text-align: left;">Device</th>
                    <th style="padding: 12px; text-align: left;">Settings</th>
                    <th style="padding: 12px; text-align: center;">Enabled</th>
                    <th style="padding: 12px; text-align: center;">Actions</th>
                </tr>
            </thead>
            <tbody>
                @foreach($subscriptions as $sub)
                <tr style="border-bottom: 1px solid #eee;">
                    <td style="padding: 12px;">{{ $sub->alert_type->label() }}</td>
                    <td style="padding: 12px;">{{ $sub->device?->name ?? 'All Devices' }}</td>
                    <td style="padding: 12px;">
                        @if($sub->settings)
                            <code style="font-size: 12px;">{{ json_encode($sub->settings) }}</code>
                        @else
                            <span style="color: #999;">No settings</span>
                        @endif
                    </td>
                    <td style="padding: 12px; text-align: center;">
                        <input type="checkbox" 
                               id="enabled-{{ $sub->id }}" 
                               {{ $sub->enabled ? 'checked' : '' }}
                               onchange="toggleEnabled({{ $sub->id }})">
                    </td>
                    <td style="padding: 12px; text-align: center;">
                        <button class="btn btn-danger" onclick="deleteSubscription({{ $sub->id }})" 
                                style="padding: 6px 12px; font-size: 13px; background: #e74c3c;">
                            Delete
                        </button>
                    </td>
                </tr>
                @endforeach
            </tbody>
        </table>
    </div>
</div>
@endif

<!-- Available Alert Types -->
<div class="card">
    <h2 class="card-title">Subscribe to New Alerts</h2>
    
    @foreach($alertTypes as $category => $types)
    <div style="margin-bottom: 30px;">
        <h3 style="font-size: 18px; margin-bottom: 15px; color: #2c3e50;">{{ $category }}</h3>
        <div style="display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); gap: 15px;">
            @foreach($types as $type)
            <div class="alert-type-card" style="border: 1px solid #ddd; border-radius: 8px; padding: 15px; background: #f9f9f9;">
                <div style="display: flex; justify-content: space-between; align-items: start; margin-bottom: 10px;">
                    <h4 style="margin: 0; font-size: 16px;">{{ $type['label'] }}</h4>
                    @if($type['requires_permission'] === 'user')
                        <span style="font-size: 11px; padding: 3px 8px; background: #3498db; color: white; border-radius: 3px;">USER+</span>
                    @endif
                </div>
                <p style="font-size: 14px; color: #666; margin-bottom: 15px;">{{ $type['description'] }}</p>
                
                @if($type['can_subscribe'])
                    <div style="margin-bottom: 10px;">
                        <label style="font-size: 13px; display: block; margin-bottom: 5px;">Device:</label>
                        <select class="device-select-{{ $type['value'] }}" style="width: 100%; padding: 6px; border-radius: 4px;">
                            <option value="">All Devices</option>
                            @foreach($devices as $device)
                                <option value="{{ $device->id }}">{{ $device->name }}</option>
                            @endforeach
                        </select>
                    </div>
                    
                    @if(in_array($type['value'], ['temp_high', 'temp_low']))
                        <div style="margin-bottom: 10px;">
                            <label style="font-size: 13px; display: block; margin-bottom: 5px;">Threshold (°C):</label>
                            <input type="number" class="threshold-{{ $type['value'] }}" 
                                   style="width: 100%; padding: 6px; border-radius: 4px;"
                                   placeholder="e.g., 25">
                        </div>
                    @endif
                    
                    <button class="btn" onclick="subscribe('{{ $type['value'] }}')" 
                            style="width: 100%; padding: 8px; font-size: 14px;">
                        Subscribe
                    </button>
                @else
                    <div style="padding: 8px; background: #fee; border-radius: 4px; font-size: 13px; color: #c33;">
                        Requires {{ $type['requires_permission'] }} role or higher
                    </div>
                @endif
            </div>
            @endforeach
        </div>
    </div>
    @endforeach
</div>

<script>
const targetUserId = {{ $targetUser->id }};

function changeUser() {
    const userId = document.getElementById('userSelect').value;
    window.location.href = `{{ route('alerts.index') }}?user_id=${userId}`;
}

function showMessage(message, isError = false) {
    const msgEl = document.getElementById('statusMessage');
    msgEl.textContent = message;
    msgEl.style.display = 'block';
    msgEl.style.backgroundColor = isError ? '#fee' : '#d4edda';
    msgEl.style.color = isError ? '#c33' : '#155724';
    msgEl.style.border = isError ? '1px solid #fcc' : '1px solid #c3e6cb';

    setTimeout(() => {
        msgEl.style.display = 'none';
    }, 5000);
}

async function subscribe(alertType) {
    const deviceId = document.querySelector(`.device-select-${alertType}`).value || null;
    const settings = {};

    // Get threshold if applicable
    const thresholdInput = document.querySelector(`.threshold-${alertType}`);
    if (thresholdInput && thresholdInput.value) {
        settings.threshold = parseFloat(thresholdInput.value);
    }

    try {
        const response = await fetch('/api/alert-subscriptions', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'X-CSRF-TOKEN': document.querySelector('meta[name="csrf-token"]').content
            },
            body: JSON.stringify({
                user_id: targetUserId,
                alert_type: alertType,
                device_id: deviceId,
                settings: Object.keys(settings).length > 0 ? settings : null
            })
        });

        const data = await response.json();

        if (response.ok) {
            showMessage('Subscription created successfully!');
            setTimeout(() => window.location.reload(), 1500);
        } else {
            showMessage(data.error || 'Failed to create subscription', true);
        }
    } catch (error) {
        showMessage('Error: ' + error.message, true);
    }
}

async function toggleEnabled(id) {
    const enabled = document.getElementById(`enabled-${id}`).checked;

    try {
        const response = await fetch(`/api/alert-subscriptions/${id}`, {
            method: 'PATCH',
            headers: {
                'Content-Type': 'application/json',
                'X-CSRF-TOKEN': document.querySelector('meta[name="csrf-token"]').content
            },
            body: JSON.stringify({ enabled })
        });

        if (response.ok) {
            showMessage(`Subscription ${enabled ? 'enabled' : 'disabled'}`);
        } else {
            showMessage('Failed to update subscription', true);
            document.getElementById(`enabled-${id}`).checked = !enabled;
        }
    } catch (error) {
        showMessage('Error: ' + error.message, true);
        document.getElementById(`enabled-${id}`).checked = !enabled;
    }
}

async function deleteSubscription(id) {
    if (!confirm('Are you sure you want to delete this subscription?')) {
        return;
    }

    try {
        const response = await fetch(`/api/alert-subscriptions/${id}`, {
            method: 'DELETE',
            headers: {
                'X-CSRF-TOKEN': document.querySelector('meta[name="csrf-token"]').content
            }
        });

        if (response.ok) {
            showMessage('Subscription deleted');
            setTimeout(() => window.location.reload(), 1000);
        } else {
            showMessage('Failed to delete subscription', true);
        }
    } catch (error) {
        showMessage('Error: ' + error.message, true);
    }
}
</script>
@endsection
