<?php

namespace App\Jobs;

use App\Mail\AlertTriggered;
use App\Models\AlertLog;
use App\Models\User;
use Illuminate\Contracts\Queue\ShouldQueue;
use Illuminate\Foundation\Queue\Queueable;
use Illuminate\Support\Facades\Mail;

class SendAlertEmail implements ShouldQueue
{
    use Queueable;

    public function __construct(
        public User $user,
        public AlertLog $alertLog
    ) {}

    public function handle(): void
    {
        Mail::to($this->user->email)
            ->send(new AlertTriggered($this->alertLog));
    }
}
