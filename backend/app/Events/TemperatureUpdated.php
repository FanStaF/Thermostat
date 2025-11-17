<?php

namespace App\Events;

use App\Models\Device;
use Illuminate\Broadcasting\Channel;
use Illuminate\Broadcasting\InteractsWithSockets;
use Illuminate\Contracts\Broadcasting\ShouldBroadcast;
use Illuminate\Foundation\Events\Dispatchable;
use Illuminate\Queue\SerializesModels;

class TemperatureUpdated implements ShouldBroadcast
{
    use Dispatchable, InteractsWithSockets, SerializesModels;

    public $deviceId;
    public $temperature;
    public $humidity;
    public $recordedAt;

    /**
     * Create a new event instance.
     */
    public function __construct(int $deviceId, float $temperature, ?float $humidity, string $recordedAt)
    {
        $this->deviceId = $deviceId;
        $this->temperature = $temperature;
        $this->humidity = $humidity;
        $this->recordedAt = $recordedAt;
    }

    /**
     * Get the channels the event should broadcast on.
     */
    public function broadcastOn(): array
    {
        return [
            new Channel('device.' . $this->deviceId),
        ];
    }

    /**
     * The event's broadcast name.
     */
    public function broadcastAs(): string
    {
        return 'temperature.updated';
    }
}
