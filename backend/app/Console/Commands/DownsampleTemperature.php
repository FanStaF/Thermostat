<?php

namespace App\Console\Commands;

use App\Models\TemperatureReading;
use App\Models\TemperatureReadingHourly;
use Carbon\CarbonImmutable;
use Illuminate\Console\Command;
use Illuminate\Support\Facades\DB;

class DownsampleTemperature extends Command
{
    protected $signature = 'temperature:downsample
                            {--days=30 : Keep this many days of raw readings; older rows are downsampled and deleted}
                            {--dry-run : Compute what would happen without writing}';

    protected $description = 'Aggregate raw temperature_readings older than --days into hourly buckets, then delete the raw rows.';

    public function handle(): int
    {
        $keepDays = (int) $this->option('days');
        $dryRun = (bool) $this->option('dry-run');
        $cutoff = CarbonImmutable::now()->subDays($keepDays)->startOfHour();

        $this->info(sprintf(
            "Downsampling readings older than %s (%d days)%s",
            $cutoff->toDateTimeString(),
            $keepDays,
            $dryRun ? ' [DRY RUN]' : ''
        ));

        // Group raw rows older than the cutoff into per-hour buckets.
        // We do this per (device_id, sensor_id, hour) so the query stays small
        // and the writes can run in short transactions.
        $buckets = TemperatureReading::query()
            ->selectRaw('
                device_id,
                sensor_id,
                DATE_FORMAT(recorded_at, "%Y-%m-%d %H:00:00") as bucket_start,
                AVG(temperature) as avg_temp,
                MIN(temperature) as min_temp,
                MAX(temperature) as max_temp,
                COUNT(*) as sample_count
            ')
            ->where('recorded_at', '<', $cutoff)
            ->groupBy('device_id', 'sensor_id', DB::raw('DATE_FORMAT(recorded_at, "%Y-%m-%d %H:00:00")'))
            ->orderBy('device_id')
            ->orderBy('bucket_start')
            ->get();

        if ($buckets->isEmpty()) {
            $this->info('Nothing to downsample.');
            return self::SUCCESS;
        }

        $this->info(sprintf('Found %d hourly buckets to write.', $buckets->count()));

        if ($dryRun) {
            $totalSamples = $buckets->sum('sample_count');
            $this->info(sprintf('Would aggregate %d raw rows into %d hourly rows.', $totalSamples, $buckets->count()));
            return self::SUCCESS;
        }

        $writtenBuckets = 0;
        $deletedRaw = 0;

        // Process per device/day to keep transactions short and locks bounded.
        $grouped = $buckets->groupBy(fn ($b) => $b->device_id . '|' . substr($b->bucket_start, 0, 10));

        foreach ($grouped as $key => $group) {
            [$deviceId, $day] = explode('|', $key);
            $dayStart = CarbonImmutable::parse($day)->startOfDay();
            $dayEnd = $dayStart->endOfDay();

            DB::transaction(function () use ($group, $deviceId, $dayStart, $dayEnd, $cutoff, &$writtenBuckets, &$deletedRaw) {
                // Upsert hourly rows. If we re-run for the same day (e.g. after a
                // partial failure) we recompute from current raw data.
                $rows = $group->map(fn ($b) => [
                    'device_id'    => $b->device_id,
                    'sensor_id'    => $b->sensor_id,
                    'bucket_start' => $b->bucket_start,
                    'avg_temp'     => round($b->avg_temp, 2),
                    'min_temp'     => round($b->min_temp, 2),
                    'max_temp'     => round($b->max_temp, 2),
                    'sample_count' => $b->sample_count,
                ])->all();

                TemperatureReadingHourly::upsert(
                    $rows,
                    ['device_id', 'sensor_id', 'bucket_start'],
                    ['avg_temp', 'min_temp', 'max_temp', 'sample_count']
                );
                $writtenBuckets += count($rows);

                // Delete the raw rows we just summarized. Constrain by day window
                // so we don't accidentally remove rows newer than the cutoff if the
                // cutoff lands mid-day (it won't with startOfHour, but defensive).
                $deletedRaw += TemperatureReading::query()
                    ->where('device_id', $deviceId)
                    ->whereBetween('recorded_at', [$dayStart, min($dayEnd, $cutoff->subSecond())])
                    ->delete();
            });
        }

        $this->info(sprintf(
            'Wrote %d hourly buckets; deleted %d raw rows.',
            $writtenBuckets,
            $deletedRaw
        ));

        $this->recordRun(sprintf(
            'Wrote %d hourly buckets; deleted %d raw rows.',
            $writtenBuckets,
            $deletedRaw
        ));

        return self::SUCCESS;
    }

    /** Best-effort write to the maintenance-UI cache; never fails the command. */
    private function recordRun(string $result): void
    {
        try {
            cache()->put('maintenance.downsample.last_run', now()->toIso8601String(), now()->addYear());
            cache()->put('maintenance.downsample.last_result', $result, now()->addYear());
        } catch (\Throwable $e) {
            $this->warn('Could not write last-run cache: ' . $e->getMessage());
        }
    }
}
