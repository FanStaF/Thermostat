<?php

namespace App\Services;

use App\AlertType;
use App\Models\AlertSubscription;
use App\Models\Device;
use App\Models\RelayStateHistory;

class AlertEvaluator
{
    public function evaluate(AlertSubscription $subscription): ?array
    {
        return match ($subscription->alert_type) {
            AlertType::TEMP_HIGH => $this->evaluateTempHigh($subscription),
            AlertType::TEMP_LOW => $this->evaluateTempLow($subscription),
            AlertType::TEMP_RAPID_CHANGE => $this->evaluateTempRapidChange($subscription),
            AlertType::TEMP_SENSOR_OFFLINE => $this->evaluateTempSensorOffline($subscription),
            AlertType::DEVICE_OFFLINE => $this->evaluateDeviceOffline($subscription),
            AlertType::DEVICE_ONLINE => $this->evaluateDeviceOnline($subscription),
            AlertType::DEVICE_NOT_REPORTING => $this->evaluateDeviceNotReporting($subscription),
            AlertType::RELAY_STATE_CHANGED => $this->evaluateRelayStateChanged($subscription),
            AlertType::RELAY_MODE_CHANGED => $this->evaluateRelayModeChanged($subscription),
            AlertType::RELAY_STUCK => $this->evaluateRelayStuck($subscription),
            AlertType::RELAY_CYCLING => $this->evaluateRelayCycling($subscription),
            AlertType::DAILY_SUMMARY => $this->evaluateDailySummary($subscription),
            AlertType::WEEKLY_SUMMARY => $this->evaluateWeeklySummary($subscription),
            default => null,
        };
    }

    private function evaluateTempHigh(AlertSubscription $sub): ?array
    {
        $threshold = $sub->settings['threshold'] ?? 30;

        $devices = $this->getDevicesToCheck($sub);

        foreach ($devices as $device) {
            $reading = $device->temperatureReadings()
                ->latest('recorded_at')
                ->first();

            if ($reading && $reading->temperature > $threshold) {
                return [
                    'triggered' => true,
                    'device_id' => $device->id,
                    'message' => "Temperature {$reading->temperature}°C exceeds threshold {$threshold}°C on {$device->name}",
                    'metadata' => array_merge([
                        'temperature' => $reading->temperature,
                        'threshold' => $threshold,
                        'device_name' => $device->name,
                        'recorded_at' => $reading->recorded_at->format('M j, Y g:i A'),
                    ], $this->getDeviceState($device)),
                ];
            }
        }

        return null;
    }

    private function evaluateTempLow(AlertSubscription $sub): ?array
    {
        $threshold = $sub->settings['threshold'] ?? 15;

        $devices = $this->getDevicesToCheck($sub);

        foreach ($devices as $device) {
            $reading = $device->temperatureReadings()
                ->latest('recorded_at')
                ->first();

            if ($reading && $reading->temperature < $threshold) {
                return [
                    'triggered' => true,
                    'device_id' => $device->id,
                    'message' => "Temperature {$reading->temperature}°C is below threshold {$threshold}°C on {$device->name}",
                    'metadata' => array_merge([
                        'temperature' => $reading->temperature,
                        'threshold' => $threshold,
                        'device_name' => $device->name,
                        'recorded_at' => $reading->recorded_at->format('M j, Y g:i A'),
                    ], $this->getDeviceState($device)),
                ];
            }
        }

        return null;
    }

    private function evaluateTempRapidChange(AlertSubscription $sub): ?array
    {
        $changeThreshold = $sub->settings['threshold'] ?? 5; // degrees in 10 minutes

        $devices = $this->getDevicesToCheck($sub);

        foreach ($devices as $device) {
            $readings = $device->temperatureReadings()
                ->where('recorded_at', '>', now()->subMinutes(10))
                ->orderBy('recorded_at')
                ->get();

            if ($readings->count() >= 2) {
                $firstTemp = $readings->first()->temperature;
                $lastTemp = $readings->last()->temperature;
                $change = abs($lastTemp - $firstTemp);

                if ($change > $changeThreshold) {
                    return [
                        'triggered' => true,
                        'device_id' => $device->id,
                        'message' => "Temperature changed {$change}°C in 10 minutes on {$device->name}",
                        'metadata' => [
                            'change' => $change,
                            'threshold' => $changeThreshold,
                            'from_temp' => $firstTemp,
                            'to_temp' => $lastTemp,
                            'device_name' => $device->name,
                        ],
                    ];
                }
            }
        }

        return null;
    }

