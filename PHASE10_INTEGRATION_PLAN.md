# Phase 10: Archipelago WebHostLib Integration

**Goal**: Replace custom implementation with proven Archipelago WebHostLib code
**Status**: IN PROGRESS
**Started**: January 4, 2026

---

## 🎯 INTEGRATION STRATEGY

### Core Principle:
**Use WebHostLib as a library, not reinvent it**

We have the complete, battle-tested Archipelago codebase at:
`/home/sekailink/archipelago_core/`

Instead of building our own generation/server/YAML systems, we'll:
1. Import WebHostLib modules directly
2. Call their functions from our routes
3. Adapt their logic to our SQLAlchemy models
4. Style their templates to match SekaiLink

---

## 📦 INTEGRATION COMPONENTS

### 1. Generation System (HIGH PRIORITY)
**File**: `backend/generation_bridge.py` (NEW)

**Replace**: Our custom `tasks.py::run_generator()`
**With**: WebHostLib's `generate.py::gen_game()`

```python
# backend/generation_bridge.py
import sys
sys.path.insert(0, '/home/sekailink/archipelago_core')

from WebHostLib.check import get_yaml_data, roll_options
from Main import main as archipelago_main
from BaseClasses import get_seed, seeddigits
from Generate import mystery_argparse, PlandoOptions
import tempfile
import random
import os

def generate_multiworld(lobby_id):
    """Generate using Archipelago's proven logic"""
    from models import Lobby, LobbyPlayer, YamlFile, User

    lobby = Lobby.query.get(lobby_id)
    players = LobbyPlayer.query.filter_by(lobby_id=lobby_id).all()

    # Collect YAML contents
    yaml_data = {}
    for player in players:
        yaml_file = YamlFile.query.get(player.yaml_file_id)
        user = User.query.get(player.user_id)
        yaml_data[f"{user.username}.yaml"] = yaml_file.content

    # Use Archipelago's validation and rolling
    results, gen_options = roll_options(yaml_data, set())

    # Check for errors
    if any(isinstance(result, str) for result in results.values()):
        return {"error": "YAML validation failed", "details": results}

    # Create temp directory
    temp_dir = tempfile.TemporaryDirectory()

    # Set up Archipelago generation args
    playercount = len(gen_options)
    seed = get_seed()
    random.seed(seed)
    seedname = "W" + f"{random.randint(0, pow(10, seeddigits) - 1)}".zfill(seeddigits)

    args = mystery_argparse([])
    args.multi = playercount
    args.seed = seed
    args.name = {x: "" for x in range(1, playercount + 1)}
    args.spoiler = 0
    args.race = False
    args.outputname = seedname
    args.outputpath = temp_dir.name
    args.teams = 1
    args.plando_options = PlandoOptions.from_set({"bosses", "items", "connections", "texts"})

    # Apply player options
    from collections import Counter
    name_counter = Counter()
    for player_num, (playerfile, settings) in enumerate(gen_options.items(), 1):
        for k, v in settings.items():
            if v is not None:
                if hasattr(args, k):
                    getattr(args, k)[player_num] = v
                else:
                    setattr(args, k, {player_num: v})

        if not args.name[player_num]:
            args.name[player_num] = os.path.splitext(os.path.split(playerfile)[-1])[0]

    # Generate server options
    server_options = {
        "hint_cost": 10,
        "release_mode": "auto",
        "remaining_mode": "enabled",
        "collect_mode": "enabled",
        "countdown_mode": "disabled",
        "item_cheat": True,
        "server_password": None,
    }

    # Run Archipelago generation
    archipelago_main(args, seed, baked_server_options=server_options)

    # Find generated files
    patches_dir = os.path.join(temp_dir.name, "patches")
    if not os.path.exists(patches_dir):
        patches_dir = temp_dir.name

    generated_files = []
    for filename in os.listdir(patches_dir):
        if filename.endswith(('.zip', '.apbp', '.aplttp', '.apsoe', '.apsmz3', '.appatch')):
            generated_files.append(os.path.join(patches_dir, filename))

    return {
        "success": True,
        "seed": seedname,
        "files": generated_files,
        "temp_dir": temp_dir
    }
```

