# MultiworldGG / Archipelago WebHostLib Integration Plan

**Date**: January 4, 2026
**Purpose**: Leverage proven Archipelago WebHostLib code instead of reinventing the wheel

---

## 🎯 KEY DISCOVERY

You were absolutely right! The Archipelago/MultiworldGG repository has **complete, battle-tested code** for:
- ✅ YAML file handling and validation
- ✅ Generation orchestration
- ✅ Custom server management (WebSocket)
- ✅ Patch file creation and downloads
- ✅ Room/lobby management
- ✅ Player options UI generation

**Location**: `/home/sekailink/archipelago_core/WebHostLib/`

---

## 📂 WebHostLib Directory Structure

```
WebHostLib/
├── generate.py          ⭐ Generation orchestration
├── customserver.py      ⭐ Multi-process server management
├── upload.py            ⭐ YAML upload & validation
├── options.py           ⭐ Game options/YAML creation UI
├── models.py            📊 Database models (Pony ORM)
├── check.py             ✅ YAML validation
├── downloads.py         📥 Patch download handling
├── tracker.py           📊 Live game tracker
├── api/                 🔌 REST API endpoints
├── templates/           🎨 Jinja2 HTML templates
│   ├── generate.html    # Upload YAMLs page
│   ├── waitSeed.html    # Waiting for generation
│   ├── seedError.html   # Generation errors
│   └── ...
└── static/              💅 CSS/JS assets
```

---

## 🔍 KEY FILES ANALYSIS

### 1. **generate.py** - Generation Orchestration

**What it does:**
- Accepts YAML file uploads
- Validates options using `roll_options()`
- Creates temporary directory for generation
- Calls Archipelago's `Main.main()` function
- Handles timeouts and errors
- Uploads ZIP results to database

**Key Functions:**

```python
@app.route('/generate', methods=['GET', 'POST'])
def generate(race=False):
    """Main generation endpoint"""
    files = request.files.getlist('file')  # Get uploaded YAMLs
    options = get_yaml_data(files)         # Parse and validate
    meta = get_meta(request.form, race)    # Get server options
    return start_generation(options, meta)  # Start generation

def gen_game(gen_options, meta, owner, sid, timeout):
    """Actual generation task (runs in thread pool)"""
    target = tempfile.TemporaryDirectory()

    # Set up args for Archipelago generation
    args = mystery_argparse([])
    args.seed = get_seed()
    args.outputpath = target.name
    # ... configure all options

    # Run Archipelago generation
    ERmain(args, seed, baked_server_options=meta["server_options"])

    # Upload results
    return upload_to_db(target.name, sid, owner, race)
```

**Key Insights:**
- Uses `DaemonThreadPoolExecutor` with timeout
- Generates in `/tmp` directory
- Returns redirect to `/wait/<seed_id>` for async generation
- Stores generation state in database (`STATE_QUEUED`, `STATE_ERROR`)
- Supports **race mode** (no hints, no spoilers)

---

### 2. **customserver.py** - Server Management

**What it does:**
- Manages long-running Archipelago MultiServer processes
- Uses `multiprocessing` to isolate server instances
- Handles WebSocket connections
- Listens to database commands
- Provides server context with game data

**Key Classes:**

```python
class WebHostContext(Context):
    """Custom context for web-hosted servers"""
    room_id: int

    def __init__(self, static_server_data, logger):
        super().__init__("", 0, "", "", 1, 40, True, ...)
        self.video = {}  # Twitch/YouTube stream tracking
        self.tags = ["AP", "WebHost"]

    def listen_to_db_commands(self):
        """Poll database for admin commands"""
        while not self.exit_event.is_set():
            commands = select(command for command in Command
                            if command.room.id == self.room_id)
            for command in commands:
                cmdprocessor(command.commandtext)
```

**Key Insights:**
- Uses Pony ORM for database integration
- Servers run in separate processes (isolation + fault tolerance)
- Commands sent via database, not direct calls
- Video stream tracking for Twitch/YouTube
- Custom client message processor for `/video` command

---

### 3. **options.py** - YAML Creation UI

**What it does:**
- Generates HTML forms for game-specific options
- Creates dropdown selects, checkboxes, text inputs
- Provides defaults from game world definitions
- Exports to `.yaml` format

