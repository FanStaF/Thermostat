<x-mail::message>
# {{ $subscription->alert_type->label() }}

<x-mail::panel>
{{ $alertLog->message }}
</x-mail::panel>

## Device Information

@component('mail::table')
| | |
|:------------- |:-------------|
| **Device** | {{ $device->name ?? 'All devices' }} |
| **Triggered At** | {{ $alertLog->triggered_at->format('M j, Y g:i A') }} |
@if($alertLog->data && isset($alertLog->data['last_seen']))
| **Last Seen** | {{ $alertLog->data['last_seen'] }} |
@endif
@if($alertLog->data && isset($alertLog->data['online']))
| **Status** | {{ $alertLog->data['online'] === 'Yes' ? '🟢 Online' : '🔴 Offline' }} |
@endif
@endcomponent

@if($alertLog->data)
@php
    $hasTemperature = isset($alertLog->data['current_temperature']) || isset($alertLog->data['temperature']) || isset($alertLog->data['avg_temperature']);
    $hasRelayInfo = collect($alertLog->data)->keys()->filter(fn($k) => str_contains($k, 'relay_'))->isNotEmpty();
    $hasReport = isset($alertLog->data['avg_temperature']) || isset($alertLog->data['period']);
@endphp

@if($hasTemperature)
## Temperature Data

@component('mail::table')
| Metric | Value |
|:------------- |:-------------|
@if(isset($alertLog->data['current_temperature']))
| **Current** | {{ $alertLog->data['current_temperature'] }} |
@endif
@if(isset($alertLog->data['temperature']))
| **Reading** | {{ $alertLog->data['temperature'] }}°C |
@endif
@if(isset($alertLog->data['threshold']))
| **Threshold** | {{ $alertLog->data['threshold'] }}°C |
@endif
@if(isset($alertLog->data['avg_temperature']))
| **Average** | {{ $alertLog->data['avg_temperature'] }} |
@endif
@if(isset($alertLog->data['min_temperature']))
| **Minimum** | {{ $alertLog->data['min_temperature'] }} |
@endif
@if(isset($alertLog->data['max_temperature']))
| **Maximum** | {{ $alertLog->data['max_temperature'] }} |
@endif
@if(isset($alertLog->data['total_readings']))
| **Total Readings** | {{ $alertLog->data['total_readings'] }} |
@endif
@endcomponent
@endif

@if(isset($alertLog->data['temperature_chart']) && $alertLog->data['temperature_chart'])
### Temperature Chart
![Temperature History]({{ $alertLog->data['temperature_chart'] }})
@endif

@if($hasRelayInfo)
## Relay Status

@component('mail::table')
| Relay | State | Mode |
|:------------- |:-------------:|:-------------:|
@foreach($alertLog->data as $key => $value)
@if(str_starts_with($key, 'relay_') && str_ends_with($key, '_name'))
@php
    $relayBase = str_replace('_name', '', $key);
    $relayName = $value;
    $relayState = $alertLog->data[$relayBase . '_state'] ?? 'Unknown';
    $relayMode = $alertLog->data[$relayBase . '_mode'] ?? 'N/A';
    $stateIcon = $relayState === 'ON' ? '🟢' : ($relayState === 'OFF' ? '⚪' : '❓');
@endphp
| **{{ $relayName }}** | {{ $stateIcon }} {{ $relayState }} | {{ ucfirst($relayMode) }} |
@endif
@endforeach
@endcomponent
@endif

@if(isset($alertLog->data['relay_activity']) && is_array($alertLog->data['relay_activity']))
## Relay Activity

@component('mail::table')
| Relay | On Time |
|:------------- |:-------------|
@foreach($alertLog->data['relay_activity'] as $relayName => $onTime)
| **{{ ucfirst(str_replace('_', ' ', $relayName)) }}** | {{ $onTime }} |
@endforeach
@endcomponent
@endif
@endif

@if($device)
<x-mail::button :url="route('dashboard.show', $device->id)">
View Device Dashboard
</x-mail::button>
@endif

<x-mail::subcopy>
**Manage Alerts:** Visit your [alerts page]({{ route('alerts.index') }}) to configure notifications.
</x-mail::subcopy>

Thanks,<br>
{{ config('app.name') }}
</x-mail::message>
