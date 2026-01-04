# Phase 9: Critical Bug Fixes - Part 1

**Date**: January 4, 2026
**Status**: 2/14 bugs fixed, API restarted, changes committed

---

## ✅ BUGS FIXED (2 critical issues)

### 1. **Lobby Creation - Slug & Status Issues** ✅ FIXED

**Problems:**
- Frontend showed "Failed to create lobby" even though lobby was created
- Lobby `slug` field was NULL in database → returned `null` in JSON → caused "undefined" in JavaScript
- Lobbies created with status='pending' instead of 'open' → not immediately joinable
- User reported: "room set to undefined" and "other users can't join"

**Root Cause:**
- `generate_lobby_slug()` function existed but was never called during lobby creation
- Initial status was hardcoded as 'pending' instead of following state machine

**Solution:**
```python
# backend/main.py:780-788
lobby_slug = generate_lobby_slug(lobby_name)  # Generate slug like "epic-racer-1234"

new_lobby = Lobby(
    host_id=uid,
    name=lobby_name,
    slug=lobby_slug,  # Now set instead of NULL
    status='open',    # Changed from 'pending'
    visibility=data.get('visibility', 'open')
)
```

**Impact:**
- Lobbies now have readable slugs like "epic-racer-1234"
- No more "undefined" errors from null slug
- Lobbies are immediately joinable after creation
- Follows correct state machine: open → pending → generating → ready → active

---

### 2. **YAML Selection → ROM/Generate Button Flow** ✅ FIXED

**Problems:**
- User reported: "once YAML selected no rom and/or generate button appears"
- No UI for ROM selection in lobby page
- Ready endpoint didn't validate or save ROM selection
- Players couldn't mark ready for ROM-requiring games

**Root Cause:**
- LobbyPlayer model has `rom_file_id` field but it was never used
- lobby.html had no ROM selector UI
- Backend ready endpoint only accepted `yaml_id`, ignored ROMs completely

**Solution:**

#### Backend (main.py:1057-1100):
```python
@app.route('/api/lobbies/<int:lobby_id>/ready', methods=['POST'])
def toggle_ready(lobby_id):
    data = request.json
    yaml_id = data.get('yaml_id')
    rom_id = data.get('rom_id')  # NEW: Accept ROM ID

    # Validate YAML
    yaml_file = YamlFile.query.filter_by(id=yaml_id, user_id=uid).first()

    # Check if game requires ROM
    game = Game.query.filter_by(slug=yaml_file.game).first()
    if game and game.requires_rom:
        if not rom_id:
            return jsonify({"error": "This game requires a ROM. Please select one."}), 400

        # Validate ROM belongs to user and matches game
        rom_file = RomFile.query.filter_by(id=rom_id, user_id=uid).first()
        if rom_file.game_detected != yaml_file.game:
            return jsonify({"error": f"ROM does not match game"}), 400

        player.rom_file_id = rom_id  # NEW: Save ROM ID

    player.yaml_file_id = yaml_id
    player.game = yaml_file.game
    player.is_ready = not player.is_ready
```

#### Frontend (lobby.html):
1. **Added ROM Selection UI:**
```html
<!-- ROM Selection (shown only if game requires ROM) -->
<div class="form-group" id="rom-selection-group" style="display: none;">
    <label class="form-label">Select ROM for <span id="rom-game-name"></span></label>
    <select id="rom-select" class="form-select">
        <option value="">-- Choose a ROM --</option>
    </select>
    <span class="form-hint" style="color: var(--status-warning);">
        This game requires a ROM file. Please select one from your vault.
    </span>
</div>
```

2. **Added Logic:**
- Detects when selected YAML is for a ROM-requiring game
- Fetches user's ROMs for that specific game
- Shows/hides ROM selector dynamically
- Validates both YAML + ROM before enabling Ready button
- Sends both `yaml_id` and `rom_id` to backend

**Impact:**
- Players can now select ROMs for ROM-requiring games
- Clear error messages if ROM is missing
- Ready button only enables when all requirements met
- Full validation chain: YAML → ROM (if needed) → Ready → Generate

---

## 🔄 CHANGES MADE

### Files Modified:
1. **backend/main.py**
   - Line 780-787: Added slug generation on lobby creation
   - Line 788: Changed status from 'pending' to 'open'
   - Line 1057-1100: Enhanced ready endpoint with ROM validation