**Key Functions:**

```python
def generate_yaml(game: str):
    """Generate customizable YAML for a game"""
    world = AutoWorldRegister.world_types[game]
    options_dict = world.options_dataclass.type_hints

    # Generate HTML form fields for each option
    for option_name, option_type in options_dict.items():
        if isinstance(option_type, Range):
            # Render slider
        elif isinstance(option_type, Choice):
            # Render dropdown
        # etc...
```

**Key Insights:**
- Dynamically generates forms from world definitions
- No hardcoded HTML - reads from game world classes
- This is what https://multiworld.gg/generate uses!

---

### 4. **models.py** - Database Schema

**What it does:**
- Defines Pony ORM models for:
  - `Room` - Active server instances
  - `Seed` - Generated multiworld seeds
  - `Generation` - Queued generation jobs
  - `Command` - Admin commands to servers
  - `GameDataPackage` - Game metadata cache

```python
class Room(db.Entity):
    id = PrimaryKey(int, auto=True)
    owner = Required(int, index=True)
    seed = Required('Seed', index=True)
    last_activity = Required(datetime.datetime, default=datetime.datetime.utcnow, index=True)
    timeout = Required(datetime.timedelta, default=datetime.timedelta(days=1, hours=12))
    multisave = Optional(bytes, lazy=True)
    # ... more fields

class Seed(db.Entity):
    id = PrimaryKey(UUID, default=lambda: UUID(int=random.getrandbits(128)))
    owner = Optional(int, index=True)
    multidata = Optional(bytes, lazy=True, max_len=2 ** 31 - 1)
    meta = Optional(Json)
    rooms = Set('Room')
```

---

### 5. **upload.py** - YAML Upload & Validation

**What it does:**
- Accepts `.yaml`, `.yml`, `.zip` files
- Validates YAML syntax
- Checks against game world schemas
- Extracts player names and game types

```python
def upload_yaml_zip(files):
    """Process uploaded YAML/ZIP files"""
    options = {}

    for file in files:
        if file.filename.endswith('.zip'):
            # Extract YAMLs from ZIP
            with zipfile.ZipFile(file) as zf:
                for name in zf.namelist():
                    if name.endswith(('.yaml', '.yml')):
                        # Parse YAML
                        data = yaml.safe_load(zf.read(name))
                        options[name] = data
        else:
            # Parse single YAML
            data = yaml.safe_load(file.read())
            options[file.filename] = data

    return validate_options(options)
```

---

## 🔄 HOW ARCHIPELAGO GENERATION WORKS

### Complete Flow:

1. **User Uploads YAMLs**
   - Via `/generate` page (multi-file upload)
   - Files validated with `check.py::get_yaml_data()`

2. **Options Rolled**
   - `check.py::roll_options()` creates player configs
   - Validates against game world schemas
   - Randomizes choices (if set to "random")

3. **Generation Queued (if > threshold)**
   - Creates `Generation` DB entry with `STATE_QUEUED`
   - Background worker picks it up
   - User redirects to `/wait/<seed_id>`

4. **Generation Executes**
   - Runs in thread pool with timeout
   - Calls `Main.main(args, seed, baked_server_options)`
   - Archipelago generates:
     - `AP_<seed>.zip` - Main multiworld data
     - Individual patches (`.aplttp`, `.apsoe`, etc.)
     - Spoiler log (if enabled)

5. **Results Stored**
   - ZIP uploaded to database as `Seed`
   - Multidata stored as blob
   - Patches extracted and stored separately

6. **Server Started (Optional)**
   - Creates `Room` DB entry
   - Spawns `multiprocessing.Process`
   - Runs `MultiServer.server()` with context
   - Listens on assigned port
   - Accepts client connections

7. **Players Download Patches**
   - Via `/seed/<seed_id>` page
   - Patch files served from DB blobs
   - Connect to server with Archipelago client

---

## 🎨 FRONTEND TEMPLATES

### generate.html - Main Upload Page

**Features:**
- Drag & drop YAML upload
- Multi-file selection
- Server options form:
  - Hint cost
  - Release mode (auto/goal/manual)
  - Remaining mode
  - Collect mode
  - Server password
- Plando options (bosses/items/connections/texts)
- Race mode toggle

