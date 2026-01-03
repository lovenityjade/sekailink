# Discord OAuth Login Loop - FIXED ✅

## Date: January 3, 2026

---

## Problem

When users tried to login with Discord:
1. Click "Login with Discord"
2. Authorize on Discord
3. Get redirected back to SekaiLink
4. **Immediately redirect back to Discord (infinite loop)**

---

## Root Cause

**Session Mismatch**:
- OAuth callback stored: `session['user_id']` (just a string ID)
- Dashboard & all pages expected: `session['user']` (full user object with username, avatar, role, etc.)

**The Flow**:
```
1. Login → Discord OAuth → Callback
2. Callback sets session['user_id'] = "abc123"
3. Redirect to /dashboard.html
4. Dashboard checks: if 'user' not in session → FAIL (only user_id exists)
5. Dashboard redirects to /api/auth/login
6. Back to Discord OAuth → INFINITE LOOP
```

---

## Fix Applied

**File**: `/home/sekailink/backend/main.py` (lines 206-217)

**Before**:
```python
session['user_id'] = user.id
return redirect("/dashboard.html")
```

**After**:
```python
# Store user object in session (not just ID)
session['user_id'] = user.id
session['user'] = {
    'id': user.id,
    'username': user.username,
    'avatar': user.avatar_url,
    'email': user.email,
    'discord_id': user.discord_id,
    'role': user.role,
    'bio': user.bio,
    'pronouns': user.pronouns
}
return redirect("/dashboard.html")
```

Now the session contains both:
- `session['user_id']` - for API routes that need just the ID
- `session['user']` - for templates that need the full user object

---

## How to Test

### 1. **Clear Your Browser Cookies**
   - The old broken session might still be cached
   - Go to Developer Tools (F12) → Application → Cookies → Clear all for sekailink.xyz

### 2. **Test Login Flow**
   1. Go to https://sekailink.xyz
   2. Click "Login with Discord"
   3. Authorize the app (if first time)
   4. You should be redirected to https://sekailink.xyz/dashboard.html
   5. **No loop!** You should see your dashboard

### 3. **Verify Session Works**
   - Check that your username appears in the header
   - Navigation should show "Dashboard", "Games", "Active Lobbies"
   - Clicking dashboard should NOT redirect to login
   - User dropdown should show your Discord avatar

### 4. **Test Logout**
   - Click your avatar → Logout
   - Should redirect to homepage
   - Click login again → Should work without loop

---

## Expected Behavior After Fix

✅ **Login**: Discord OAuth → Dashboard (one redirect, no loop)
✅ **Session Persists**: Refresh dashboard → stays logged in
✅ **Protected Pages**: Access dashboard/settings without redirect loop
✅ **User Info**: Username and avatar display in navigation
✅ **Logout**: Clears session and redirects to homepage

---

## Session Data Structure

After successful login, `session['user']` contains:
```python
{
    'id': 'uuid-string',
    'username': 'YourDiscordName',
    'avatar': 'https://cdn.discordapp.com/avatars/...',
    'email': 'your@email.com',
    'discord_id': '123456789',
    'role': 'user',  # or 'moderator', 'admin'
    'bio': None,     # or user's bio if set
    'pronouns': None # or user's pronouns if set
}
```

Templates access this via: `{{ user.username }}`, `{{ user.avatar }}`, etc.

---

## What Still Doesn't Work (Expected)

These features require Phase 3 API implementation:

❌ **Dashboard Content**: Shows placeholder data (no real YAMLs, ROMs, lobbies)
❌ **YAML Manager**: Upload/create/edit not implemented yet
❌ **ROM Manager**: Upload/validation not implemented yet
❌ **Twitch Connection**: OAuth not implemented yet
❌ **Friends/Blacklist**: API not implemented yet
❌ **Create Lobby**: Form exists but POST endpoint not implemented
❌ **Profile Edit**: Update button doesn't work (API missing)

But **authentication itself now works!**

---

## Files Modified

1. `/home/sekailink/backend/main.py`
   - Line 206-217: Updated OAuth callback to store full user object

---

## Testing Checklist

After clearing cookies, test these scenarios:

- [ ] Fresh login from homepage works
- [ ] No infinite redirect loop
- [ ] Dashboard loads after OAuth
- [ ] Username shows in header navigation
- [ ] User dropdown menu shows avatar
- [ ] Refresh dashboard → stays logged in
- [ ] Logout → redirects to homepage
- [ ] Login again → works without loop
- [ ] Protected pages accessible when logged in
- [ ] Protected pages redirect to login when NOT logged in

---

## Server Status

**API**: ✅ Restarted with fix
**Domain**: ✅ https://sekailink.xyz
**OAuth**: ✅ Fixed and ready to test
**Session**: ✅ Properly stores user object

---

**Try logging in now!** The infinite loop should be fixed. 🎉

If you still see issues, clear your browser cookies and try again.

---

*Fix applied: January 3, 2026 15:44 UTC*
*OAuth flow: Discord → Callback → Dashboard (no loop)*