### 2. YAML Validation (HIGH PRIORITY)
**File**: `backend/yaml_validator.py` (NEW)

**Replace**: Our basic YAML parsing
**With**: WebHostLib's comprehensive validation

```python
# backend/yaml_validator.py
import sys
sys.path.insert(0, '/home/sekailink/archipelago_core')

from WebHostLib.check import get_yaml_data, roll_options

def validate_yaml_upload(file_content, filename):
    """Validate YAML using Archipelago's validator"""
    try:
        yaml_data = {filename: file_content}
        options = get_yaml_data([yaml_data])

        if isinstance(options, str):
            # Error message returned
            return {"valid": False, "error": options}

        # Additional validation through roll_options
        results, gen_options = roll_options(options, set())

        errors = [msg for msg in results.values() if isinstance(msg, str)]
        if errors:
            return {"valid": False, "error": errors[0]}

        # Extract game name from gen_options
        for playerfile, opts in gen_options.items():
            game_name = opts.game
            return {
                "valid": True,
                "game": game_name,
                "player_name": getattr(opts, 'name', 'Player')
            }

    except Exception as e:
        return {"valid": False, "error": str(e)}
```

### 3. YAML Creator UI (HIGH PRIORITY)
**File**: `backend/yaml_creator.py` (NEW)

**Replace**: Missing YAML creation page
**With**: Dynamic form generation from game world definitions

```python
# backend/yaml_creator.py
import sys
sys.path.insert(0, '/home/sekailink/archipelago_core')

from worlds import AutoWorldRegister
from flask import render_template, jsonify
from app import app

@app.route('/games/<game_slug>/create-yaml')
def create_yaml_for_game(game_slug):
    """Generate YAML creation form for a specific game"""

    # Get game world class
    world_types = AutoWorldRegister.world_types

    # Find matching world
    world = None
    for game_name, world_class in world_types.items():
        if game_name.lower().replace(' ', '_') == game_slug.lower():
            world = world_class
            break

    if not world:
        return "Game not found", 404

    # Get options from world
    options_dict = world.options_dataclass.type_hints

    # Build form fields
    form_fields = []
    for option_name, option_type in options_dict.items():
        field = {
            "name": option_name,
            "display_name": option_name.replace('_', ' ').title(),
            "type": str(option_type.__class__.__name__),
        }

        # Check option type
        if hasattr(option_type, 'options'):
            # Choice field
            field["field_type"] = "select"
            field["options"] = list(option_type.options.keys())
            field["default"] = option_type.default
        elif hasattr(option_type, 'range_start'):
            # Range field
            field["field_type"] = "range"
            field["min"] = option_type.range_start
            field["max"] = option_type.range_end
            field["default"] = option_type.default
        elif option_type.__class__.__name__ == 'Toggle':
            # Boolean field
            field["field_type"] = "checkbox"
            field["default"] = option_type.default
        else:
            # Text field
            field["field_type"] = "text"
            field["default"] = getattr(option_type, 'default', '')

        form_fields.append(field)

    return render_template('yaml_creator.html',
                          game_name=world.game,
                          game_slug=game_slug,
                          fields=form_fields)

@app.route('/api/yaml/export', methods=['POST'])
def export_yaml():
    """Export form data to YAML format"""
    import yaml
    from flask import request

    data = request.json
    game_name = data.get('game')
    options = data.get('options', {})
    player_name = data.get('name', 'Player')

    # Build YAML structure
    yaml_content = {
        'name': player_name,
        'game': game_name,
        **options
    }

    # Convert to YAML string
    yaml_string = yaml.dump(yaml_content, default_flow_style=False)

    return jsonify({
        "yaml": yaml_string,
        "filename": f"{player_name}_{game_name}.yaml"
    })
```

### 4. Server Management (MEDIUM PRIORITY)
**File**: `backend/server_manager.py` (NEW)

**Replace**: Our basic server startup
**With**: WebHostLib's multiprocess server management

