@extends('layouts.app')

@section('content')
<h1 style="margin-bottom: 20px;">Maintenance</h1>

@if(session('maintenance_output'))
    @php $out = session('maintenance_output'); @endphp
    <div class="card" style="border-left: 4px solid #27ae60;">
        <div class="card-title">Last run: {{ $out['job'] }}</div>
        <pre style="background: #2c3e50; color: #ecf0f1; padding: 15px; border-radius: 6px; overflow-x: auto; font-size: 13px; line-height: 1.5;">{{ $out['output'] }}</pre>
    </div>
@endif

<div class="card">
    <div class="card-title">Database snapshot</div>
    <table style="width: 100%; border-collapse: collapse;">
        <tbody>
            <tr><td style="padding: 6px 0; color: #666;">Devices</td><td style="padding: 6px 0; text-align: right; font-weight: 600;">{{ number_format($stats['devices']) }}</td></tr>
            <tr><td style="padding: 6px 0; color: #666;">Temperature readings (raw)</td><td style="padding: 6px 0; text-align: right; font-weight: 600;">{{ number_format($stats['temperature_readings_raw']) }}</td></tr>
            <tr><td style="padding: 6px 0; color: #666;">Temperature readings (hourly aggregates)</td><td style="padding: 6px 0; text-align: right; font-weight: 600;">{{ number_format($stats['temperature_readings_hourly']) }}</td></tr>
            <tr><td style="padding: 6px 0; color: #666;">Oldest raw reading</td><td style="padding: 6px 0; text-align: right; font-weight: 600;">{{ $stats['oldest_raw_reading'] ?: '—' }}</td></tr>
            <tr><td style="padding: 6px 0; color: #666;">Relay state changes</td><td style="padding: 6px 0; text-align: right; font-weight: 600;">{{ number_format($stats['relay_states']) }}</td></tr>
            <tr><td style="padding: 6px 0; color: #666;">Device commands (pending / total)</td><td style="padding: 6px 0; text-align: right; font-weight: 600;">{{ number_format($stats['device_commands_pending']) }} / {{ number_format($stats['device_commands_total']) }}</td></tr>
            <tr><td style="padding: 6px 0; color: #666;">Alert logs (unresolved / total)</td><td style="padding: 6px 0; text-align: right; font-weight: 600;">{{ number_format($stats['alert_logs_unresolved']) }} / {{ number_format($stats['alert_logs_total']) }}</td></tr>
            @if($stats['db_size_estimate'])
                <tr><td style="padding: 6px 0; color: #666;">Database size</td><td style="padding: 6px 0; text-align: right; font-weight: 600;">{{ $stats['db_size_estimate'] }}</td></tr>
            @endif
        </tbody>
    </table>
</div>

<div class="card">
    <div class="card-title">Downsample old temperature readings</div>
    <p style="margin-bottom: 15px; color: #666; font-size: 14px;">
        Aggregates rows older than the threshold into hourly buckets (avg/min/max) and deletes the raw rows.
        Scheduled to run daily at 03:15.
    </p>
    @if($lastRuns['downsample']['at'])
        <p style="margin-bottom: 15px; font-size: 13px; color: #27ae60;">
            Last run: {{ \Carbon\Carbon::parse($lastRuns['downsample']['at'])->diffForHumans() }} —
            {{ $lastRuns['downsample']['result'] }}
        </p>
    @else
        <p style="margin-bottom: 15px; font-size: 13px; color: #999;">Never run.</p>
    @endif
    <form method="POST" action="{{ route('maintenance.downsample') }}" style="display: flex; gap: 10px; align-items: center;">
        @csrf
        <label style="font-size: 14px; color: #333;">Keep raw for last <input type="number" name="days" value="30" min="1" max="365" style="width: 60px; padding: 4px 8px; border: 1px solid #ddd; border-radius: 4px;"> days</label>
        <button type="submit" class="btn">Run downsample</button>
    </form>
</div>

<div class="card">
    <div class="card-title">Prune old data</div>
    <p style="margin-bottom: 15px; color: #666; font-size: 14px;">
        Drops <code>relay_states</code>, completed <code>device_commands</code>, and resolved <code>alert_logs</code> beyond their retention windows.
        Scheduled to run daily at 03:30.
    </p>
    @if($lastRuns['prune']['at'])
        <p style="margin-bottom: 15px; font-size: 13px; color: #27ae60;">
            Last run: {{ \Carbon\Carbon::parse($lastRuns['prune']['at'])->diffForHumans() }} —
            {{ $lastRuns['prune']['result'] }}
        </p>
    @else
        <p style="margin-bottom: 15px; font-size: 13px; color: #999;">Never run.</p>
    @endif
    <form method="POST" action="{{ route('maintenance.prune') }}" style="display: grid; grid-template-columns: 1fr 1fr 1fr auto; gap: 10px; align-items: end;">
        @csrf
        <label style="font-size: 13px; color: #333;">Relay states (days)<br><input type="number" name="relay_days" value="365" min="1" style="width: 100%; padding: 4px 8px; border: 1px solid #ddd; border-radius: 4px;"></label>
        <label style="font-size: 13px; color: #333;">Commands (days)<br><input type="number" name="command_days" value="30" min="1" style="width: 100%; padding: 4px 8px; border: 1px solid #ddd; border-radius: 4px;"></label>
        <label style="font-size: 13px; color: #333;">Alert logs (days)<br><input type="number" name="alert_days" value="90" min="1" style="width: 100%; padding: 4px 8px; border: 1px solid #ddd; border-radius: 4px;"></label>
        <button type="submit" class="btn">Run prune</button>
    </form>
</div>

<div class="card">
    <div class="card-title">Dedupe relay state changes (one-shot)</div>
    <p style="margin-bottom: 15px; color: #666; font-size: 14px;">
        Removes <code>relay_states</code> rows that match their immediate predecessor for the same relay — historical no-ops from the firmware re-sending state on every boot and on every web-handler invocation. Safe to run repeatedly.
    </p>
    @if($lastRuns['dedupe_relay_states']['at'])
        <p style="margin-bottom: 15px; font-size: 13px; color: #27ae60;">
            Last run: {{ \Carbon\Carbon::parse($lastRuns['dedupe_relay_states']['at'])->diffForHumans() }} —
            {{ $lastRuns['dedupe_relay_states']['result'] }}
        </p>
    @else
        <p style="margin-bottom: 15px; font-size: 13px; color: #999;">Never run.</p>
    @endif
    <form method="POST" action="{{ route('maintenance.dedupe-relay-states') }}" style="display: flex; gap: 15px; align-items: center;">
        @csrf
        <label style="font-size: 14px; color: #333;">
            <input type="checkbox" name="dry_run" value="1" style="margin-right: 6px;">Dry run (count only)
        </label>
        <button type="submit" class="btn">Run dedupe</button>
    </form>
</div>
@endsection
