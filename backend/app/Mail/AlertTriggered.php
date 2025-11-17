<?php

namespace App\Mail;

use App\Models\AlertLog;
use Illuminate\Bus\Queueable;
use Illuminate\Contracts\Queue\ShouldQueue;
use Illuminate\Mail\Mailable;
use Illuminate\Mail\Mailables\Content;
use Illuminate\Mail\Mailables\Envelope;
use Illuminate\Queue\SerializesModels;

class AlertTriggered extends Mailable
{
    use Queueable, SerializesModels;

    public function __construct(public AlertLog $alertLog) {}

    public function envelope(): Envelope
    {
        $subscription = $this->alertLog->subscription;

        return new Envelope(
            subject: 'Alert: ' . $subscription->alert_type->label(),
        );
    }

    public function content(): Content
    {
        return new Content(
            markdown: 'emails.alert-triggered',
            with: [
                'alertLog' => $this->alertLog,
                'subscription' => $this->alertLog->subscription,
                'device' => $this->alertLog->device,
                'user' => $this->alertLog->subscription->user,
            ],
        );
    }

    public function attachments(): array
    {
        return [];
    }
}
