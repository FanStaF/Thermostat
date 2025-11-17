<?php

namespace App\Events;

use Illuminate\Broadcasting\Channel;
use Illuminate\Broadcasting\InteractsWithSockets;
use Illuminate\Contracts\Broadcasting\ShouldBroadcast;
use Illuminate\Foundation\Events\Dispatchable;
use Illuminate\Queue\SerializesModels;

class RelayStateUpdated implements ShouldBroadcast
{
    use Dispatchable, InteractsWithSockets, SerializesModels;

    public $deviceId;
    public $relayNumber;
    public $state;
    public $mode;
    public $tempOn;
    public $tempOff;

    /**
     * Create a new event instance.
     */
    public function __construct(int $deviceId, int $relayNumber, bool $state, string $mode, float $tempOn, float $tempOff)
    {
        $this->deviceId = $deviceId;
        $this->relayNumber = $relayNumber;
        $this->state = $state;
        $this->mode = $mode;
        $this->tempOn = $tempOn;
        $this->tempOff = $tempOff;
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
        return 'relay.updated';
    }
}
