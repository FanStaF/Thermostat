@extends('layouts.app')

@section('content')
<a href="{{ route('dashboard.index') }}" class="back-link">← Back to Dashboard</a>

<div class="card">
    <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px;">
        <div>
            <h1 style="margin: 0 0 10px 0;">Alert Subscriptions</h1>
            <p style="color: #666; margin: 0;">
                Managing alerts for: <strong>{{ $targetUser->name }}</strong>
                @if($targetUser->email)
                    ({{ $targetUser->email }})
                @else
                    <span style="color: #e74c3c; font-weight: bold;">⚠ No email - alerts won't be sent</span>
                @endif
            </p>
        </div>
        
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
</div>

<div id="statusMessage" style="display: none; padding: 15px; margin-bottom: 20px; border-radius: 5px;"></div>

@php
    $subscriptionMap = $subscriptions->keyBy(function($sub) {
        return $sub->alert_type->value . '_' . ($sub->device_id ?? 'all');
    });
@endphp

@foreach($alertTypes as $category => $types)
<div class="card">
    <h2 class="card-title">{{ $category }} Alerts</h2>
    <div style="overflow-x: auto;">
        <table style="width: 100%; border-collapse: collapse;">
            <thead>
                <tr style="background: #f5f5f5;">
                    <th style="padding: 12px; text-align: left; width: 200px;">Alert Type</th>
                    <th style="padding: 12px; text-align: left;">Description</th>
                    <th style="padding: 12px; text-align: center; width: 120px;">Device</th>
                    <th style="padding: 12px; text-align: center; width: 100px;">Settings</th>
                    <th style="padding: 12px; text-align: center; width: 100px;">Cooldown (min)</th>
                    <th style="padding: 12px; text-align: center; width: 100px;">Schedule</th>
                    <th style="padding: 12px; text-align: center; width: 100px;">Status</th>
                    <th style="padding: 12px; text-align: center; width: 100px;">Action</th>
                </tr>
            </thead>
            <tbody>
                @foreach($types as $type)
                @php
                    $subscription = $subscriptionMap->first(function($sub) use ($type) {
                        return $sub->alert_type->value === $type['value'];
                    });
                    $isSubscribed = $subscription !== null;
                @endphp
                <tr style="border-bottom: 1px solid #eee; {{ $isSubscribed ? 'background: #f0f8ff;' : '' }}">
                    <td style="padding: 12px;">
                        <strong>{{ $type['label'] }}</strong>
                    </td>
                    <td style="padding: 12px; color: #666; font-size: 14px;">
                        {{ $type['description'] }}
                    </td>
                    <td style="padding: 12px; text-align: center;">
                        @if($isSubscribed)
                            {{ $subscription->device?->name ?? 'All' }}
                        @else
                            @if($type['can_subscribe'])
                                <select id="device-{{ $type['value'] }}" style="width: 100%; padding: 4px; font-size: 12px;">
                                    <option value="">All</option>
                                    @foreach($devices as $device)
                                        <option value="{{ $device->id }}">{{ $device->name }}</option>
                                    @endforeach
                                </select>
                            @else
                                -
                            @endif
                        @endif
                    </td>
                    <td style="padding: 12px; text-align: center;">
                        @if($isSubscribed)
                            @if($subscription->settings)
                                <code style="font-size: 11px;">{{ json_encode($subscription->settings) }}</code>
                            @else
                                -
                            @endif
                        @else
                            @if($type['can_subscribe'] && in_array($type['value'], ['temp_high', 'temp_low']))
                                <input type="number" id="threshold-{{ $type['value'] }}" 
                                       placeholder="°C" style="width: 60px; padding: 4px; font-size: 12px;">
                            @else
                                -
                            @endif
                        @endif
                    </td>
                    <td style="padding: 12px; text-align: center;">
                        @if($isSubscribed)
                            {{ $subscription->cooldown_minutes }}
                        @else
                            @if($type['can_subscribe'])
                                <input type="number" id="cooldown-{{ $type['value'] }}"
                                       value="30" style="width: 60px; padding: 4px; font-size: 12px;">
                            @else
                                -
                            @endif
                        @endif
                    </td>
                    <td style="padding: 12px; text-align: center;">
                        @if($isSubscribed)
                            {{ $subscription->scheduled_time ?? '-' }}
                        @else
                            @if($type['can_subscribe'] && in_array($type['value'], ['daily_summary', 'weekly_summary']))
                                <input type="time" id="schedule-{{ $type['value'] }}"
                                       value="09:00" style="width: 100px; padding: 4px; font-size: 12px;">
                            @else
                                -
                            @endif
                        @endif
                    </td>
                    <td style="padding: 12px; text-align: center;">
                        @if($isSubscribed)
                            <label style="cursor: pointer;">
                                <input type="checkbox" id="enabled-{{ $subscription->id }}" 
                                       {{ $subscription->enabled ? 'checked' : '' }}
                                       onchange="toggleEnabled({{ $subscription->id }})">
                                <span style="font-size: 12px; margin-left: 4px;">
                                    {{ $subscription->enabled ? 'On' : 'Off' }}
                                </span>
                            </label>
                        @else
                            @if(!$type['can_subscribe'])
                                <span style="color: #e74c3c; font-size: 12px;">No access</span>
                            @else
                                -
                            @endif
                        @endif
                    </td>
                    <td style="padding: 12px; text-align: center;">
                        @if($isSubscribed)
                            <div style="display: flex; gap: 5px; justify-content: center;">
                                @if(auth()->user()->isAdmin())
                                    <button class="btn" onclick="testAlert({{ $subscription->id }})"
                                            style="padding: 4px 8px; font-size: 12px; background: #3498db; color: white;">
                                        Test
                                    </button>
                                @endif
                                <button class="btn btn-danger" onclick="deleteSubscription({{ $subscription->id }})"
                                        style="padding: 4px 8px; font-size: 12px; background: #e74c3c;">
                                    Remove
                                </button>
                            </div>
                        @else
                            @if($type['can_subscribe'])
                                <button class="btn" onclick="subscribe('{{ $type['value'] }}')"
                                        style="padding: 4px 8px; font-size: 12px; width: 80px;">
                                    Subscribe
                                </button>
                            @else
                                -
                            @endif
                        @endif
                    </td>
                </tr>
                @endforeach
            </tbody>
        </table>
    </div>
