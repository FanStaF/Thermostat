<?php

namespace App;

enum AlertType: string
{
    // Temperature Alerts
    case TEMP_HIGH = 'temp_high';
    case TEMP_LOW = 'temp_low';
    case TEMP_RAPID_CHANGE = 'temp_rapid_change';
    case TEMP_SENSOR_OFFLINE = 'temp_sensor_offline';

    // Device Status Alerts
    case DEVICE_OFFLINE = 'device_offline';
    case DEVICE_ONLINE = 'device_online';
    case DEVICE_NOT_REPORTING = 'device_not_reporting';

    // Relay Alerts
    case RELAY_STATE_CHANGED = 'relay_state_changed';
    case RELAY_MODE_CHANGED = 'relay_mode_changed';
    case RELAY_STUCK = 'relay_stuck';
    case RELAY_CYCLING = 'relay_cycling';

    // Configuration Alerts
    case CONFIG_CHANGED = 'config_changed';
    case THRESHOLDS_CHANGED = 'thresholds_changed';

    // Summary Reports
    case DAILY_SUMMARY = 'daily_summary';
    case WEEKLY_SUMMARY = 'weekly_summary';

    public function label(): string
    {
        return match($this) {
            self::TEMP_HIGH => 'Temperature Above Threshold',
            self::TEMP_LOW => 'Temperature Below Threshold',
            self::TEMP_RAPID_CHANGE => 'Rapid Temperature Change',
            self::TEMP_SENSOR_OFFLINE => 'Temperature Sensor Offline',
            self::DEVICE_OFFLINE => 'Device Offline',
            self::DEVICE_ONLINE => 'Device Back Online',
            self::DEVICE_NOT_REPORTING => 'Device Not Reporting',
            self::RELAY_STATE_CHANGED => 'Relay State Changed',
            self::RELAY_MODE_CHANGED => 'Relay Mode Changed',
            self::RELAY_STUCK => 'Relay Stuck',
            self::RELAY_CYCLING => 'Relay Cycling Too Frequently',
            self::CONFIG_CHANGED => 'Configuration Changed',
            self::THRESHOLDS_CHANGED => 'Thresholds Modified',
            self::DAILY_SUMMARY => 'Daily Summary Report',
            self::WEEKLY_SUMMARY => 'Weekly Summary Report',
        };
    }

    public function description(): string
    {
        return match($this) {
            self::TEMP_HIGH => 'Get notified when temperature exceeds your threshold',
            self::TEMP_LOW => 'Get notified when temperature drops below your threshold',
            self::TEMP_RAPID_CHANGE => 'Alert when temperature changes ±5° in 5 minutes',
            self::TEMP_SENSOR_OFFLINE => 'Alert when temperature sensor stops reporting',
            self::DEVICE_OFFLINE => 'Alert when device disconnects',
            self::DEVICE_ONLINE => 'Alert when device reconnects',
            self::DEVICE_NOT_REPORTING => 'Alert when device hasn\'t reported in X minutes',
            self::RELAY_STATE_CHANGED => 'Notify when relay turns ON/OFF',
            self::RELAY_MODE_CHANGED => 'Notify when relay mode changes',
            self::RELAY_STUCK => 'Alert when relay should change but doesn\'t',
            self::RELAY_CYCLING => 'Alert when relay switches too frequently',
            self::CONFIG_CHANGED => 'Notify when device settings change',
            self::THRESHOLDS_CHANGED => 'Notify when relay thresholds are modified',
            self::DAILY_SUMMARY => 'Daily report of temperature and relay activity',
            self::WEEKLY_SUMMARY => 'Weekly summary of trends and statistics',
        };
    }

    public function category(): string
    {
        return match($this) {
            self::TEMP_HIGH, self::TEMP_LOW, self::TEMP_RAPID_CHANGE, self::TEMP_SENSOR_OFFLINE => 'Temperature',
            self::DEVICE_OFFLINE, self::DEVICE_ONLINE, self::DEVICE_NOT_REPORTING => 'Device Status',
            self::RELAY_STATE_CHANGED, self::RELAY_MODE_CHANGED, self::RELAY_STUCK, self::RELAY_CYCLING => 'Relays',
            self::CONFIG_CHANGED, self::THRESHOLDS_CHANGED => 'Configuration',
            self::DAILY_SUMMARY, self::WEEKLY_SUMMARY => 'Reports',
        };
    }

    public function requiresPermission(): string
    {
        return match($this) {
            self::RELAY_STATE_CHANGED, self::RELAY_MODE_CHANGED, self::RELAY_STUCK,
            self::RELAY_CYCLING, self::CONFIG_CHANGED, self::THRESHOLDS_CHANGED => 'user',
            default => 'viewer',
        };
    }
}