    private function evaluateTempSensorOffline(AlertSubscription $sub): ?array
    {
        $devices = $this->getDevicesToCheck($sub);

        foreach ($devices as $device) {
            $lastReading = $device->temperatureReadings()
                ->latest('recorded_at')
                ->first();

            if (!$lastReading || $lastReading->recorded_at < now()->subMinutes(10)) {
                $minutesSince = $lastReading
                    ? now()->diffInMinutes($lastReading->recorded_at)
                    : 999;

                return [
                    'triggered' => true,
                    'device_id' => $device->id,
                    'message' => "No temperature readings from {$device->name} for {$minutesSince} minutes",
                    'metadata' => [
                        'device_name' => $device->name,
                        'minutes_since_reading' => $minutesSince,
                        'last_reading' => $lastReading?->recorded_at?->toIso8601String(),
                    ],
                ];
            }
        }

        return null;
    }

    private function evaluateDeviceOffline(AlertSubscription $sub): ?array
    {
        $devices = $this->getDevicesToCheck($sub);

        foreach ($devices as $device) {
            $minutesSinceLastSeen = now()->diffInMinutes($device->last_seen);

            if ($minutesSinceLastSeen > 5) {
                return [
                    'triggered' => true,
                    'device_id' => $device->id,
                    'message' => "{$device->name} has been offline for {$minutesSinceLastSeen} minutes",
                    'metadata' => [
                        'device_name' => $device->name,
                        'last_seen' => $device->last_seen->toIso8601String(),
                        'minutes_offline' => $minutesSinceLastSeen,
                    ],
                ];
            }
        }

        return null;
    }

    private function evaluateDeviceOnline(AlertSubscription $sub): ?array
    {
        // This alert triggers when a previously offline device comes back online
        // We'll check this differently - looking at recent heartbeats after a gap
        $devices = $this->getDevicesToCheck($sub);

        foreach ($devices as $device) {
            $minutesSinceLastSeen = now()->diffInMinutes($device->last_seen);

            // Device is online (within last 2 minutes)
            if ($minutesSinceLastSeen <= 2) {
                // Check if device was offline before (no readings between 5-15 minutes ago)
                $wasOffline = !$device->temperatureReadings()
                    ->whereBetween('recorded_at', [now()->subMinutes(15), now()->subMinutes(5)])
                    ->exists();

                if ($wasOffline) {
                    return [
                        'triggered' => true,
                        'device_id' => $device->id,
                        'message' => "{$device->name} is back online",
                        'metadata' => [
                            'device_name' => $device->name,
                            'last_seen' => $device->last_seen->toIso8601String(),
                        ],
                    ];
                }
            }
        }

        return null;
    }

    private function evaluateDeviceNotReporting(AlertSubscription $sub): ?array
    {
        // Similar to offline but more lenient (15+ minutes)
        $devices = $this->getDevicesToCheck($sub);

        foreach ($devices as $device) {
            $minutesSinceLastSeen = now()->diffInMinutes($device->last_seen);

            if ($minutesSinceLastSeen > 15) {
                return [
                    'triggered' => true,
                    'device_id' => $device->id,
                    'message' => "{$device->name} has not reported in {$minutesSinceLastSeen} minutes",
                    'metadata' => [
                        'device_name' => $device->name,
                        'last_seen' => $device->last_seen->toIso8601String(),
                        'minutes_since_report' => $minutesSinceLastSeen,
                    ],
                ];
            }
        }

        return null;
    }

    private function evaluateRelayStateChanged(AlertSubscription $sub): ?array
    {
        $devices = $this->getDevicesToCheck($sub);

        foreach ($devices as $device) {
            foreach ($device->relays as $relay) {
                $recentChange = RelayStateHistory::where('relay_id', $relay->id)
                    ->where('changed_at', '>', now()->subMinutes(2))
                    ->latest('changed_at')
                    ->first();

                if ($recentChange) {
                    return [
                        'triggered' => true,
                        'device_id' => $device->id,
                        'message' => "{$relay->name} on {$device->name} changed to " . ($recentChange->state ? 'ON' : 'OFF'),
                        'metadata' => [
                            'device_name' => $device->name,
                            'relay_name' => $relay->name,
                            'state' => $recentChange->state ? 'ON' : 'OFF',
                            'changed_at' => $recentChange->changed_at->toIso8601String(),
                        ],
                    ];
                }
            }
        }

        return null;
    }

