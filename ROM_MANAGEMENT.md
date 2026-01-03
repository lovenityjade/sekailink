# ROM Management System

## Overview

SekaiLink implements a **temporary ROM storage system** that prioritizes security, privacy, and efficient storage management. ROMs are never stored permanently and are automatically cleaned up after use.

## Architecture

### Storage Structure

```
/tmp/generation/
├── roms/                      # User ROM storage (temporary)
│   ├── {user_id}/            # Per-user directory
│   │   ├── pokemon_emerald.gba
│   │   ├── pokemon_firered.gba
│   │   └── zelda_alttp.sfc
│   └── {another_user_id}/
│       └── ...
└── {lobby_id}/               # Lobby-specific generation (deleted after 24h)
    ├── player1.yaml
    ├── player2.yaml
    └── seed_output.zip
```

### Storage Lifecycle

1. **Upload** → `/tmp/generation/roms/{user_id}/{filename}`
2. **Generation** → Files copied to `/tmp/generation/{lobby_id}/`
3. **Completion** → Lobby directory deleted after 24 hours
4. **Monthly Cleanup** → User ROMs older than 30 days deleted

## Database Schema

### RomFile Model (Updated)

```python
class RomFile(db.Model):
    id = Integer (Primary Key)
    user_id = String (Foreign Key → users.id)
    filename = String(255)
    file_path = String(500)        # NEW: Temporary storage path
    sha1 = String(40)              # SHA-1 hash for validation
    game_detected = String(100)    # Detected game name
    status = String(20)            # 'verified' or 'unverified'
    uploaded_at = DateTime         # NEW: For cleanup tracking
```

## API Endpoints

### Upload ROM

**POST** `/api/roms/upload`

Upload a ROM file with automatic SHA-1 validation.

**Request:**
```http
POST /api/roms/upload
Content-Type: multipart/form-data

file: <ROM file>
```

**Response (Success - Verified):**
```json
{
    "status": "verified",
    "game": "Pokémon Emerald (USA)",
    "sha1": "f3ae08818175831599faef9e236b28096f600f94",
    "message": "ROM uploaded successfully"
}
```

**Response (Success - Unverified):**
```json
{
    "status": "unverified",
    "game": "Unknown",
    "sha1": "abc123...",
    "message": "ROM uploaded but not verified"
}
```

**Response (Error):**
```json
{
    "error": "invalid file type"
}
```

**Behavior:**
- Stores ROM in `/tmp/generation/roms/{user_id}/`
- Calculates SHA-1 hash
- Validates against known ROM database
- If ROM already exists (same SHA-1), updates the record
- Returns verification status

### List User ROMs

**GET** `/api/roms`

Get all ROMs uploaded by the current user.

**Response:**
```json
[
    {
        "id": 1,
        "name": "pokemon_emerald.gba",
        "sha": "f3ae08818175831599faef9e236b28096f600f94",
        "game": "Pokémon Emerald (USA)",
        "status": "verified"
    }
]
```

### Delete ROM

**DELETE** `/api/roms/{rom_id}`

Delete a ROM from temporary storage.

**Response:**
```json
{
    "message": "ROM deleted successfully"
}
```

**Behavior:**
- Deletes physical file from `/tmp/generation/roms/{user_id}/`
- Removes database record
- Only allows users to delete their own ROMs

## Known ROMs Database

Currently supported games with SHA-1 verification:

```python
KNOWN_ROMS = {
    "f3ae08818175831599faef9e236b28096f600f94": "Pokémon Emerald (USA)",
    "e26ee8a442910395379059f1479fa6972ef784d8": "Pokémon FireRed (USA v1.0)",
    "03a63b3f0e989a518464999597196034c3116bc2": "Zelda: A Link to the Past (USA v1.0)",
    "d63d2c86b4479a381484c47bc4795b54a2221351": "Super Metroid (Japan/USA)",
}
```

### Adding New ROMs

To add support for a new game:

1. Obtain the correct ROM's SHA-1 hash
2. Add to `KNOWN_ROMS` dictionary in `main.py`
3. Update documentation

**Calculate SHA-1:**
```bash
sha1sum rom_file.gba
```

## Automatic Cleanup

