<?php

namespace App\Console\Commands;

use App\Models\AlertLog;
use App\Models\DeviceCommand;
use App\Models\RelayState;
use Illuminate\Console\Command;

class PruneOldData extends Command
{
    protected $signature = 'db:prune
                            {--relay-states-days=365 : Drop relay_states older than this many days}
                            {--commands-days=30 : Drop completed/failed device_commands older than this many days}
                            {--alert-logs-days=90 : Drop resolved alert_logs older than this many days}
                            {--dry-run : Count what would be deleted without deleting}';

    protected $description = 'Delete old rows from relay_states, device_commands, and alert_logs.';

    public function handle(): int
    {
        $dryRun = (bool) $this->option('dry-run');
        $relayDays = (int) $this->option('relay-states-days');
        $cmdDays = (int) $this->option('commands-days');
        $alertDays = (int) $this->option('alert-logs-days');

        $relayCutoff = now()->subDays($relayDays);
        $cmdCutoff = now()->subDays($cmdDays);
        $alertCutoff = now()->subDays($alertDays);

        $this->info('Pruning' . ($dryRun ? ' [DRY RUN]' : ''));

        $relayQ = RelayState::where('changed_at', '<', $relayCutoff);
        $cmdQ = DeviceCommand::whereIn('status', ['completed', 'failed'])
            ->where('created_at', '<', $cmdCutoff);
        $alertQ = AlertLog::whereNotNull('resolved_at')
            ->where('resolved_at', '<', $alertCutoff);

        $relayCount = (clone $relayQ)->count();
        $cmdCount = (clone $cmdQ)->count();
        $alertCount = (clone $alertQ)->count();

        $this->table(
            ['Table', 'Cutoff', 'Rows'],
            [
                ['relay_states (>1y)',         $relayCutoff->toDateTimeString(), $relayCount],
                ['device_commands (>30d done)', $cmdCutoff->toDateTimeString(),   $cmdCount],
                ['alert_logs (>90d resolved)',  $alertCutoff->toDateTimeString(), $alertCount],
            ]
        );

        if ($dryRun) {
            return self::SUCCESS;
        }

        $relayDeleted = $relayQ->delete();
        $cmdDeleted = $cmdQ->delete();
        $alertDeleted = $alertQ->delete();

        $summary = sprintf(
            'Deleted: %d relay_states, %d device_commands, %d alert_logs',
            $relayDeleted, $cmdDeleted, $alertDeleted
        );
        $this->info($summary);

        try {
            cache()->put('maintenance.prune.last_run', now()->toIso8601String(), now()->addYear());
            cache()->put('maintenance.prune.last_result', $summary, now()->addYear());
        } catch (\Throwable $e) {
            $this->warn('Could not write last-run cache: ' . $e->getMessage());
        }

        return self::SUCCESS;
    }
}
