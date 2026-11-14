<?php

namespace Tests\Feature;

use App\Models\Device;
use App\Models\DeviceCommand;
use App\Models\TemperatureReading;
use App\Models\User;
use Illuminate\Foundation\Testing\RefreshDatabase;
use Tests\TestCase;

class DeviceCommunicationTest extends TestCase
{
    use RefreshDatabase;

    private Device $device;
    private string $token;

    protected function setUp(): void
    {
        parent::setUp();
        $this->device = Device::create([
            'name' => 'thermostat',
            'hostname' => 'thermostat',
            'mac_address' => 'AA:BB:CC:DD:EE:FF',
            'last_seen_at' => now()->subMinutes(10),
        ]);
        $this->device->settings()->create(['update_frequency' => 60, 'use_fahrenheit' => false, 'timezone' => 'UTC']);
        $this->token = $this->device->createToken('device-token')->plainTextToken;
    }

    private function batch(array $readings)
    {
        return $this->withToken($this->token)
            ->postJson("/api/devices/{$this->device->id}/temperature/batch", ['readings' => $readings]);
    }

    // ---- batch upload with device timestamps ----

    public function test_batch_stores_readings_with_device_timestamps(): void
    {
        $t1 = now()->subHours(3)->timestamp;
        $t2 = now()->subHours(2)->timestamp;

        $this->batch([
            ['temperature' => 20.5, 'sensor_id' => 0, 'recorded_at' => $t1],
            ['temperature' => 21.0, 'sensor_id' => 0, 'recorded_at' => $t2],
        ])->assertCreated()->assertJson(['stored' => 2, 'duplicates' => 0, 'rejected' => 0]);

        $stored = TemperatureReading::orderBy('recorded_at')->pluck('recorded_at')->map->timestamp->all();
        $this->assertSame([$t1, $t2], $stored);
        $this->assertTrue($this->device->refresh()->is_online);
    }

    public function test_retried_batch_does_not_duplicate(): void
    {
        $readings = [['temperature' => 20.5, 'recorded_at' => now()->subMinute()->timestamp]];

        $this->batch($readings)->assertJson(['stored' => 1]);
        $this->batch($readings)->assertJson(['stored' => 0, 'duplicates' => 1]);

        $this->assertSame(1, TemperatureReading::count());
    }

    public function test_reading_without_timestamp_uses_receipt_time(): void
    {
        $this->batch([['temperature' => 19.0]])->assertJson(['stored' => 1]);

        $this->assertEqualsWithDelta(now()->timestamp, TemperatureReading::first()->recorded_at->timestamp, 5);
    }

    public function test_out_of_window_timestamps_are_rejected(): void
    {
        $this->batch([
            ['temperature' => 20, 'recorded_at' => now()->subDays(5)->timestamp],
            ['temperature' => 20, 'recorded_at' => now()->addHour()->timestamp],
            ['temperature' => 20, 'recorded_at' => now()->timestamp],
        ])->assertJson(['stored' => 1, 'rejected' => 2]);
    }

    public function test_invalid_batch_is_422(): void
    {
        $this->batch([['temperature' => 999]])->assertStatus(422);
        $this->batch(array_fill(0, 101, ['temperature' => 20]))->assertStatus(422);
    }

    public function test_other_devices_token_cannot_post_batch(): void
    {
        $other = Device::create(['name' => 'x', 'hostname' => 'x', 'mac_address' => '11:11:11:11:11:11']);
        $otherToken = $other->createToken('t')->plainTextToken;

        $this->withToken($otherToken)
            ->postJson("/api/devices/{$this->device->id}/temperature/batch", ['readings' => [['temperature' => 20]]])
            ->assertForbidden();
    }

    // ---- pending commands signalled in replies ----

    public function test_replies_report_pending_commands(): void
    {
        $this->batch([['temperature' => 20]])->assertJson(['pending_commands' => 0]);
        $this->withToken($this->token)->postJson("/api/devices/{$this->device->id}/heartbeat", [])
            ->assertJson(['pending_commands' => 0]);

        DeviceCommand::create(['device_id' => $this->device->id, 'type' => 'restart', 'params' => [], 'status' => 'pending']);

        $this->withToken($this->token)->postJson("/api/devices/{$this->device->id}/heartbeat", [])
            ->assertJson(['pending_commands' => 1]);
        $this->batch([['temperature' => 20]])->assertJson(['pending_commands' => 1]);
    }

    // ---- optional HTTPS-only device API ----

    public function test_plain_http_rejected_only_when_enabled(): void
    {
        $this->withToken($this->token)->postJson("/api/devices/{$this->device->id}/heartbeat", [])->assertOk();

        config(['app.api_require_https' => true]);
        $this->withToken($this->token)->postJson("/api/devices/{$this->device->id}/heartbeat", [])->assertForbidden();
        $this->withToken($this->token)->withHeader('X-Forwarded-Proto', 'https')
            ->postJson("/api/devices/{$this->device->id}/heartbeat", [])->assertOk();
    }

    // ---- dashboard live refresh ----

    public function test_dashboard_data_version_changes_only_with_new_data(): void
    {
        $admin = User::factory()->create(['role' => 'admin']);
        $url = "/devices/{$this->device->id}/dashboard-data";

        $first = $this->actingAs($admin)->getJson($url)->assertOk()
            ->assertJson(['updateFrequency' => 60])->json();

        // A heartbeat moves last-seen but is not "new data".
        $this->travel(2)->seconds();
        $this->app['auth']->forgetGuards();
        $this->withToken($this->token)->postJson("/api/devices/{$this->device->id}/heartbeat", []);
        $this->app['auth']->forgetGuards();
        $afterHeartbeat = $this->actingAs($admin)->getJson($url)->json();
        $this->assertSame($first['version'], $afterHeartbeat['version']);
        $this->assertNotSame($first['lastSeenAt'], $afterHeartbeat['lastSeenAt']);

        $this->app['auth']->forgetGuards();
        $this->batch([['temperature' => 22]])->assertCreated();
        $this->app['auth']->forgetGuards();
        $afterReading = $this->actingAs($admin)->getJson($url)->json();
        $this->assertNotSame($first['version'], $afterReading['version']);
    }

    public function test_dashboard_data_relay_shape_matches_page_script(): void
    {
        // show.blade.php's refreshData() reads relays[].current_state[0].
        $this->withToken($this->token)->postJson("/api/devices/{$this->device->id}/relay-state", [
            'relay_number' => 1, 'state' => true, 'mode' => 'MANUAL_ON', 'temp_on' => 20, 'temp_off' => 22,
        ])->assertSuccessful();
        $this->app['auth']->forgetGuards();

        $relay = $this->actingAs(User::factory()->create(['role' => 'admin']))
            ->getJson("/devices/{$this->device->id}/dashboard-data")->json('relays.0');

        $this->assertSame(1, $relay['relay_number']);
        $this->assertTrue((bool) $relay['current_state'][0]['state']);
        $this->assertSame('MANUAL_ON', $relay['current_state'][0]['mode']);
    }

    public function test_summary_lists_only_accessible_devices(): void
    {
        Device::create(['name' => 'other', 'hostname' => 'other', 'mac_address' => '22:22:22:22:22:22']);
        $user = User::factory()->create(['role' => 'viewer']);
        $user->devices()->attach($this->device->id);

        $rows = $this->actingAs($user)->getJson('/dashboard/summary')->assertOk()->json();

        $this->assertCount(1, $rows);
        $this->assertSame($this->device->id, $rows[0]['id']);
        $this->assertSame(60, $rows[0]['updateFrequency']);
    }
}