### Lobby Cleanup (After Generation)

**When:** 24 hours after successful generation, immediately after failure

**What's Deleted:**
- All files in `/tmp/generation/{lobby_id}/`
- YAMLs, ROMs (copies), generated seeds

**Implementation:**
```python
# In tasks.py
@celery_app.task
def cleanup_lobby_files(lobby_id):
    # Removes /tmp/generation/{lobby_id}/
    shutil.rmtree(f"/tmp/generation/{lobby_id}")
```

**Scheduling:**
- Success: `cleanup_lobby_files.apply_async(args=[lobby_id], countdown=86400)`
- Failure: `cleanup_lobby_files.delay(lobby_id)` (immediate)

### Monthly ROM Cleanup

**When:** 1st of each month (needs to be scheduled via cron/Celery Beat)

**What's Deleted:**
- ROMs older than 30 days
- Empty user directories
- Associated database records

**Implementation:**
```python
# In tasks.py
@celery_app.task
def cleanup_old_roms():
    # Deletes ROMs with uploaded_at < 30 days ago
```

**Scheduling with Celery Beat:**
```python
# In tasks.py or separate config
from celery.schedules import crontab

celery_app.conf.beat_schedule = {
    'monthly-rom-cleanup': {
        'task': 'tasks.cleanup_old_roms',
        'schedule': crontab(day_of_month='1', hour=0, minute=0),
    },
}
```

**Manual Trigger:**
```bash
# From within Docker container or Celery worker
from tasks import cleanup_old_roms
cleanup_old_roms.delay()
```

## Security Features

### 1. SHA-1 Validation
- Every ROM is hashed on upload
- Prevents corruption
- Enables duplicate detection
- Verifies authenticity against known good ROMs

### 2. Temporary Storage
- ROMs stored in `/tmp` (cleared on reboot)
- No permanent storage = no long-term data retention
- Reduces storage costs and privacy concerns

### 3. User Isolation
- Each user has their own directory
- Users can only access/delete their own ROMs
- Authorization checks on all endpoints

### 4. Automatic Cleanup
- Old files automatically removed
- No manual intervention required
- Prevents storage bloat

### 5. File Type Validation
```python
ALLOWED_EXTENSIONS = {'sfc', 'gbc', 'gba', 'n64', 'iso'}
```

## Storage Quotas (Optional - Not Implemented Yet)

Future enhancement to limit ROM uploads per user:

```python
# Example quota system
MAX_ROMS_PER_USER = 10
MAX_ROM_SIZE_MB = 50

# In upload endpoint
user_rom_count = RomFile.query.filter_by(user_id=uid).count()
if user_rom_count >= MAX_ROMS_PER_USER:
    return jsonify({"error": "ROM quota exceeded"}), 429
```

## Frontend Integration

### Upload Form Example

```html
<form id="rom-upload-form" enctype="multipart/form-data">
    <input type="file" name="file" accept=".gba,.gbc,.sfc,.smc,.n64,.z64,.iso" required>
    <button type="submit">Upload ROM</button>
</form>

<script>
document.getElementById('rom-upload-form').addEventListener('submit', async (e) => {
    e.preventDefault();

    const formData = new FormData(e.target);

    try {
        const response = await fetch('/api/roms/upload', {
            method: 'POST',
            body: formData,
            credentials: 'include'
        });

        const result = await response.json();

        if (response.ok) {
            if (result.status === 'verified') {
                alert(`✅ ${result.game} uploaded successfully!`);
            } else {
                alert(`⚠️ ROM uploaded but not verified. SHA-1: ${result.sha1}`);
            }
        } else {
            alert(`❌ Error: ${result.error}`);
        }
    } catch (error) {
        alert('Upload failed: ' + error.message);
    }
});
</script>
```

### Display ROM List