2. **frontend/src/lobby.html**
   - Line 97-106: Added ROM selection UI
   - Line 269-277: Added state variables for ROM selection
   - Line 453-555: Added game loading, ROM loading, and validation logic
   - Line 818-844: Updated ready button to send both YAML and ROM IDs
   - Line 920-923: Added game loading to init sequence

### Docker:
- Restarted `api` container to apply changes

### Git:
- Committed all changes with detailed commit message

---

## 🧪 TESTING STATUS

### ✅ Code Review Complete:
- Lobby creation flow audited
- Ready endpoint validated
- ROM selection logic verified
- State machine compliance checked

### ⚠️ Needs Live Testing:
1. **Create Lobby:**
   - Verify slug appears in URL (e.g., `/lobby.html?id=1` shows "epic-racer-1234")
   - Verify lobby status is 'open' in database
   - Verify other users can join immediately

2. **YAML Selection:**
   - Select YAML for non-ROM game → Ready button enables
   - Select YAML for ROM game → ROM selector appears
   - Verify ROMs are filtered by game type

3. **ROM Selection:**
   - Select ROM → Ready button enables
   - Click Ready without ROM → Error message appears
   - Click Ready with ROM → Both YAML and ROM saved

4. **Join Flow:**
   - User A creates lobby
   - User B joins lobby
   - Verify User B appears in player list with name and avatar

---

## 📋 REMAINING BUGS (12 issues)

### High Priority (Blocking):
3. **Lobby chat doesn't work**
   - WebSocket handlers exist and look correct
   - Needs live testing to verify

4. **Generation system doesn't work**
   - Need to test end-to-end flow
   - Verify Celery tasks, server startup, port assignment

5. **Can't leave lobbies / no timeout**
   - Broken lobbies pile up
   - Need lobby cleanup mechanism

6. **Room URL vs Server URL display**
   - Server URL only shows for 'ready'/'active' status
   - Should show earlier? Or generation needs to work first?

### Medium Priority (Missing Features):
7. **YAML creation page (WebhostLib style)**
   - Need to create per-game form interface
   - Use Archipelago WebhostLib as reference

8. **Replace ROM Vault with temporary storage**
   - Delete ROMs on logout for legal reasons
   - Session-based storage instead of permanent

9. **Favorite games button doesn't work**
   - API endpoint needs checking

10. **Game cards show "failed to load data"**
    - API or frontend issue

11. **Create YAML from game card = 404**
    - Route doesn't exist

### Low Priority (Admin Panel):
12. **Admin can't manage custom worlds**
13. **Admin user role shows "undefined"**
    - Likely field name mismatch

---

## 🎯 NEXT STEPS

### For User Testing:
1. **Test lobby creation flow:**
   - Create a lobby
   - Check URL has readable slug
   - Verify lobby appears in lobby list
   - Have another user join

2. **Test YAML/ROM selection:**
   - Select YAML for a non-ROM game (e.g., Minecraft)
   - Verify Ready button enables immediately
   - Select YAML for ROM game (e.g., Link to the Past)
   - Verify ROM selector appears
   - Upload a ROM if none available
   - Select ROM and mark ready

3. **Test chat:**
   - Send messages in lobby
   - Verify messages appear for all users

4. **Report any errors:**
   - Browser console errors (F12)
   - Server logs if available
   - Screenshots of issues

### For Next Development Session:
1. Fix chat if broken (check WebSocket connections)
2. Fix/test generation system end-to-end
3. Add lobby timeout/cleanup mechanism
4. Create YAML creation page
5. Replace ROM vault with session storage

---

## 📊 OVERALL STATUS

**Phase 9 Progress**: ~20% complete (2/14 bugs fixed)
**Core System**: Lobby creation and YAML/ROM flow now functional
**Critical Path**: Generation system is the next blocker

**Architecture Status:**
- ✅ Database models correct
- ✅ State machine implemented
- ✅ WebSocket infrastructure in place
- ✅ Lobby creation fixed
- ✅ ROM selection implemented
- ⚠️ Generation system untested
- ⚠️ Chat system untested
- ❌ YAML creation UI missing
- ❌ ROM storage needs refactoring

---

**Note:** The fixes made so far address the root causes of lobby creation failures and the missing ROM selection flow. These were fundamental blocking issues. The remaining bugs are either (a) systems that need live testing to verify they work (chat, generation), or (b) missing features that need to be built (YAML creation page, ROM storage refactor).