    private function evaluateRelayModeChanged(AlertSubscription $sub): ?array
    {
        // Check for mode changes in the last 2 minutes
        $devices = $this->getDevicesToCheck($sub);

        foreach ($devices as $device) {
            foreach ($device->relays as $relay) {
                // We'd need a relay_mode_history table for this, or check updated_at on relays table
                // For now, we'll skip detailed implementation
                // This would be similar to relay state changed but tracking mode field
            }
        }

        return null;
    }

    private function evaluateRelayStuck(AlertSubscription $sub): ?array
    {
        // Check if relay has been in same state for unusually long (e.g., heating on for 2+ hours)
        $devices = $this->getDevicesToCheck($sub);

        foreach ($devices as $device) {
            foreach ($device->relays as $relay) {
                $currentState = $relay->currentState()->first();

                if (!$currentState) {
                    continue;
                }

                $hoursSinceLastChange = RelayStateHistory::where('relay_id', $relay->id)
                    ->where('state', '!=', $currentState->state)
                    ->latest('changed_at')
                    ->first();

                if (!$hoursSinceLastChange) {
                    continue;
                }

                $hours = now()->diffInHours($hoursSinceLastChange->changed_at);

                if ($hours >= 3) {
                    return [
                        'triggered' => true,
                        'device_id' => $device->id,
                        'message' => "{$relay->name} on {$device->name} has been " . ($currentState->state ? 'ON' : 'OFF') . " for {$hours} hours",
                        'metadata' => [
                            'device_name' => $device->name,
                            'relay_name' => $relay->name,
                            'state' => $currentState->state ? 'ON' : 'OFF',
                            'hours_in_state' => $hours,
                        ],
                    ];
                }
            }
        }

        return null;
    }

    private function evaluateRelayCycling(AlertSubscription $sub): ?array
    {
        // Check if relay is cycling too frequently (e.g., 10+ state changes in 30 minutes)
        $devices = $this->getDevicesToCheck($sub);

        foreach ($devices as $device) {
            foreach ($device->relays as $relay) {
                $recentChanges = RelayStateHistory::where('relay_id', $relay->id)
                    ->where('changed_at', '>', now()->subMinutes(30))
                    ->count();

                if ($recentChanges >= 10) {
                    return [
                        'triggered' => true,
                        'device_id' => $device->id,
                        'message' => "{$relay->name} on {$device->name} cycled {$recentChanges} times in 30 minutes",
                        'metadata' => [
                            'device_name' => $device->name,
                            'relay_name' => $relay->name,
                            'cycle_count' => $recentChanges,
                            'time_period' => '30 minutes',
                        ],
                    ];
                }
            }
        }

        return null;
    }

    private function evaluateDailySummary(AlertSubscription $sub): ?array
    {
        // Check if it's the scheduled time (default 09:00)
        $scheduledTime = $sub->scheduled_time ?? '09:00';
        $now = now();

        // Only trigger within 5 minutes of scheduled time
        if ($now->format('H:i') < $scheduledTime || $now->format('H:i') > date('H:i', strtotime($scheduledTime) + 300)) {
            return null;
        }

        $devices = $this->getDevicesToCheck($sub);

        foreach ($devices as $device) {
            // Get yesterday's data
            $stats = $this->calculateDailyStats($device);

            if (!$stats) {
                continue;
            }

            return [
                'triggered' => true,
                'device_id' => $device->id,
                'message' => "Daily Summary for {$device->name}",
                'metadata' => $stats,
            ];
        }

        return null;
    }

    private function evaluateWeeklySummary(AlertSubscription $sub): ?array
    {
        // Check if it's the scheduled time and Monday
        $scheduledTime = $sub->scheduled_time ?? '09:00';
        $now = now();

        if ($now->dayOfWeek !== 1) { // Not Monday
            return null;
        }

        // Only trigger within 5 minutes of scheduled time
        if ($now->format('H:i') < $scheduledTime || $now->format('H:i') > date('H:i', strtotime($scheduledTime) + 300)) {
            return null;
        }

        $devices = $this->getDevicesToCheck($sub);

        foreach ($devices as $device) {
            // Get last week's data
            $stats = $this->calculateWeeklyStats($device);

            if (!$stats) {
                continue;
            }

            return [
                'triggered' => true,
                'device_id' => $device->id,
                'message' => "Weekly Summary for {$device->name}",
                'metadata' => $stats,
            ];
        }

        return null;
    }

