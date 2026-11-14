<?php

namespace Tests\Feature;

use App\Models\Device;
use App\Models\DeviceBoot;
use Illuminate\Foundation\Testing\RefreshDatabase;
use Tests\TestCase;

class DeviceHealthTest extends TestCase
{
    use RefreshDatabase;

    private Device $device;
    private string $token;

    protected function setUp(): void
    {
        parent::setUp();
        $this->device = Device::create([
            'name' => 'thermostat', 'hostname' => 'thermostat', 'mac_address' => 'AA:BB:CC:DD:EE:FF',
        ]);
        $this->device->settings()->create(['update_frequency' => 60, 'use_fahrenheit' => false, 'timezone' => 'America/Chicago']);
        $this->token = $this->device->createToken('device-token')->plainTextToken;
    }

    private function heartbeat(array $health)
    {
        return $this->withToken($this->token)
            ->postJson("/api/devices/{$this->device->id}/heartbeat", ['health' => $health]);
    }

    private function health(int $boot, int $uptime, string $reason = 'Power On'): array
    {
        return [
            'boot_count' => $boot, 'reset_reason' => $reason, 'uptime_s' => $uptime,
            'wifi_ssid' => 'home-wifi', 'rssi' => -61, 'free_heap' => 22000,
            'max_free_block' => 19000, 'heap_frag' => 12, 'outbox_bytes' => 0,
        ];
    }

    public function test_health_snapshot_is_stored(): void
    {
        $this->heartbeat($this->health(7, 120))->assertOk();

        $h = $this->device->refresh()->health;
        $this->assertSame(7, $h['boot_count']);
        $this->assertSame(-61, $h['rssi']);
        $this->assertArrayHasKey('reported_at', $h);
    }

    public function test_each_new_boot_is_recorded_once_with_boot_time(): void
    {
        $this->heartbeat($this->health(7, 120))->assertOk();
        $this->heartbeat($this->health(7, 150))->assertOk();   // same boot
        $this->heartbeat($this->health(8, 30, 'External System'))->assertOk();

        $boots = DeviceBoot::orderBy('boot_count')->get();
        $this->assertSame([7, 8], $boots->pluck('boot_count')->all());
        $this->assertEqualsWithDelta(now()->subSeconds(30)->timestamp, $boots[1]->booted_at->timestamp, 5);
        $this->assertSame('Reset button / power glitch', $boots[1]->reason_label);
    }

    public function test_invalid_health_is_rejected(): void
    {
        $this->heartbeat(['rssi' => 50])->assertStatus(422);
    }

    public function test_old_firmware_without_health_still_works(): void
    {
        $this->withToken($this->token)->postJson("/api/devices/{$this->device->id}/heartbeat", [])->assertOk();

        $this->assertNull($this->device->refresh()->health);
        $this->assertSame(0, DeviceBoot::count());
    }
}
