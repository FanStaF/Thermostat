<?php

namespace App\Models;

// use Illuminate\Contracts\Auth\MustVerifyEmail;
use Illuminate\Database\Eloquent\Factories\HasFactory;
use Illuminate\Foundation\Auth\User as Authenticatable;
use Illuminate\Notifications\Notifiable;
use Laravel\Sanctum\HasApiTokens;

class User extends Authenticatable
{
    /** @use HasFactory<\Database\Factories\UserFactory> */
    use HasApiTokens, HasFactory, Notifiable;

    /**
     * The attributes that are mass assignable.
     *
     * @var list<string>
     */
    protected $fillable = [
        'name',
        'email',
        'password',
        'role',
    ];

    /**
     * The attributes that should be hidden for serialization.
     *
     * @var list<string>
     */
    protected $hidden = [
        'password',
        'remember_token',
    ];

    /**
     * Get the attributes that should be cast.
     *
     * @return array<string, string>
     */
    protected function casts(): array
    {
        return [
            'email_verified_at' => 'datetime',
            'password' => 'hashed',
        ];
    }

    public function devices()
    {
        return $this->belongsToMany(Device::class, 'device_user')
            ->withPivot('role')
            ->withTimestamps();
    }

    // Permission helper methods
    public function isAdmin()
    {
        return $this->role === 'admin';
    }

    public function isUser()
    {
        return $this->role === 'user';
    }

    public function isViewer()
    {
        return $this->role === 'viewer';
    }

    public function canControl()
    {
        return in_array($this->role, ['admin', 'user']);
    }

    public function canManageUsers()
    {
        return $this->isAdmin();
    }

    public function canAccessDevice($deviceId)
    {
        if ($this->isAdmin()) {
            return true;
        }
        return $this->devices()->where('device_id', $deviceId)->exists();
    }
}
