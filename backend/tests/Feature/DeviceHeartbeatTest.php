<?php

namespace Tests\Feature;

use App\Models\Device;
use Illuminate\Foundation\Testing\RefreshDatabase;
use Tests\TestCase;

class DeviceHeartbeatTest extends TestCase
{
    use RefreshDatabase;

    private function makeDevice(array $attrs = []): array
    {
        $device = Device::create(array_merge([
            'name' => 'thermostat',
            'hostname' => 'thermostat',
            'mac_address' => 'AA:BB:CC:DD:EE:FF',
            'ip_address' => '192.168.8.243',
            'firmware_version' => '2.1.0',
            'last_seen_at' => now()->subMinutes(30),
        ], $attrs));
        $token = $device->createToken('device-token')->plainTextToken;

        return [$device, $token];
    }

    private function heartbeat(Device $device, string $token, array $payload)
    {
        return $this->withToken($token)
            ->postJson("/api/devices/{$device->id}/heartbeat", $payload);
    }

    public function test_old_firmware_heartbeat_leaves_fault_fields_alone(): void
    {
        [$device, $token] = $this->makeDevice(['sensor_fault' => true]);

        $this->heartbeat($device, $token, ['ip_address' => '192.168.8.50'])->assertOk();

        $device->refresh();
        $this->assertTrue($device->sensor_fault);
        $this->assertSame('192.168.8.50', $device->ip_address);
        $this->assertTrue($device->is_online);
    }

    public function test_fault_with_age_is_stored_and_active(): void
    {
        [$device, $token] = $this->makeDevice();

        $this->heartbeat($device, $token, [
            'sensor_fault' => true,
            'seconds_since_good_reading' => 120,
            'firmware_version' => '2.2.0',
        ])->assertOk();

        $device->refresh();
        $this->assertTrue($device->has_active_sensor_fault);
        $this->assertSame('2.2.0', $device->firmware_version);
        $this->assertEqualsWithDelta(now()->subSeconds(120)->timestamp, $device->last_good_reading_at->timestamp, 5);
        $this->assertStringContainsString('Last good reading', $device->sensor_fault_message);
    }

    public function test_explicit_null_age_clears_stale_timestamp(): void
    {
        [$device, $token] = $this->makeDevice(['last_good_reading_at' => now()->subDays(3)]);

        $this->heartbeat($device, $token, [
            'sensor_fault' => true,
            'seconds_since_good_reading' => null,
        ])->assertOk();

        $device->refresh();
        $this->assertNull($device->last_good_reading_at);
        $this->assertStringContainsString('No good reading since the device started', $device->sensor_fault_message);
    }

    public function test_null_sensor_fault_does_not_clear_fault(): void
    {
        [$device, $token] = $this->makeDevice(['sensor_fault' => true]);

        $this->heartbeat($device, $token, ['sensor_fault' => null])->assertOk();

        $this->assertTrue($device->refresh()->sensor_fault);
    }

    public function test_huge_age_is_rejected_not_500(): void
    {
        [$device, $token] = $this->makeDevice();

        $this->heartbeat($device, $token, ['seconds_since_good_reading' => 99999999999])
            ->assertStatus(422);
    }

    public function test_fault_is_inactive_when_device_offline(): void
    {
        [$device] = $this->makeDevice(['sensor_fault' => true, 'last_seen_at' => now()->subHour()]);

        $this->assertFalse($device->has_active_sensor_fault);
        $this->assertNull($device->sensor_fault_message);
    }

    public function test_register_clears_stale_fault(): void
    {
        [$device, $token] = $this->makeDevice([
            'sensor_fault' => true,
            'last_good_reading_at' => now()->subDays(3),
        ]);

        // Re-registering an existing device requires its current token.
        $this->withHeader('X-API-Key', 'test-key')
            ->withToken($token)
            ->postJson('/api/devices/register', [
                'hostname' => 'thermostat',
                'mac_address' => 'AA:BB:CC:DD:EE:FF',
                'firmware_version' => '2.2.0',
            ])
            ->assertCreated();

        $device->refresh();
        $this->assertFalse($device->sensor_fault);
        $this->assertNull($device->last_good_reading_at);
    }

    protected function setUp(): void
    {
        parent::setUp();
        // ValidateApiKey compares X-API-Key against app.api_key.
        config(['app.api_key' => 'test-key']);
    }
}