    private function calculateDailyStats(Device $device): ?array
    {
        $yesterday = now()->subDay();

        $readings = $device->temperatureReadings()
            ->whereDate('recorded_at', $yesterday->toDateString())
            ->get();

        if ($readings->isEmpty()) {
            return null;
        }

        $temps = $readings->pluck('temperature');

        // Calculate relay on time
        $relayStats = [];
        foreach ($device->relays as $relay) {
            $onTime = RelayStateHistory::where('relay_id', $relay->id)
                ->whereDate('changed_at', $yesterday->toDateString())
                ->where('state', true)
                ->count();

            $relayStats[$relay->name] = $this->formatDuration($onTime * 60); // Assuming 1 entry per minute avg
        }

        return [
            'device_name' => $device->name,
            'date' => $yesterday->format('M j, Y'),
            'avg_temperature' => round($temps->avg(), 1) . '°C',
            'min_temperature' => round($temps->min(), 1) . '°C',
            'max_temperature' => round($temps->max(), 1) . '°C',
            'readings_count' => $readings->count(),
            'relay_activity' => $relayStats,
        ];
    }

    private function calculateWeeklyStats(Device $device): ?array
    {
        $lastWeek = now()->subWeek();
        $startOfWeek = $lastWeek->startOfWeek();
        $endOfWeek = $lastWeek->endOfWeek();

        $readings = $device->temperatureReadings()
            ->whereBetween('recorded_at', [$startOfWeek, $endOfWeek])
            ->get();

        if ($readings->isEmpty()) {
            return null;
        }

        $temps = $readings->pluck('temperature');

        // Calculate relay on time
        $relayStats = [];
        foreach ($device->relays as $relay) {
            $onTime = RelayStateHistory::where('relay_id', $relay->id)
                ->whereBetween('changed_at', [$startOfWeek, $endOfWeek])
                ->where('state', true)
                ->count();

            $relayStats[$relay->name] = $this->formatDuration($onTime * 60);
        }

        return [
            'device_name' => $device->name,
            'period' => $startOfWeek->format('M j') . ' - ' . $endOfWeek->format('M j, Y'),
            'avg_temperature' => round($temps->avg(), 1) . '°C',
            'min_temperature' => round($temps->min(), 1) . '°C',
            'max_temperature' => round($temps->max(), 1) . '°C',
            'total_readings' => $readings->count(),
            'relay_activity' => $relayStats,
        ];
    }

    private function formatDuration(int $seconds): string
    {
        $hours = floor($seconds / 3600);
        $minutes = floor(($seconds % 3600) / 60);

        return sprintf('%dh %dm', $hours, $minutes);
    }

    private function getDevicesToCheck(AlertSubscription $sub): \Illuminate\Database\Eloquent\Collection
    {
        return $sub->device_id
            ? Device::with(['relays.currentState', 'temperatureReadings'])
                ->where('id', $sub->device_id)
                ->get()
            : Device::with(['relays.currentState', 'temperatureReadings'])->get();
    }

    private function getDeviceState(Device $device): array
    {
        $state = [
            'last_seen' => $device->last_seen ? $device->last_seen->format('M j, Y g:i A') : 'Never',
            'online' => $device->last_seen && $device->last_seen > now()->subMinutes(5),
        ];

        // Get current temperature
        $latestReading = $device->temperatureReadings()->latest('recorded_at')->first();
        if ($latestReading) {
            $state['current_temperature'] = round($latestReading->temperature, 1) . '°C';
        }

        // Get relay states
        foreach ($device->relays as $relay) {
            $currentState = $relay->currentState()->first();
            $state['relay_' . $relay->id . '_name'] = $relay->name;
            $state['relay_' . $relay->id . '_state'] = $currentState ? ($currentState->state ? 'ON' : 'OFF') : 'Unknown';
            $state['relay_' . $relay->id . '_mode'] = $relay->mode ?? 'manual';
        }

        return $state;
    }
}