```python
# backend/server_manager.py
import sys
sys.path.insert(0, '/home/sekailink/archipelago_core')

import multiprocessing
import asyncio
import logging
from WebHostLib.customserver import WebHostContext
from MultiServer import server

class ServerManager:
    """Manages Archipelago server processes"""

    def __init__(self):
        self.processes = {}

    def start_server(self, lobby_id, multidata_path, port):
        """Start an Archipelago server in a separate process"""

        def run_server():
            # Create logger for this server
            logger = logging.getLogger(f"Server-{lobby_id}")
            logger.setLevel(logging.INFO)

            # Load multidata
            import pickle
            with open(multidata_path, 'rb') as f:
                multidata = pickle.load(f)

            # Create context
            context = WebHostContext(multidata, logger)
            context.room_id = lobby_id

            # Start server
            asyncio.run(server(context, host='0.0.0.0', port=port))

        # Start in separate process
        process = multiprocessing.Process(target=run_server, name=f"AP-Server-{lobby_id}")
        process.start()

        self.processes[lobby_id] = {
            "process": process,
            "pid": process.pid,
            "port": port,
            "started_at": datetime.utcnow()
        }

        return process.pid

    def stop_server(self, lobby_id):
        """Stop a running server"""
        if lobby_id in self.processes:
            process = self.processes[lobby_id]["process"]
            process.terminate()
            process.join(timeout=5)
            if process.is_alive():
                process.kill()
            del self.processes[lobby_id]

    def get_server_status(self, lobby_id):
        """Check if server is running"""
        if lobby_id not in self.processes:
            return {"running": False}

        process = self.processes[lobby_id]["process"]
        return {
            "running": process.is_alive(),
            "pid": process.pid,
            "port": self.processes[lobby_id]["port"]
        }

# Global server manager
server_manager = ServerManager()
```

---

## 🔄 UPDATED GENERATION FLOW

### Old Flow (Custom):
```
1. Collect YAMLs from database
2. Copy to /tmp/generation/{lobby_id}/
3. Run subprocess: python Generate.py
4. Hope it works
5. Parse output files
```

### New Flow (WebHostLib):
```
1. Collect YAMLs from database
2. Call get_yaml_data() - validates syntax
3. Call roll_options() - validates against schemas
4. Call archipelago_main() - generates in temp dir
5. Get generated ZIP and patches
6. Store in database
7. Start server with multidata
```

---

## 📝 TEMPLATE INTEGRATION

### Create New Template: `frontend/src/yaml_creator.html`

```html
{% extends "base.html" %}

{% block title %}Create YAML - {{ game_name }} - SekaiLink{% endblock %}

{% block content %}
<div class="container">
    <div class="card">
        <div class="card-header">
            <h1>Create YAML Configuration</h1>
            <p class="text-muted">{{ game_name }}</p>
        </div>

        <div class="card-body">
            <form id="yaml-creator-form">
                <input type="hidden" name="game" value="{{ game_name }}">

                <!-- Player Name -->
                <div class="form-group">
                    <label class="form-label">Player Name</label>
                    <input type="text" name="name" class="form-input"
                           placeholder="Your Name" required maxlength="16">
                </div>

                <div class="divider"></div>

                <!-- Dynamic Fields -->
                {% for field in fields %}
                <div class="form-group">
                    <label class="form-label">{{ field.display_name }}</label>

                    {% if field.field_type == 'select' %}
                        <select name="{{ field.name }}" class="form-select">
                            {% for option in field.options %}
                            <option value="{{ option }}"
                                    {% if option == field.default %}selected{% endif %}>
                                {{ option }}
                            </option>
                            {% endfor %}
                        </select>

                    {% elif field.field_type == 'range' %}
                        <input type="range" name="{{ field.name }}"
                               min="{{ field.min }}" max="{{ field.max }}"
                               value="{{ field.default }}" class="form-range">
                        <output>{{ field.default }}</output>

                    {% elif field.field_type == 'checkbox' %}
                        <div class="form-checkbox">
                            <input type="checkbox" name="{{ field.name }}"
                                   {% if field.default %}checked{% endif %}>
                            <label>Enable</label>
                        </div>

                    {% else %}
                        <input type="text" name="{{ field.name }}"
                               class="form-input" value="{{ field.default }}">
                    {% endif %}
                </div>
                {% endfor %}

                <div class="divider"></div>

                <!-- Actions -->
                <div class="flex gap-md">
                    <button type="submit" class="btn" style="flex: 1;">
                        Download YAML
                    </button>
                    <button type="button" class="btn btn-secondary"
                            onclick="saveToVault()">
                        Save to Vault
                    </button>
                </div>
            </form>
        </div>
    </div>
</div>
{% endblock %}

{% block extra_js %}
<script>
document.getElementById('yaml-creator-form').addEventListener('submit', async (e) => {
    e.preventDefault();

    const formData = new FormData(e.target);
    const options = {};

    for (let [key, value] of formData.entries()) {
        if (key !== 'name' && key !== 'game') {
            options[key] = value;
        }
    }

    const response = await fetch('/api/yaml/export', {
        method: 'POST',
        headers: {'Content-Type': 'application/json'},
        credentials: 'include',
        body: JSON.stringify({
            name: formData.get('name'),
            game: formData.get('game'),
            options: options
        })
    });

    const data = await response.json();

    // Download YAML
    const blob = new Blob([data.yaml], {type: 'text/yaml'});
    const url = window.URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = data.filename;
    a.click();
});

async function saveToVault() {
    // TODO: Save to user's YAML vault
    alert('Save to vault functionality coming soon!');
}
</script>
{% endblock %}
```