**URL**: https://multiworld.gg/generate (or https://archipelago.gg/generate)

### waitSeed.html - Generation Progress

**Features:**
- Loading spinner
- WebSocket updates for generation progress
- Auto-redirect when complete
- Error display if failed

### player-options - Per-Game YAML Creator

**Features:**
- Dynamically generated forms per game
- Options read from world definitions:
  - Dropdown selects (Choice options)
  - Number inputs (Range options)
  - Checkboxes (Toggle options)
  - Text inputs (TextChoice options)
- Export to `.yaml` file
- Save to account (if logged in)

**Example URL**: https://archipelago.gg/games/A%20Link%20to%20the%20Past/player-options

---

## 🔧 HOW TO INTEGRATE INTO SEKAILINK

### Option A: Direct WebHostLib Integration (Recommended)

**Pros:**
- ✅ Battle-tested code (used by thousands)
- ✅ Already handles all edge cases
- ✅ Automatic updates when Archipelago updates
- ✅ Minimal custom code needed

**Cons:**
- ⚠️ Uses Pony ORM (we use SQLAlchemy)
- ⚠️ Different Flask app structure
- ⚠️ Need to adapt authentication

**Implementation:**
1. Mount WebHostLib as Flask blueprint
2. Wrap WebHostLib routes with our auth middleware
3. Adapt Pony ORM models to SQLAlchemy (or use both)
4. Use their generation/server logic as-is
5. Style their templates to match SekaiLink

### Option B: Selective Code Reuse

**Pros:**
- ✅ Keep our architecture
- ✅ Use SQLAlchemy
- ✅ Pick only what we need

**Cons:**
- ❌ More work to integrate
- ❌ Need to maintain our own versions
- ❌ Miss out on upstream updates

**Implementation:**
1. Copy generation logic from `generate.py::gen_game()`
2. Adapt to use our models
3. Copy YAML validation from `check.py`
4. Build our own UI inspired by their templates
5. Use their MultiServer setup from `customserver.py`

---

## 🚀 RECOMMENDED INTEGRATION PLAN

### Phase 1: Generation System (High Priority)

**Use WebHostLib's `generate.py` logic:**

```python
# backend/generation_service.py
from WebHostLib.check import get_yaml_data, roll_options
from WebHostLib.generate import gen_game
from Main import main as archipelago_main

def generate_multiworld(lobby_id):
    """Generate using Archipelago's proven logic"""
    lobby = Lobby.query.get(lobby_id)
    players = LobbyPlayer.query.filter_by(lobby_id=lobby_id).all()

    # Collect YAMLs
    yaml_files = []
    for player in players:
        yaml_file = YamlFile.query.get(player.yaml_file_id)
        yaml_files.append(yaml_file.content)

    # Use Archipelago's validation
    options = get_yaml_data(yaml_files)
    results, gen_options = roll_options(options, set())

    # Use Archipelago's generation
    meta = {
        "server_options": {
            "hint_cost": lobby.settings.hint_cost or 10,
            "release_mode": lobby.settings.release_mode or "auto",
            # ... etc
        },
        "generator_options": {
            "spoiler": 0,
            "race": False
        }
    }

    # Generate (this calls Archipelago's Main.main)
    seed_id = gen_game(
        {name: vars(options) for name, options in gen_options.items()},
        meta=meta,
        owner=lobby.host_id,
        timeout=600
    )

    return seed_id
```

### Phase 2: YAML Creation Page (High Priority)

**Use WebHostLib's `options.py` approach:**

```python
# backend/yaml_creator.py
from worlds import AutoWorldRegister

@app.route('/games/<game_slug>/create-yaml')
def create_yaml_for_game(game_slug):
    """Generate YAML creation form for a game"""
    world = AutoWorldRegister.world_types[game_slug]
    options = world.options_dataclass.type_hints

    # Render form dynamically
    return render_template('yaml_creator.html',
                          game=game_slug,
                          options=options)
```

### Phase 3: Server Management (Medium Priority)

**Use WebHostLib's `customserver.py` approach:**

```python
# backend/server_manager.py
import multiprocessing
from WebHostLib.customserver import WebHostContext
from MultiServer import server

def start_server(lobby_id, seed_data, port):
    """Start Archipelago server in separate process"""
    def run_server():
        context = WebHostContext(seed_data, logger)
        context.room_id = lobby_id

        # Start listening
        asyncio.run(server(context, host='0.0.0.0', port=port))

    process = multiprocessing.Process(target=run_server)
    process.start()

    return process.pid
```

