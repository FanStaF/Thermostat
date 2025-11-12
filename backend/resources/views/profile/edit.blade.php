@extends('layouts.app')

@section('content')
<div class="card">
    <div class="card-title">Edit Profile</div>

    @if(session('success'))
        <div style="padding: 12px; background: #d4edda; color: #155724; border-radius: 4px; margin-bottom: 20px;">
            {{ session('success') }}
        </div>
    @endif

    <form method="POST" action="{{ route('profile.update') }}">
        @csrf
        @method('PATCH')

        <div style="margin-bottom: 20px;">
            <label style="display: block; margin-bottom: 5px; font-weight: 600;">Name</label>
            <input type="text" name="name" value="{{ old('name', $user->name) }}" required
                   style="width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px;">
            @error('name')
                <span style="color: #dc3545; font-size: 13px;">{{ $message }}</span>
            @enderror
        </div>

        <div style="margin-bottom: 20px;">
            <label style="display: block; margin-bottom: 5px; font-weight: 600;">Email</label>
            <input type="email" name="email" value="{{ old('email', $user->email) }}" required
                   style="width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px;">
            @error('email')
                <span style="color: #dc3545; font-size: 13px;">{{ $message }}</span>
            @enderror
        </div>

        <div style="margin-bottom: 20px;">
            <label style="display: block; margin-bottom: 5px; font-weight: 600;">Role</label>
            <input type="text" value="{{ ucfirst($user->role) }}" disabled
                   style="width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px; background: #f5f5f5;">
            <span style="font-size: 12px; color: #666;">Contact an administrator to change your role</span>
        </div>

        <button type="submit" class="btn">Update Profile</button>
    </form>
</div>

<div class="card">
    <div class="card-title">Change Password</div>

    <form method="POST" action="{{ route('profile.password.update') }}">
        @csrf
        @method('PATCH')

        <div style="margin-bottom: 20px;">
            <label style="display: block; margin-bottom: 5px; font-weight: 600;">Current Password</label>
            <input type="password" name="current_password" required
                   style="width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px;">
            @error('current_password')
                <span style="color: #dc3545; font-size: 13px;">{{ $message }}</span>
            @enderror
        </div>

        <div style="margin-bottom: 20px;">
            <label style="display: block; margin-bottom: 5px; font-weight: 600;">New Password</label>
            <input type="password" name="password" required
                   style="width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px;">
            @error('password')
                <span style="color: #dc3545; font-size: 13px;">{{ $message }}</span>
            @enderror
        </div>

        <div style="margin-bottom: 20px;">
            <label style="display: block; margin-bottom: 5px; font-weight: 600;">Confirm New Password</label>
            <input type="password" name="password_confirmation" required
                   style="width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px;">
        </div>

        <button type="submit" class="btn">Change Password</button>
    </form>
</div>
@endsection