```javascript
async function loadUserROMs() {
    const response = await fetch('/api/roms', {
        credentials: 'include'
    });

    const roms = await response.json();

    const romList = document.getElementById('rom-list');
    romList.innerHTML = roms.map(rom => `
        <div class="rom-item">
            <span class="rom-name">${rom.name}</span>
            <span class="rom-game">${rom.game}</span>
            <span class="rom-status ${rom.status}">
                ${rom.status === 'verified' ? '✅ Verified' : '⚠️ Unverified'}
            </span>
            <button onclick="deleteROM(${rom.id})">Delete</button>
        </div>
    `).join('');
}

async function deleteROM(romId) {
    if (!confirm('Delete this ROM?')) return;

    const response = await fetch(`/api/roms/${romId}`, {
        method: 'DELETE',
        credentials: 'include'
    });

    if (response.ok) {
        alert('ROM deleted');
        loadUserROMs(); // Refresh list
    }
}
```

## Monitoring & Maintenance

### Check Storage Usage

```bash
# Total ROM storage
du -sh /tmp/generation/roms/

# Per user
du -sh /tmp/generation/roms/*

# Lobby directories
du -sh /tmp/generation/*/
```

### Manual Cleanup Commands

```bash
# Clean up old lobby directories (older than 1 day)
find /tmp/generation -maxdepth 1 -type d -mtime +1 -exec rm -rf {} \;

# Clean up empty user directories
find /tmp/generation/roms -type d -empty -delete

# List ROMs by size
du -h /tmp/generation/roms/*/* | sort -h
```

### Celery Task Monitoring

```bash
# View task status
docker-compose exec api python -c "
from tasks import cleanup_lobby_files, cleanup_old_roms
print('Lobby cleanup task:', cleanup_lobby_files)
print('ROM cleanup task:', cleanup_old_roms)
"

# Trigger manual cleanup
docker-compose exec celery_worker python -c "
from tasks import cleanup_old_roms
cleanup_old_roms.delay()
"
```

## Migration from Old System

If you have ROMs in the old `static/uploads/roms/` directory:

```bash
# 1. Create new directory structure
mkdir -p /tmp/generation/roms

# 2. Move existing ROMs to user directories (example)
# You'll need to map filenames to user_ids from your database
# This is just a template:

# 3. Update database records with new file paths
docker-compose exec api python -c "
from main import db, RomFile
import os

roms = RomFile.query.all()
for rom in roms:
    old_path = rom.file_path  # or construct from old pattern
    new_path = f'/tmp/generation/roms/{rom.user_id}/{rom.filename}'

    # Move file if it exists
    if os.path.exists(old_path):
        os.makedirs(os.path.dirname(new_path), exist_ok=True)
        os.rename(old_path, new_path)
        rom.file_path = new_path

db.session.commit()
print('Migration complete')
"

# 4. Clean up old directory
rm -rf backend/static/uploads/roms
```

## Troubleshooting

### "ROM not found" during generation
- Check that ROM still exists: `ls /tmp/generation/roms/{user_id}/`
- Verify database record has correct `file_path`
- Check if ROM was cleaned up (older than 30 days)

### Storage full
- Run manual cleanup: `cleanup_old_roms.delay()`
- Check for stuck lobby directories: `ls /tmp/generation/`
- Increase volume size in Docker if needed

### SHA-1 mismatch
- ROM file may be corrupted
- Wrong ROM version (e.g., EU vs USA)
- Re-upload with correct ROM version

### Permission errors
- Ensure `/tmp/generation` has correct permissions
- Check Docker volume mounts
- Verify user running API has write access

## Best Practices

1. **Always validate ROMs before uploading**
   - Use known good ROM sources
   - Verify SHA-1 hash locally first

2. **Re-upload before monthly cleanup**
   - If you use ROMs regularly, re-upload before 30 days

3. **Monitor storage usage**
   - Set up alerts for disk space
   - Review cleanup logs monthly

4. **Schedule Celery Beat**
   - Ensure monthly cleanup runs automatically
   - Monitor task failures

5. **Backup important data**
   - ROMs are temporary - keep originals elsewhere
   - Database records are backed up separately

## Summary

✅ **Temporary storage** in `/tmp/generation/roms/`
✅ **SHA-1 validation** on upload
✅ **Automatic cleanup** after generation (24h)
✅ **Monthly cleanup** of old ROMs (30 days)
✅ **User isolation** - each user has own directory
✅ **Secure deletion** - files removed from disk
✅ **No permanent storage** - privacy-focused

The ROM management system is designed to be secure, efficient, and automatic. Users don't need to worry about storage management - the system handles it all!
