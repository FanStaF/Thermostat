<x-mail::message>
# Alert: {{ $subscription->alert_type->label() }}

{{ $alertLog->message }}

**Device:** {{ $device->name ?? 'All devices' }}
**Time:** {{ $alertLog->triggered_at->format('M j, Y g:i A') }}

@if($alertLog->data)
## Details

@if(isset($alertLog->data['temperature_chart']) && $alertLog->data['temperature_chart'])
![Temperature Chart]({{ $alertLog->data['temperature_chart'] }})
@endif

@foreach($alertLog->data as $key => $value)
@if($key === 'temperature_chart')
@continue
@endif
@if(is_array($value))
**{{ ucfirst(str_replace('_', ' ', $key)) }}:**
@foreach($value as $subKey => $subValue)
  - {{ ucfirst(str_replace('_', ' ', $subKey)) }}: {{ $subValue }}
@endforeach
@else
- **{{ ucfirst(str_replace('_', ' ', $key)) }}:** {{ $value }}
@endif
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
