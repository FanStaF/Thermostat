<?php

namespace App\Http\Controllers\Web;

use App\Http\Controllers\Controller;
use App\Models\AlertLog;
use App\Models\Device;
use App\Models\DeviceCommand;
use App\Models\RelayState;
use App\Models\TemperatureReading;
use App\Models\TemperatureReadingHourly;
use Illuminate\Http\Request;
use Illuminate\Support\Facades\Artisan;
use Illuminate\Support\Facades\DB;

class MaintenanceController extends Controller
{
    public function index()
    {
        $stats = [
            'devices'                       => Device::count(),
            'temperature_readings_raw'      => TemperatureReading::count(),
            'temperature_readings_hourly'   => TemperatureReadingHourly::count(),
            'oldest_raw_reading'            => TemperatureReading::min('recorded_at'),
            'relay_states'                  => RelayState::count(),
            'device_commands_total'         => DeviceCommand::count(),
            'device_commands_pending'       => DeviceCommand::where('status', 'pending')->count(),
            'alert_logs_total'              => AlertLog::count(),
            'alert_logs_unresolved'         => AlertLog::whereNull('resolved_at')->count(),
            'db_size_estimate'              => $this->estimateDbSize(),
        ];

        $lastRuns = [
            'downsample'           => $this->safeCacheRead('maintenance.downsample'),
            'prune'                => $this->safeCacheRead('maintenance.prune'),
            'dedupe_relay_states'  => $this->safeCacheRead('maintenance.dedupe_relay_states'),
        ];

        return view('maintenance.index', compact('stats', 'lastRuns'));
    }

    public function runDownsample(Request $request)
    {
        $output = $this->runArtisan('temperature:downsample', [
            '--days' => $request->input('days', 30),
        ]);

        return back()->with('maintenance_output', [
            'job'    => 'temperature:downsample',
            'output' => $output,
        ]);
    }

    public function runPrune(Request $request)
    {
        $output = $this->runArtisan('db:prune', [
            '--relay-states-days' => $request->input('relay_days', 365),
            '--commands-days'     => $request->input('command_days', 30),
            '--alert-logs-days'   => $request->input('alert_days', 90),
        ]);

        return back()->with('maintenance_output', [
            'job'    => 'db:prune',
            'output' => $output,
        ]);
    }

    public function runDedupeRelayStates(Request $request)
    {
        $output = $this->runArtisan('relay-states:dedupe', [
            '--dry-run' => $request->boolean('dry_run'),
        ]);

        return back()->with('maintenance_output', [
            'job'    => 'relay-states:dedupe',
            'output' => $output,
        ]);
    }

    private function safeCacheRead(string $prefix): array
    {
        try {
            return [
                'at'     => cache("{$prefix}.last_run"),
                'result' => cache("{$prefix}.last_result"),
            ];
        } catch (\Throwable) {
            return ['at' => null, 'result' => null];
        }
    }

    private function runArtisan(string $command, array $params = []): string
    {
        try {
            Artisan::call($command, $params);
            return Artisan::output();
        } catch (\Throwable $e) {
            return "ERROR: " . $e->getMessage();
        }
    }

    private function estimateDbSize(): ?string
    {
        if (DB::connection()->getDriverName() !== 'mysql') {
            return null;
        }

        $row = DB::selectOne('
            SELECT ROUND(SUM(data_length + index_length) / 1024 / 1024, 1) AS mb
            FROM information_schema.tables
            WHERE table_schema = DATABASE()
        ');

        return $row && $row->mb !== null ? $row->mb . ' MB' : null;
    }
}
