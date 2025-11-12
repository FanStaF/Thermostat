@extends('layouts.app')

@section('content')
<div class="card">
    <div style="display: flex; justify-content: space-between; align-items: center; margin-bottom: 20px;">
        <div class="card-title" style="margin-bottom: 0;">User Management</div>
        <a href="{{ route('users.create') }}" class="btn">Add New User</a>
    </div>

    @if(session('success'))
        <div style="padding: 12px; background: #d4edda; color: #155724; border-radius: 4px; margin-bottom: 20px;">
            {{ session('success') }}
        </div>
    @endif

    @if(session('error'))
        <div style="padding: 12px; background: #f8d7da; color: #721c24; border-radius: 4px; margin-bottom: 20px;">
            {{ session('error') }}
        </div>
    @endif

    <table style="width: 100%; border-collapse: collapse;">
        <thead>
            <tr style="border-bottom: 2px solid #ddd; text-align: left;">
                <th style="padding: 12px; font-weight: 600;">Name</th>
                <th style="padding: 12px; font-weight: 600;">Email</th>
                <th style="padding: 12px; font-weight: 600;">Role</th>
                <th style="padding: 12px; font-weight: 600;">Devices</th>
                <th style="padding: 12px; font-weight: 600;">Created</th>
                <th style="padding: 12px; font-weight: 600; text-align: right;">Actions</th>
            </tr>
        </thead>
        <tbody>
            @foreach($users as $user)
                <tr style="border-bottom: 1px solid #eee;">
                    <td style="padding: 12px;">{{ $user->name }}</td>
                    <td style="padding: 12px;">{{ $user->email }}</td>
                    <td style="padding: 12px;">
                        <span class="status {{ $user->role === 'admin' ? 'online' : '' }}" style="display: inline-block;">
                            {{ ucfirst($user->role) }}
                        </span>
                    </td>
                    <td style="padding: 12px;">
                        @if($user->isAdmin())
                            <span style="color: #666; font-size: 13px;">All devices</span>
                        @else
                            <span style="color: #666; font-size: 13px;">{{ $user->devices->count() }} device(s)</span>
                        @endif
                    </td>
                    <td style="padding: 12px; color: #666; font-size: 13px;">
                        {{ $user->created_at->format('M d, Y') }}
                    </td>
                    <td style="padding: 12px; text-align: right;">
                        <a href="{{ route('users.edit', $user) }}" class="btn" style="padding: 6px 12px; font-size: 13px; margin-right: 8px;">
                            Edit
                        </a>
                        @if($user->id !== auth()->id())
                            <form method="POST" action="{{ route('users.destroy', $user) }}" style="display: inline;">
                                @csrf
                                @method('DELETE')
                                <button type="submit" class="btn btn-secondary" style="padding: 6px 12px; font-size: 13px; background: #dc3545;"
                                        onclick="return confirm('Are you sure you want to delete this user?')">
                                    Delete
                                </button>
                            </form>
                        @endif
                    </td>
                </tr>
            @endforeach
        </tbody>
    </table>

    @if($users->isEmpty())
        <div style="text-align: center; padding: 40px; color: #999;">
            No users found.
        </div>
    @endif
</div>
@endsection
