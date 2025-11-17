<?php

namespace App\Console\Commands;

use App\Jobs\SendAlertEmail;
use App\Models\AlertLog;
use App\Models\AlertSubscription;
use App\Services\AlertEvaluator;
use Illuminate\Console\Command;

class CheckAlerts extends Command
{
    protected $signature = 'alerts:check';
    protected $description = 'Evaluate all active alert subscriptions and trigger notifications';

    public function handle(AlertEvaluator $evaluator)
    {
        $this->info('Checking alerts...');

        $subscriptions = AlertSubscription::with(['user', 'device'])
            ->enabled()
            ->get();

        $triggeredCount = 0;
        $resolvedCount = 0;

        foreach ($subscriptions as $subscription) {
            // Skip if user has no email
            if (!$subscription->user->email) {
                $this->warn("Skipping subscription {$subscription->id} - user has no email");
                continue;
            }

            try {
                // Evaluate alert condition
                $result = $evaluator->evaluate($subscription);

                if ($result && $result['triggered']) {
                    // Check if we already sent this alert recently (cooldown)
                    $cooldownMinutes = $this->getCooldownMinutes($subscription->alert_type);

                    $recentAlert = AlertLog::where('alert_subscription_id', $subscription->id)
                        ->where('device_id', $result['device_id'])
                        ->where('triggered_at', '>', now()->subMinutes($cooldownMinutes))
                        ->whereNull('resolved_at')
                        ->first();

                    if (!$recentAlert) {
                        // Create alert log
                        $alertLog = AlertLog::create([
                            'alert_subscription_id' => $subscription->id,
                            'device_id' => $result['device_id'],
                            'triggered_at' => now(),
                            'message' => $result['message'],
                            'metadata' => $result['metadata'],
                        ]);

                        // Queue email
                        SendAlertEmail::dispatch($subscription->user, $alertLog);

                        $this->info("✓ Alert triggered: {$result['message']}");
                        $triggeredCount++;
                    }
                } else {
                    // Check if we should mark alert as resolved
                    $unresolvedAlerts = AlertLog::where('alert_subscription_id', $subscription->id)
                        ->whereNull('resolved_at')
                        ->get();

                    foreach ($unresolvedAlerts as $unresolvedAlert) {
                        $unresolvedAlert->update(['resolved_at' => now()]);
                        $this->info("✓ Alert resolved for subscription {$subscription->id}");
                        $resolvedCount++;
                    }
                }
            } catch (\Exception $e) {
                $this->error("Error evaluating subscription {$subscription->id}: {$e->getMessage()}");
            }
        }

        $this->info("\nSummary:");
        $this->info("- Checked {$subscriptions->count()} subscriptions");
        $this->info("- Triggered {$triggeredCount} new alerts");
        $this->info("- Resolved {$resolvedCount} alerts");

        return 0;
    }

    private function getCooldownMinutes($alertType): int
    {
        // Different alert types have different cooldown periods
        return match ($alertType->value) {
            'temp_high', 'temp_low' => 30,
            'temp_rapid_change' => 60,
            'device_offline', 'device_not_reporting' => 15,
            'device_online' => 5,
            'relay_state_changed', 'relay_mode_changed' => 10,
            'relay_stuck' => 120,
            'relay_cycling' => 60,
            default => 30,
        };
    }
}