</div>
@endforeach

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
    const deviceSelect = document.getElementById(`device-${alertType}`);
    const deviceId = deviceSelect ? (deviceSelect.value || null) : null;
    const settings = {};

    // Get threshold if applicable
    const thresholdInput = document.getElementById(`threshold-${alertType}`);
    if (thresholdInput && thresholdInput.value) {
        settings.threshold = parseFloat(thresholdInput.value);
    }

    // Get cooldown
    const cooldownInput = document.getElementById(`cooldown-${alertType}`);
    const cooldownMinutes = cooldownInput ? parseInt(cooldownInput.value) : 30;

    // Get scheduled time if applicable
    const scheduleInput = document.getElementById(`schedule-${alertType}`);
    const scheduledTime = scheduleInput ? scheduleInput.value : null;

    try {
        const response = await fetch('/alert-subscriptions', {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Accept': 'application/json',
                'X-CSRF-TOKEN': document.querySelector('meta[name="csrf-token"]').content
            },
            body: JSON.stringify({
                user_id: targetUserId,
                alert_type: alertType,
                device_id: deviceId,
                settings: Object.keys(settings).length > 0 ? settings : null,
                cooldown_minutes: cooldownMinutes,
                scheduled_time: scheduledTime
            })
        });

        const data = await response.json();

        if (response.ok) {
            showMessage('Subscription created successfully!');
            setTimeout(() => window.location.reload(), 1000);
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
        const response = await fetch(`/alert-subscriptions/${id}`, {
            method: 'PATCH',
            headers: {
                'Content-Type': 'application/json',
                'Accept': 'application/json',
                'X-CSRF-TOKEN': document.querySelector('meta[name="csrf-token"]').content
            },
            body: JSON.stringify({ enabled })
        });

        if (response.ok) {
            showMessage(`Alert ${enabled ? 'enabled' : 'disabled'}`);
        } else {
            showMessage('Failed to update', true);
            document.getElementById(`enabled-${id}`).checked = !enabled;
        }
    } catch (error) {
        showMessage('Error: ' + error.message, true);
        document.getElementById(`enabled-${id}`).checked = !enabled;
    }
}

async function deleteSubscription(id) {
    if (!confirm('Remove this alert subscription?')) {
        return;
    }

    try {
        const response = await fetch(`/alert-subscriptions/${id}`, {
            method: 'DELETE',
            headers: {
                'Accept': 'application/json',
                'X-CSRF-TOKEN': document.querySelector('meta[name="csrf-token"]').content
            }
        });

        if (response.ok) {
            showMessage('Subscription removed');
            setTimeout(() => window.location.reload(), 1000);
        } else {
            showMessage('Failed to remove subscription', true);
        }
    } catch (error) {
        showMessage('Error: ' + error.message, true);
    }
}

async function testAlert(id) {
    if (!confirm('Send a test alert email for this subscription?')) {
        return;
    }

    showMessage('Sending test alert...', false);

    try {
        const response = await fetch(`/alert-subscriptions/${id}/test`, {
            method: 'POST',
            headers: {
                'Content-Type': 'application/json',
                'Accept': 'application/json',
                'X-CSRF-TOKEN': document.querySelector('meta[name="csrf-token"]').content
            }
        });

        const data = await response.json();

        if (response.ok) {
            showMessage(`Test alert sent to ${data.email_sent_to}`, false);
        } else {
            showMessage(data.error || 'Failed to send test alert', true);
        }
    } catch (error) {
        showMessage('Error: ' + error.message, true);
    }
}
</script>
@endsection
