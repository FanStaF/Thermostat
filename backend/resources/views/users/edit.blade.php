@extends('layouts.app')

@section('content')
<a href="{{ route('users.index') }}" class="back-link">&larr; Back to Users</a>

<div class="card">
    <div class="card-title">Edit User</div>

    <form method="POST" action="{{ route('users.update', $user) }}">
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
            <select name="role" required
                    style="width: 100%; padding: 10px; border: 1px solid #ddd; border-radius: 4px; font-size: 14px;">
                <option value="admin" {{ old('role', $user->role) === 'admin' ? 'selected' : '' }}>Admin - Full access to all devices and settings</option>
                <option value="user" {{ old('role', $user->role) === 'user' ? 'selected' : '' }}>User - Can view and control assigned devices</option>
                <option value="viewer" {{ old('role', $user->role) === 'viewer' ? 'selected' : '' }}>Viewer - Can only view assigned devices (read-only)</option>
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
                <div style="display: grid; grid-template-columns: repeat(auto-fill, minmax(250px, 1fr)); gap: 10px;">
                    @foreach($devices as $device)
                        <label style="display: flex; align-items: center; padding: 10px; background: #f8f9fa; border-radius: 4px; cursor: pointer;">
                            <input type="checkbox" name="devices[]" value="{{ $device->id }}"
                                   {{ in_array($device->id, old('devices', $userDevices)) ? 'checked' : '' }}
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
            <button type="submit" class="btn">Update User</button>
            <a href="{{ route('users.index') }}" class="btn btn-secondary">Cancel</a>
        </div>
    </form>
</div>

<div class="card">
    <div class="card-title">Password Reset</div>
    <p style="color: #666; font-size: 14px; margin-bottom: 15px;">
        User passwords cannot be edited directly. The user must change their own password from their Profile page.
    </p>
</div>
@endsection
