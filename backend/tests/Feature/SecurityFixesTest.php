<?php

namespace Tests\Feature;

use App\Models\AlertSubscription;
use App\Models\Device;
use App\Models\User;
use App\Services\AlertEvaluator;
use Illuminate\Foundation\Testing\RefreshDatabase;
use Tests\TestCase;

class SecurityFixesTest extends TestCase
{
    use RefreshDatabase;

    private const MAC = 'AA:BB:CC:DD:EE:FF';

    protected function setUp(): void
    {
        parent::setUp();
        config(['app.api_key' => 'test-key']);
    }

    private function register(?string $bearer = null, string $mac = self::MAC)
    {
        $req = $this->withHeader('X-API-Key', 'test-key');
        if ($bearer) {
            $req = $req->withToken($bearer);
        }

        return $req->postJson('/api/devices/register', [
            'hostname' => 'thermostat',
            'mac_address' => $mac,
            'firmware_version' => '2.3.0',
        ]);
    }

    private function device(string $mac = '11:22:33:44:55:66'): Device
    {
        return Device::create([
            'name' => 'dev-' . $mac,
            'hostname' => 'thermostat-' . $mac,
            'mac_address' => $mac,
            'last_seen_at' => now(),
        ]);
    }

    // ---- #1 registration takeover (option a) ----

    public function test_new_device_registers_with_api_key_only(): void
    {
        $this->register()->assertCreated()->assertJsonStructure(['device_id', 'token']);
    }

    public function test_existing_device_cannot_be_reregistered_with_api_key_only(): void
    {
        $this->register()->assertCreated();
        $this->app['auth']->forgetGuards();

        $this->register()->assertForbidden();
    }

    public function test_existing_device_reregisters_with_its_current_token(): void
    {
        $token = $this->register()->json('token');
        $this->app['auth']->forgetGuards();

        $this->register($token)->assertCreated();
    }

    public function test_another_devices_token_does_not_allow_takeover(): void
    {
        $this->register()->assertCreated();
        $this->app['auth']->forgetGuards();
        $otherToken = $this->register(null, '99:99:99:99:99:99')->json('token');
        $this->app['auth']->forgetGuards();

        $this->register($otherToken)->assertForbidden();
    }

    public function test_admin_approval_allows_one_reregistration(): void
    {
        $deviceId = $this->register()->json('device_id');
        $this->app['auth']->forgetGuards();
        Device::find($deviceId)->update(['reregistration_allowed' => true]);

        $this->register()->assertCreated();
        $this->assertFalse(Device::find($deviceId)->reregistration_allowed);
        $this->app['auth']->forgetGuards();
        $this->register()->assertForbidden();
    }

    public function test_only_admins_can_allow_reregistration(): void
    {
        $device = $this->device();
        $user = User::factory()->create(['role' => 'user']);
        $user->devices()->attach($device->id);

        $this->actingAs($user)->post("/devices/{$device->id}/allow-reregistration")->assertStatus(403);
        $this->actingAs(User::factory()->create(['role' => 'admin']))
            ->postJson("/devices/{$device->id}/allow-reregistration")->assertOk();
        $this->assertTrue($device->refresh()->reregistration_allowed);
    }

    // ---- #2 device tokens on the user API ----

    public function test_device_token_is_rejected_by_user_api(): void
    {
        $token = $this->register()->json('token');

        $this->withToken($token)->getJson('/api/devices')->assertUnauthorized();
        $this->withToken($token)->getJson('/api/alert-subscriptions')->assertUnauthorized();
    }

    public function test_user_api_lists_only_accessible_devices(): void
    {
        $mine = $this->device('11:11:11:11:11:11');
        $this->device('22:22:22:22:22:22');
        $user = User::factory()->create(['role' => 'user']);
        $user->devices()->attach($mine->id);

        $ids = $this->withToken($user->createToken('t')->plainTextToken)
            ->getJson('/api/devices')->assertOk()->json('*.id');

        $this->assertSame([$mine->id], $ids);
    }

    // ---- #3 alert subscriptions respect device access ----

    public function test_cannot_subscribe_to_inaccessible_device(): void
    {
        $other = $this->device();
        $viewer = User::factory()->create(['role' => 'viewer']);

        $this->actingAs($viewer)->postJson('/alert-subscriptions', [
            'device_id' => $other->id,
            'alert_type' => 'device_offline',
        ])->assertForbidden();
    }

    public function test_global_subscription_only_evaluates_accessible_devices(): void
    {
        $mine = $this->device('11:11:11:11:11:11');
        $this->device('22:22:22:22:22:22');
        $user = User::factory()->create(['role' => 'user']);
        $user->devices()->attach($mine->id);
        $sub = new AlertSubscription(['user_id' => $user->id, 'device_id' => null]);
        $sub->setRelation('user', $user);

        $method = new \ReflectionMethod(AlertEvaluator::class, 'getDevicesToCheck');
        $devices = $method->invoke(app(AlertEvaluator::class), $sub);

        $this->assertSame([$mine->id], $devices->pluck('id')->all());
        $this->assertFalse($devices->first()->relationLoaded('temperatureReadings'));
    }

    // ---- #5 login throttling ----

    public function test_login_is_throttled_per_account(): void
    {
        User::factory()->create(['email' => 'a@example.com']);

        for ($i = 0; $i < 5; $i++) {
            $this->post('/login', ['email' => 'a@example.com', 'password' => 'wrong']);
        }
        $this->post('/login', ['email' => 'a@example.com', 'password' => 'wrong'])
            ->assertSessionHasErrors('email');
        $this->assertStringStartsWith('Too many login attempts', session('errors')->first('email'));
    }

    // ---- #7 live dashboard refresh via session ----

    public function test_dashboard_data_works_for_logged_in_user_with_access(): void
    {
        $device = $this->device();
        $user = User::factory()->create(['role' => 'viewer']);
        $user->devices()->attach($device->id);

        $this->actingAs($user)->getJson("/devices/{$device->id}/dashboard-data")
            ->assertOk()->assertJsonStructure(['relays', 'sensorFault']);
    }

    public function test_dashboard_data_denied_without_device_access(): void
    {
        $device = $this->device();

        $this->actingAs(User::factory()->create(['role' => 'viewer']))
            ->getJson("/devices/{$device->id}/dashboard-data")->assertForbidden();
    }

    // ---- #8 schedule time with seconds ----

    public function test_schedule_window_accepts_time_column_format(): void
    {
        $method = new \ReflectionMethod(AlertEvaluator::class, 'isWithinScheduleWindow');
        $evaluator = app(AlertEvaluator::class);

        $this->assertTrue($method->invoke($evaluator, now()->format('H:i') . ':00'));
        $this->assertTrue($method->invoke($evaluator, now()->format('H:i')));
    }

    // ---- #9 device without settings ----

    public function test_device_page_creates_missing_settings(): void
    {
        $device = $this->device();
        $this->assertNull($device->settings);

        // The chart query further down uses MySQL-only UNIX_TIMESTAMP, so on
        // the SQLite test DB the page itself can't render; the settings row is
        // created before that point, which is what this test covers.
        $this->actingAs(User::factory()->create(['role' => 'admin']))
            ->get("/devices/{$device->id}");

        $this->assertNotNull($device->refresh()->settings);
    }
}
