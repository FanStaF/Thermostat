<?php

namespace App\Console\Commands;

use App\Models\RelayState;
use Illuminate\Console\Command;
use Illuminate\Support\Facades\DB;

class DedupeRelayStates extends Command
{
    protected $signature = 'relay-states:dedupe {--dry-run : Count duplicates without deleting}';

    protected $description = 'Delete relay_states rows that match their immediate predecessor for the same relay (no-op duplicates).';

    public function handle(): int
    {
        $dryRun = (bool) $this->option('dry-run');

        // Window function over (relay_id, changed_at) pairs each row with the
        // previous row's columns; if every column matches, the row is a no-op
        // duplicate and can be deleted without losing any state-change history.
        // MySQL 8+ required.
        $sql = "
            SELECT id FROM (
                SELECT id,
                       state,    LAG(state)    OVER w AS prev_state,
                       mode,     LAG(mode)     OVER w AS prev_mode,
                       temp_on,  LAG(temp_on)  OVER w AS prev_temp_on,
                       temp_off, LAG(temp_off) OVER w AS prev_temp_off
                FROM relay_states
                WINDOW w AS (PARTITION BY relay_id ORDER BY changed_at, id)
            ) AS labelled
            WHERE state    = prev_state
              AND mode     = prev_mode
              AND temp_on  = prev_temp_on
              AND temp_off = prev_temp_off
        ";

        $duplicateIds = collect(DB::select($sql))->pluck('id')->all();
        $count = count($duplicateIds);

        $this->info(sprintf(
            'Found %d duplicate row(s)%s.',
            $count,
            $dryRun ? ' [DRY RUN]' : ''
        ));

        if ($count === 0 || $dryRun) {
            return self::SUCCESS;
        }

        $deleted = 0;
        foreach (array_chunk($duplicateIds, 1000) as $chunk) {
            $deleted += RelayState::whereIn('id', $chunk)->delete();
        }

        $summary = "Deleted {$deleted} duplicate relay_states row(s).";
        $this->info($summary);

        try {
            cache()->put('maintenance.dedupe_relay_states.last_run', now()->toIso8601String(), now()->addYear());
            cache()->put('maintenance.dedupe_relay_states.last_result', $summary, now()->addYear());
        } catch (\Throwable $e) {
            $this->warn('Could not write last-run cache: ' . $e->getMessage());
        }

        return self::SUCCESS;
    }
}