---

## 📋 SPECIFIC INTEGRATION STEPS

### Step 1: Fix Immediate Issues (Today)

✅ Done:
- Added join button to homepage
- Added auto-join to lobby page
- Fixed import errors in tasks.py

🔄 Remaining:
- Debug why ROM selector not showing (JavaScript console errors?)
- Test if generation works with fixed imports

### Step 2: Replace Generation Logic (Next Session)

Replace our custom `tasks.py::run_generator()` with:

```python
from WebHostLib.generate import gen_game
from WebHostLib.check import roll_options, get_yaml_data

@shared_task
def run_generator(lobby_id, yaml_contents, output_name):
    # Use Archipelago's battle-tested generation
    options = get_yaml_data(yaml_contents)
    results, gen_options = roll_options(options, set())

    meta = get_generation_meta(lobby_id)
    seed_id = gen_game(gen_options, meta=meta, owner=lobby.host_id)

    # Update lobby with seed_id
    lobby.seed_id = seed_id
    lobby.status = 'ready'
    db.session.commit()
```

### Step 3: Create YAML Builder Pages (2-3 hours)

For each game, create a page like:
`/games/<slug>/create-yaml`

Use their dynamic form generation:
- Read from `worlds/<game>/__init__.py::options_dataclass`
- Generate HTML form fields dynamically
- Export to `.yaml` format
- Save to user's YAML vault

### Step 4: Server Management (3-4 hours)

Replace our server startup with their multiprocess approach:
- Use `WebHostContext` for server state
- Spawn process per lobby
- Use database commands for admin actions
- Track server health and auto-restart if needed

---

## 🎯 QUICK WINS

### 1. Use Their Templates Directly

Copy and adapt:
- `templates/generate.html` → Our `/generate` page
- `templates/waitSeed.html` → Our waiting page
- `templates/tracker.html` → Live game tracker

Just add our nav/header/footer and style to match.

### 2. Use Their YAML Validation

```python
from WebHostLib.check import get_yaml_data

# Instead of our custom validation:
yaml_data = get_yaml_data([file1, file2, file3])
# Returns validated options or error string
```

### 3. Use Their MultiServer Setup

```python
from MultiServer import server, Context

# Instead of our custom server code:
await server(context, host='0.0.0.0', port=38281)
```

---

## 🔗 REFERENCES

### MultiworldGG Live Site:
- https://multiworld.gg/generate
- https://multiworld.gg/games
- https://multiworld.gg/tracker

### Archipelago Official:
- https://archipelago.gg/generate
- https://archipelago.gg/games/A%20Link%20to%20the%20Past/player-options

### Code Repositories:
- MultiworldGG: https://github.com/MultiworldGG/MultiworldGG
- Archipelago: https://github.com/ArchipelagoMW/Archipelago

### Our Local Copy:
- `/home/sekailink/archipelago_core/WebHostLib/`

---

## 💡 NEXT STEPS

1. **Test current fixes** - Join button, auto-join, ROM selector
2. **Check browser console** - Find JavaScript errors blocking ROM selector
3. **Review WebHostLib templates** - See their UI/UX
4. **Plan integration approach** - Option A (full integration) vs Option B (selective)
5. **Prototype generation replacement** - Use their `gen_game()` function
6. **Build YAML creator** - Copy their dynamic form approach

---

## ✅ IMMEDIATE ACTION ITEMS

For you to test:
- [ ] Try joining a lobby from homepage
- [ ] Check if auto-join works when visiting lobby URL
- [ ] Select a YAML - check browser console (F12) for errors
- [ ] Report if ROM selector appears for ROM-requiring games

For next development session:
- [ ] Replace generation logic with WebHostLib's `gen_game()`
- [ ] Build YAML creator using their dynamic form approach
- [ ] Integrate their server management approach
- [ ] Style their templates to match SekaiLink

---

**Bottom Line**: We have a **gold mine** of proven code. Instead of debugging our custom implementation, we should leverage their battle-tested logic. This will save weeks of development and give us a rock-solid foundation.
