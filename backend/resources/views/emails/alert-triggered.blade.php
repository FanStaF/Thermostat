<x-mail::message>
# Alert: {{ $subscription->alert_type->label() }}

{{ $alertLog->message }}

**Device:** {{ $device->name ?? 'All devices' }}
**Time:** {{ $alertLog->triggered_at->format('M j, Y g:i A') }}

@if($alertLog->metadata)
## Details

@foreach($alertLog->metadata as $key => $value)
- **{{ ucfirst(str_replace('_', ' ', $key)) }}:** {{ is_string($value) || is_numeric($value) ? $value : json_encode($value) }}
@endforeach
@endif

@if($device)
<x-mail::button :url="route('dashboard.show', $device->id)">
View Device
</x-mail::button>
@endif

<x-mail::panel>
To manage your alert subscriptions, visit your [alerts page]({{ route('alerts.index') }}).
</x-mail::panel>

Thanks,<br>
{{ config('app.name') }}
</x-mail::message>
