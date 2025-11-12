@extends('layouts.app')

@section('content')
<style>
    .device-grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(250px, 1fr)); gap: 10px; }
    @media (max-width: 768px) {
        .device-grid { grid-template-columns: 1fr; }
        input[type="text"], input[type="email"], input[type="password"], select {
            font-size: 16px !important; /* Prevents zoom on iOS */
        }
    }
</style>

<a href="{{ route('users.index') }}" class="back-link">&larr; Back to Users</a>

<div class="card">
    <div class="card-title">Create New User</div>

    <form method="POST" action="{{ route('users.store') }}">
        @csrf

        <div style="margin-bottom: 20px;">
            <label style="display: block; margin-bottom: 5px; font-weight: 600;">Name</label>
            <input type="text" name="name" value="{{ old('name') }}" required
                   style="width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px;">
            @error('name')
                <span style="color: #dc3545; font-size: 13px;">{{ $message }}</span>
            @enderror
        </div>

        <div style="margin-bottom: 20px;">
            <label style="display: block; margin-bottom: 5px; font-weight: 600;">Email</label>
            <input type="email" name="email" value="{{ old('email') }}" required
                   style="width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px;">
            @error('email')
                <span style="color: #dc3545; font-size: 13px;">{{ $message }}</span>
            @enderror
        </div>

        <div style="margin-bottom: 20px;">
            <label style="display: block; margin-bottom: 5px; font-weight: 600;">Password</label>
            <input type="password" name="password" required
                   style="width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px;">
            @error('password')
                <span style="color: #dc3545; font-size: 13px;">{{ $message }}</span>
            @enderror
            <span style="font-size: 12px; color: #666;">Minimum 8 characters</span>
        </div>

        <div style="margin-bottom: 20px;">
            <label style="display: block; margin-bottom: 5px; font-weight: 600;">Confirm Password</label>
            <input type="password" name="password_confirmation" required
                   style="width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px;">
        </div>

        <div style="margin-bottom: 20px;">
            <label style="display: block; margin-bottom: 5px; font-weight: 600;">Role</label>
            <select name="role" required
                    style="width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px;">
                <option value="">Select Role</option>
                <option value="admin" {{ old('role') === 'admin' ? 'selected' : '' }}>Admin - Full access to all devices and settings</option>
                <option value="user" {{ old('role') === 'user' ? 'selected' : '' }}>User - Can view and control assigned devices</option>
                <option value="viewer" {{ old('role') === 'viewer' ? 'selected' : '' }}>Viewer - Can only view assigned devices (read-only)</option>
            </select>
            @error('role')
                <span style="color: #dc3545; font-size: 13px;">{{ $message }}</span>
            @enderror
        </div>

        <div style="margin-bottom: 20px;">
            <label style="display: block; margin-bottom: 10px; font-weight: 600;">Assigned Devices</label>
            <span style="font-size: 12px; color: #666; display: block; margin-bottom: 10px;">
                Admin users have access to all devices. Select specific devices for User and Viewer roles.
            </span>

            @if($devices->isEmpty())
                <div style="padding: 12px; background: #fff3cd; color: #856404; border-radius: 4px; font-size: 13px;">
                    No devices available. Users can be assigned devices later.
                </div>
            @else
                <div class="device-grid">
                    @foreach($devices as $device)
                        <label style="display: flex; align-items: center; padding: 10px; background: #f8f9fa; border-radius: 4px; cursor: pointer;">
                            <input type="checkbox" name="devices[]" value="{{ $device->id }}"
                                   {{ in_array($device->id, old('devices', [])) ? 'checked' : '' }}
                                   style="margin-right: 8px;">
                            <span style="font-size: 14px;">{{ $device->name }}</span>
                        </label>
                    @endforeach
                </div>
            @endif
            @error('devices')
                <span style="color: #dc3545; font-size: 13px;">{{ $message }}</span>
            @enderror
        </div>

        <div style="display: flex; gap: 10px;">
            <button type="submit" class="btn">Create User</button>
            <a href="{{ route('users.index') }}" class="btn btn-secondary">Cancel</a>
        </div>
    </form>
</div>
@endsection
