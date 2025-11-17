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
                    'metadata' => [
                        'temperature' => $reading->temperature,
                        'threshold' => $threshold,
                        'device_name' => $device->name,
                        'recorded_at' => $reading->recorded_at->toIso8601String(),
                    ],
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
                    'metadata' => [
                        'temperature' => $reading->temperature,
                        'threshold' => $threshold,
                        'device_name' => $device->name,
                        'recorded_at' => $reading->recorded_at->toIso8601String(),
                    ],
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
                $currentState = $relay->currentState;

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

    private function getDevicesToCheck(AlertSubscription $sub): \Illuminate\Database\Eloquent\Collection
    {
        return $sub->device_id
            ? Device::with(['relays.currentState', 'temperatureReadings'])
                ->where('id', $sub->device_id)
                ->get()
            : Device::with(['relays.currentState', 'temperatureReadings'])->get();
    }
}