---

## 🔌 ROUTE INTEGRATION

### Update `backend/main.py`:

```python
# Import WebHostLib bridges
from generation_bridge import generate_multiworld
from yaml_validator import validate_yaml_upload
from yaml_creator import create_yaml_for_game, export_yaml
from server_manager import server_manager

# Replace generate endpoint
@app.route('/api/generate', methods=['POST'])
def generate_seed():
    """Start seed generation using WebHostLib"""
    uid = session.get('user_id')
    if not uid:
        return jsonify({"error": "not authenticated"}), 401

    data = request.json
    lobby_id = data.get('lobby_id')

    lobby = Lobby.query.get(lobby_id)
    if not lobby or lobby.host_id != uid:
        return jsonify({"error": "unauthorized"}), 403

    # Update status
    lobby.status = 'generating'
    db.session.commit()

    # Start generation using WebHostLib
    from tasks import run_webhost_generation
    run_webhost_generation.delay(lobby_id)

    return jsonify({"status": "started", "lobby_id": lobby_id})

# YAML upload with WebHostLib validation
@app.route('/api/yamls', methods=['POST'])
def upload_yaml():
    """Upload YAML with WebHostLib validation"""
    uid = session.get('user_id')
    if not uid:
        return jsonify({"error": "not authenticated"}), 401

    if 'file' not in request.files:
        return jsonify({"error": "no file provided"}), 400

    file = request.files['file']
    content = file.read().decode('utf-8')

    # Validate using WebHostLib
    validation = validate_yaml_upload(content, file.filename)

    if not validation['valid']:
        return jsonify({"error": validation['error']}), 400

    # Save to database
    yaml_file = YamlFile(
        user_id=uid,
        filename=file.filename,
        content=content,
        game=validation['game']
    )
    db.session.add(yaml_file)
    db.session.commit()

    return jsonify({
        "id": yaml_file.id,
        "filename": yaml_file.filename,
        "game": yaml_file.game
    }), 201
```

---

## 🔧 CELERY TASK UPDATE

### Update `backend/tasks.py`:

```python
from generation_bridge import generate_multiworld
from server_manager import server_manager

@shared_task(bind=True)
def run_webhost_generation(self, lobby_id):
    """Generate using WebHostLib integration"""
    logger.info(f"🎮 Starting WebHostLib generation for lobby {lobby_id}")

    try:
        # Generate using WebHostLib
        result = generate_multiworld(lobby_id)

        if 'error' in result:
            logger.error(f"❌ Generation failed: {result['error']}")

            # Update lobby status
            lobby = Lobby.query.get(lobby_id)
            lobby.status = 'failed'
            db.session.commit()

            return {"status": "ERROR", "error": result['error']}

        # Save generated files
        seed_name = result['seed']
        files = result['files']

        logger.info(f"✅ Generation successful: {seed_name}")
        logger.info(f"📦 Generated {len(files)} files")

        # Find multidata file
        multidata_file = None
        for file in files:
            if file.endswith('.zip'):
                multidata_file = file
                break

        # Find available port
        port = find_available_port()

        # Start server
        logger.info(f"🚀 Starting server on port {port}")
        pid = server_manager.start_server(lobby_id, multidata_file, port)

        # Update lobby
        lobby = Lobby.query.get(lobby_id)
        lobby.status = 'ready'
        lobby.server_port = port
        lobby.seed_url = seed_name
        db.session.commit()

        logger.info(f"✅ Server started (PID: {pid}) on port {port}")

        # Emit WebSocket event
        socketio.emit('generation_complete', {
            'lobby_id': lobby_id,
            'success': True,
            'seed': seed_name,
            'port': port
        }, room=f"lobby_{lobby_id}")

        return {"status": "SUCCESS", "seed": seed_name, "port": port}

    except Exception as e:
        logger.error(f"❌ Generation exception: {str(e)}")

        lobby = Lobby.query.get(lobby_id)
        lobby.status = 'failed'
        db.session.commit()

        return {"status": "ERROR", "error": str(e)}
```

---

## ✅ IMPLEMENTATION CHECKLIST

### Phase 10.1: Generation System
- [ ] Create `backend/generation_bridge.py`
- [ ] Create `backend/yaml_validator.py`
- [ ] Update `backend/tasks.py::run_webhost_generation()`
- [ ] Update `/api/generate` endpoint
- [ ] Update `/api/yamls` upload endpoint
- [ ] Test YAML validation
- [ ] Test generation with 2+ players
- [ ] Verify patches are created

### Phase 10.2: YAML Creator
- [ ] Create `backend/yaml_creator.py`
- [ ] Create `frontend/src/yaml_creator.html`
- [ ] Add route `/games/<slug>/create-yaml`
- [ ] Add route `/api/yaml/export`
- [ ] Test form generation for 3+ games
- [ ] Test YAML download
- [ ] Add "Save to Vault" functionality

### Phase 10.3: Server Management
- [ ] Create `backend/server_manager.py`
- [ ] Integrate multiprocess server startup
- [ ] Add server health monitoring
- [ ] Add server stop/restart endpoints
- [ ] Test server connections
- [ ] Test multiple concurrent servers

### Phase 10.4: Polish & Testing
- [ ] Style YAML creator to match SekaiLink
- [ ] Add loading states to generation
- [ ] Add error messages for generation failures
- [ ] Test full flow: Create → Upload → Generate → Connect
- [ ] Test with ROM-requiring games
- [ ] Document new endpoints

---

## 📊 EXPECTED OUTCOMES

### After Phase 10.1:
✅ Generation uses proven Archipelago logic
✅ YAML validation matches archipelago.gg standards
✅ Generation errors are caught and reported properly
✅ Patches are created correctly

### After Phase 10.2:
✅ Users can create YAMLs for any game
✅ Dynamic forms generated from game definitions
✅ No more manual YAML writing
✅ Matches archipelago.gg/multiworld.gg UX

### After Phase 10.3:
✅ Servers run in isolated processes
✅ Server crashes don't affect main app
✅ Multiple lobbies can run simultaneously
✅ Servers can be managed (stop/restart)

### After Phase 10.4:
✅ Complete SekaiLink experience
✅ Professional, polished UI
✅ End-to-end working system
✅ Ready for production use

---

## 🎯 SUCCESS METRICS

- [ ] Generation success rate: >95%
- [ ] Generation time: <60 seconds for 10 players
- [ ] YAML validation matches archipelago.gg
- [ ] Servers stable for >4 hours
- [ ] Zero crashes during generation
- [ ] All game types supported

---

**Next Step**: Create `generation_bridge.py` and start replacing the generation system!
