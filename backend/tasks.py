import os
import sys
import subprocess
import shutil
import logging
from datetime import datetime
from celery import Celery

# --- LOGGING CONFIGURATION ---
log_level = os.getenv('LOG_LEVEL', 'INFO').upper()
logging.basicConfig(
    level=getattr(logging, log_level),
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger("SekaiLink.Celery")

# --- ENVIRONMENT VALIDATION ---
def validate_celery_environment():
    """
    Validate that required environment variables for Celery are set.
    Fails fast with clear error messages if any are missing.
    """
    required_vars = {
        'REDIS_URL': 'Redis connection URL for Celery broker',
        'DATABASE_URL': 'PostgreSQL database connection URL',
        'ARCHIPELAGO_PATH': 'Path to Archipelago core installation',
    }

    missing_vars = []

    for var_name, description in required_vars.items():
        value = os.getenv(var_name)
        if value is None or value.strip() == '':
            missing_vars.append(f"  - {var_name}: {description}")

    if missing_vars:
        error_msg = "\n" + "="*70 + "\n"
        error_msg += "❌ CELERY CONFIGURATION ERROR: Missing required environment variables\n"
        error_msg += "="*70 + "\n\n"
        error_msg += "Missing variables:\n"
        error_msg += "\n".join(missing_vars) + "\n\n"
        error_msg += "="*70 + "\n"
        error_msg += "How to fix:\n"
        error_msg += "  1. Ensure .env file exists and is properly configured\n"
        error_msg += "  2. Check docker-compose.yml passes environment variables to celery_worker\n"
        error_msg += "  3. See ENV_SETUP.md for detailed setup instructions\n"
        error_msg += "="*70 + "\n"

        logger.error(error_msg)
        sys.exit(1)

    logger.info("✅ Celery environment validation passed")

# Validate environment before initializing Celery
validate_celery_environment()

# Configuration de Celery (Messagerie via Redis)
# Environment is already validated, safe to use without defaults
celery_app = Celery(
    'tasks',
    broker=os.getenv('REDIS_URL'),
    backend=os.getenv('CELERY_RESULT_BACKEND', os.getenv('REDIS_URL'))
)

# Celery configuration
celery_app.conf.update(
    task_serializer='json',
    accept_content=['json'],
    result_serializer='json',
    timezone='UTC',
    enable_utc=True,
    task_track_started=True,
    task_time_limit=3600,  # 1 hour max per task
    task_soft_time_limit=3300,  # 55 minutes soft limit
)

logger.info("🚀 Celery worker initialized successfully")

# --- PORT POOL MANAGEMENT ---
PORT_POOL_START = 38281
PORT_POOL_END = 38380

def find_available_port():
    """
    Find an available port from the pool (38281-38380).
    Checks database for ports in use by active lobbies.
    """
    from sqlalchemy import create_engine
    from sqlalchemy.orm import sessionmaker

    engine = create_engine(os.getenv('DATABASE_URL'))
    Session = sessionmaker(bind=engine)
    db_session = Session()

    try:
        # Import Lobby model
        sys.path.append('/app')
        from models import Lobby

        # Get all active lobbies with assigned ports
        active_lobbies = db_session.query(Lobby).filter(
            Lobby.server_port.isnot(None),
            Lobby.status.in_(['generating', 'ready', 'active'])
        ).all()

        used_ports = {lobby.server_port for lobby in active_lobbies}

        # Find first available port
        for port in range(PORT_POOL_START, PORT_POOL_END + 1):
            if port not in used_ports:
                logger.info(f"📍 Found available port: {port}")
                return port

        logger.error("❌ No available ports in pool!")
        return None

    finally:
        db_session.close()


def start_archipelago_server(lobby_id, seed_file_path, port):
    """
    Start an Archipelago MultiServer for a lobby.
    Returns the process object.
    """
    archipelago_path = os.getenv('ARCHIPELAGO_PATH')

    # Check for MultiServer.py (newer) or ArchipelagoServer.py (older)
    server_script = None
    for script_name in ['MultiServer.py', 'ArchipelagoServer.py']:
        script_path = os.path.join(archipelago_path, script_name)
        if os.path.exists(script_path):
            server_script = script_path
            break

    if not server_script:
        logger.error(f"❌ No Archipelago server script found in {archipelago_path}")
        return None

    cmd = [
        "python3",
        server_script,
        "--port", str(port),
        seed_file_path
    ]

    # Create log file for server output
    log_dir = f"/tmp/generation/{lobby_id}/logs"
    os.makedirs(log_dir, exist_ok=True)
    log_file_path = f"{log_dir}/server.log"

    log_file = open(log_file_path, 'w')

    logger.info(f"🌐 Starting Archipelago server on port {port}")
    logger.info(f"   Command: {' '.join(cmd)}")
    logger.info(f"   Logs: {log_file_path}")

    try:
        process = subprocess.Popen(
            cmd,
            cwd=archipelago_path,
            stdout=log_file,
            stderr=subprocess.STDOUT,
            start_new_session=True  # Detach from parent process
        )

        # Store PID for later cleanup
        pid_file = f"/tmp/generation/{lobby_id}/server.pid"
        with open(pid_file, 'w') as f:
            f.write(str(process.pid))

        logger.info(f"✅ Archipelago server started (PID: {process.pid})")
        return process

    except Exception as e:
        logger.error(f"❌ Failed to start Archipelago server: {e}")
        log_file.close()
        return None


@celery_app.task(bind=True)
def run_webhost_generation(self, lobby_id):
    """
    Generate using WebHostLib integration (NEW - RECOMMENDED)
    This replaces run_generator with Archipelago's proven WebHostLib logic
    """
    from generation_bridge import generate_multiworld
    from sqlalchemy import create_engine
    from sqlalchemy.orm import sessionmaker

    logger.info(f"🎮 Starting WebHostLib generation for lobby {lobby_id}")

    # Connect to database
    engine = create_engine(os.getenv('DATABASE_URL'))
    Session = sessionmaker(bind=engine)
    db_session = Session()

    try:
        # Import models
        sys.path.append('/app')
        from models import Lobby

        # Generate using WebHostLib
        result = generate_multiworld(lobby_id, db_session)

        if not result.get('success'):
            logger.error(f"❌ Generation failed: {result.get('error')}")

            # Update lobby status
            lobby = db_session.query(Lobby).get(lobby_id)
            if lobby:
                lobby.status = 'failed'
                db_session.commit()

            return {"status": "ERROR", "error": result.get('error')}

        # Extract results
        seed_name = result['seed']
        multidata_file = result['multidata_file']
        patch_files = result['patch_files']

        logger.info(f"✅ Generation successful: {seed_name}")
        logger.info(f"📦 Generated {len(patch_files)} patch files")

        # Copy files to persistent storage
        import shutil
        lobby_dir = f"/tmp/generation/{lobby_id}"
        patches_dir = f"{lobby_dir}/patches"
        os.makedirs(patches_dir, exist_ok=True)

        # Copy multidata
        multidata_dest = os.path.join(lobby_dir, os.path.basename(multidata_file))
        shutil.copy2(multidata_file, multidata_dest)
        logger.info(f"📦 Copied multidata to {multidata_dest}")

        # Copy patches
        for patch_file in patch_files:
            patch_dest = os.path.join(patches_dir, os.path.basename(patch_file))
            shutil.copy2(patch_file, patch_dest)
            logger.info(f"🎮 Copied patch: {os.path.basename(patch_file)}")

        # Clean up temp directory
        result['temp_dir'].cleanup()

        # Find available port
        port = find_available_port()

        # Start server using ServerManager (NEW - multiprocess isolation)
        logger.info(f"🚀 Starting server with ServerManager on port {port}")

        from server_manager import get_server_manager
        server_manager = get_server_manager()

        server_info = server_manager.start_server(lobby_id, multidata_dest, port)

        if not server_info:
            logger.error(f"❌ Failed to start server for lobby {lobby_id}")
            lobby = db_session.query(Lobby).get(lobby_id)
            if lobby:
                lobby.status = 'failed'
                db_session.commit()
            return {"status": "ERROR", "error": "Failed to start server"}

        # Update lobby
        lobby = db_session.query(Lobby).get(lobby_id)
        if lobby:
            lobby.status = 'ready'
            lobby.server_port = port
            lobby.seed_url = seed_name
            db_session.commit()

        logger.info(f"✅ Server started (PID: {server_info.pid}) on port {port}")

        return {
            "status": "SUCCESS",
            "seed": seed_name,
            "port": port,
            "patch_count": len(patch_files)
        }

    except Exception as e:
        logger.error(f"❌ Generation exception: {str(e)}")
        logger.exception(e)

        # Update lobby status
        from models import Lobby
        lobby = db_session.query(Lobby).get(lobby_id)
        if lobby:
            lobby.status = 'failed'
            db_session.commit()

        return {"status": "ERROR", "error": str(e)}

    finally:
        db_session.close()


@celery_app.task(bind=True)
def run_generator(self, lobby_id, yaml_paths, output_name):
    """
    Execute Archipelago seed generation with automatic cleanup.

    DEPRECATED: Use run_webhost_generation() instead
    This is kept for backward compatibility only

    This task:
    1. Creates temporary lobby directory
    2. Copies YAMLs (already done by main.py)
    3. Copies required ROMs from user storage
    4. Runs Archipelago generation
    5. Saves patch URLs to database
    6. Schedules cleanup
    """
    from sqlalchemy import create_engine
    from sqlalchemy.orm import sessionmaker

    lobby_dir = f"/tmp/generation/{lobby_id}"
    patches_dir = f"{lobby_dir}/patches"
    os.makedirs(patches_dir, exist_ok=True)

    # Chemin vers le cœur d'Archipelago (monté dans Docker)
    archipelago_path = os.getenv('ARCHIPELAGO_PATH')

    logger.info(f"🎮 Starting generation for lobby {lobby_id} in {lobby_dir}")

    # Connect to database
    engine = create_engine(os.getenv('DATABASE_URL'))
    Session = sessionmaker(bind=engine)
    db_session = Session()

    try:
        # Import models from models package
        import sys
        sys.path.append('/app')
        from models import Lobby, LobbyPlayer, RomFile, User

        # Get lobby and players
        lobby = db_session.query(Lobby).get(lobby_id)
        if not lobby:
            logger.error(f"Lobby {lobby_id} not found")
            return {"status": "ERROR", "details": "Lobby not found"}

        players = db_session.query(LobbyPlayer).filter_by(lobby_id=lobby_id).all()
        logger.info(f"📋 Found {len(players)} players in lobby")

        # Copy ROMs for players who need them
        rom_games = ['Pokemon Emerald', 'Pokemon FireRed', 'Zelda ALTTP', 'Super Metroid', 'A Link to the Past']

        for player in players:
            user = db_session.query(User).get(player.user_id)
            logger.info(f"👤 Processing player: {user.username} (game: {player.game})")

            # Check if this game needs a ROM
            if player.rom_file_id:
                rom = db_session.query(RomFile).get(player.rom_file_id)
                if rom and rom.file_path and os.path.exists(rom.file_path):
                    # Copy ROM to lobby directory
                    rom_dest = os.path.join(lobby_dir, os.path.basename(rom.file_path))
                    shutil.copy2(rom.file_path, rom_dest)
                    logger.info(f"✅ Copied ROM for {user.username}: {rom.filename}")
                else:
                    logger.warning(f"⚠️ ROM not found for {user.username}")

        # Run Archipelago generation
        logger.info("🔧 Running Archipelago Generate.py...")

        cmd = [
            "python3",
            f"{archipelago_path}/Generate.py",
            "--player_files_path", lobby_dir,
            "--outputpath", patches_dir,
            "--seed", str(os.urandom(8).hex())
        ]

        logger.info(f"Command: {' '.join(cmd)}")
        process = subprocess.run(cmd, capture_output=True, text=True, timeout=600, cwd=archipelago_path)

        if process.returncode != 0:
            logger.error(f"❌ Generation failed: {process.stderr}")
            lobby.status = 'failed'
            db_session.commit()

            # Emit WebSocket event
            emit_generation_complete(lobby_id, False, None, process.stderr)

            cleanup_lobby_files.delay(lobby_id)
            return {"status": "FAILED", "error": process.stderr}

        logger.info(f"✅ Generation completed successfully")
        logger.info(f"Output: {process.stdout}")

        # Find generated files
        generated_files = []
        for filename in os.listdir(patches_dir):
            if filename.endswith(('.zip', '.apbp', '.aplttp', '.apsoe', '.apsmz3', '.appatch')):
                generated_files.append(filename)
                logger.info(f"📦 Found patch file: {filename}")

        # Map patch files to players
        # Archipelago typically names files like "AP_<seed>_P<player_number>_<game>.zip"
        # or creates individual patch files per player

        if not generated_files:
            logger.error("❌ No patch files generated")
            lobby.status = 'failed'
            db_session.commit()
            emit_generation_complete(lobby_id, False, None, "No patch files generated")
            cleanup_lobby_files.delay(lobby_id)
            return {"status": "FAILED", "error": "No patch files generated"}

        # Update players with patch URLs
        for idx, player in enumerate(players):
            user = db_session.query(User).get(player.user_id)

            # Try to match patch file to player
            # Simple approach: use index or find file with player name
            patch_file = None

            # Look for file with username
            for f in generated_files:
                if user.username.lower() in f.lower():
                    patch_file = f
                    break

            # If not found, use main zip file (multiworld case)
            if not patch_file and generated_files:
                patch_file = generated_files[0]  # Use first file (usually the main .zip)

            if patch_file:
                player.patch_url = f"/api/lobbies/{lobby_id}/patches/{patch_file}"
                logger.info(f"✅ Assigned patch to {user.username}: {patch_file}")

        # Find and assign server port
        port = find_available_port()
        if not port:
            logger.error("❌ No available ports for Archipelago server")
            lobby.status = 'failed'
            db_session.commit()
            emit_generation_complete(lobby_id, False, None, "No available server ports")
            cleanup_lobby_files.delay(lobby_id)
            return {"status": "FAILED", "error": "No available server ports"}

        # Start Archipelago server
        # Use the main .zip file as the seed
        seed_file = os.path.join(patches_dir, generated_files[0])
        server_process = start_archipelago_server(lobby_id, seed_file, port)

        if not server_process:
            logger.error("❌ Failed to start Archipelago server")
            lobby.status = 'failed'
            db_session.commit()
            emit_generation_complete(lobby_id, False, None, "Failed to start game server")
            cleanup_lobby_files.delay(lobby_id)
            return {"status": "FAILED", "error": "Failed to start game server"}

        # Update lobby status
        lobby.status = 'ready'
        lobby.seed_url = f"/api/lobbies/{lobby_id}/patches/{generated_files[0]}"
        lobby.server_port = port
        lobby.started_at = datetime.utcnow()
        db_session.commit()

        logger.info(f"🎉 Generation complete for lobby {lobby_id}!")

        # Emit WebSocket event
        emit_generation_complete(lobby_id, True, lobby.seed_url, None)

        # Schedule cleanup after 24 hours
        cleanup_lobby_files.apply_async(args=[lobby_id], countdown=86400)

        return {
            "status": "SUCCESS",
            "lobby_id": lobby_id,
            "patches": generated_files,
            "seed_url": lobby.seed_url
        }

    except subprocess.TimeoutExpired:
        logger.error(f"⏱️ Generation timeout for lobby {lobby_id}")

        lobby = db_session.query(Lobby).get(lobby_id)
        if lobby:
            lobby.status = 'failed'
            db_session.commit()

        emit_generation_complete(lobby_id, False, None, "Generation timeout (10 minutes)")
        cleanup_lobby_files.delay(lobby_id)
        return {"status": "ERROR", "details": "Generation timeout"}

    except Exception as e:
        logger.error(f"❌ Generation error for lobby {lobby_id}: {str(e)}")
        logger.exception(e)

        try:
            lobby = db_session.query(Lobby).get(lobby_id)
            if lobby:
                lobby.status = 'failed'
                db_session.commit()
        except:
            pass

        emit_generation_complete(lobby_id, False, None, str(e))
        cleanup_lobby_files.delay(lobby_id)
        return {"status": "ERROR", "details": str(e)}

    finally:
        db_session.close()


def emit_generation_complete(lobby_id, success, seed_url, error):
    """Emit WebSocket event for generation completion"""
    try:
        # This would normally use socketio.emit, but we're in Celery
        # For now, we'll update the database and let the frontend poll
        # TODO: Use Redis pub/sub for real-time updates
        logger.info(f"Generation complete: success={success}, seed_url={seed_url}, error={error}")
    except Exception as e:
        logger.error(f"Failed to emit WebSocket event: {e}")


@celery_app.task
def stop_archipelago_server(lobby_id):
    """
    Stop the Archipelago server for a lobby.
    Reads the PID from file and kills the process.
    """
    pid_file = f"/tmp/generation/{lobby_id}/server.pid"

    if not os.path.exists(pid_file):
        logger.warning(f"No server PID file found for lobby {lobby_id}")
        return {"status": "skipped", "reason": "no pid file"}

    try:
        with open(pid_file, 'r') as f:
            pid = int(f.read().strip())

        logger.info(f"🛑 Stopping Archipelago server for lobby {lobby_id} (PID: {pid})")

        # Try to kill the process
        try:
            os.kill(pid, 15)  # SIGTERM - graceful shutdown
            logger.info(f"✅ Sent SIGTERM to process {pid}")

            # Wait a bit, then force kill if still running
            import time
            time.sleep(2)

            try:
                os.kill(pid, 9)  # SIGKILL - force kill
                logger.info(f"✅ Sent SIGKILL to process {pid}")
            except ProcessLookupError:
                logger.info(f"✅ Process {pid} already terminated")

        except ProcessLookupError:
            logger.info(f"✅ Process {pid} not running")

        return {"status": "stopped", "pid": pid}

    except Exception as e:
        logger.error(f"Failed to stop server for lobby {lobby_id}: {str(e)}")
        return {"status": "error", "error": str(e)}


@celery_app.task
def cleanup_lobby_files(lobby_id):
    """
    Clean up temporary files for a lobby after generation.
    This removes YAMLs, ROMs, and generated files from /tmp/generation/{lobby_id}
    Also stops the Archipelago server if running.
    """
    # Stop the server first
    stop_archipelago_server(lobby_id)

    # Clean up generation directory
    lobby_dir = f"/tmp/generation/{lobby_id}"
    rom_dir = f"/tmp/lobbies/{lobby_id}"

    total_size = 0
    cleaned_dirs = []

    # Clean generation directory
    if os.path.exists(lobby_dir):
        try:
            dir_size = sum(
                os.path.getsize(os.path.join(dirpath, filename))
                for dirpath, _, filenames in os.walk(lobby_dir)
                for filename in filenames
            )
            shutil.rmtree(lobby_dir)
            total_size += dir_size
            cleaned_dirs.append("generation")
            logger.info(f"🗑️ Cleaned up generation dir for lobby {lobby_id}")
        except Exception as e:
            logger.error(f"Failed to cleanup generation dir: {str(e)}")

    # Clean temporary ROM upload directory
    if os.path.exists(rom_dir):
        try:
            dir_size = sum(
                os.path.getsize(os.path.join(dirpath, filename))
                for dirpath, _, filenames in os.walk(rom_dir)
                for filename in filenames
            )
            shutil.rmtree(rom_dir)
            total_size += dir_size
            cleaned_dirs.append("roms")
            logger.info(f"🗑️ Cleaned up ROM dir for lobby {lobby_id}")
        except Exception as e:
            logger.error(f"Failed to cleanup ROM dir: {str(e)}")

    if not cleaned_dirs:
        logger.warning(f"No directories to clean for lobby {lobby_id}")
        return {"status": "skipped", "reason": "no directories found"}

    logger.info(f"🗑️ Cleaned up lobby {lobby_id}: {', '.join(cleaned_dirs)} ({total_size / 1024 / 1024:.2f} MB freed)")
    return {
        "status": "cleaned",
        "lobby_id": lobby_id,
        "directories": cleaned_dirs,
        "size_freed_mb": total_size / 1024 / 1024
    }


@celery_app.task
def cleanup_old_roms():
    """
    Monthly cleanup task to remove old ROM files.
    Removes ROMs older than 30 days from /tmp/generation/roms/

    This should be scheduled to run on the 1st of each month.
    """
    from datetime import timedelta
    from sqlalchemy import create_engine
    from sqlalchemy.orm import sessionmaker

    logger.info("🗑️ Starting monthly ROM cleanup")

    rom_base = "/tmp/generation/roms"
    if not os.path.exists(rom_base):
        logger.info("No ROM directory found, nothing to clean")
        return {"status": "skipped", "reason": "no roms directory"}

    # Connect to database to update records
    engine = create_engine(os.getenv('DATABASE_URL'))
    Session = sessionmaker(bind=engine)
    db_session = Session()

    cutoff_date = datetime.utcnow() - timedelta(days=30)
    cleaned_count = 0
    freed_space = 0

    try:
        # Import RomFile model dynamically to avoid circular imports
        from models import RomFile

        # Find old ROM records
        old_roms = db_session.query(RomFile).filter(
            RomFile.uploaded_at < cutoff_date
        ).all()

        for rom in old_roms:
            # Delete physical file
            if rom.file_path and os.path.exists(rom.file_path):
                try:
                    file_size = os.path.getsize(rom.file_path)
                    os.remove(rom.file_path)
                    freed_space += file_size
                    logger.info(f"Deleted old ROM: {rom.file_path}")
                except Exception as e:
                    logger.error(f"Failed to delete ROM file {rom.file_path}: {e}")

            # Delete database record
            db_session.delete(rom)
            cleaned_count += 1

        db_session.commit()

        # Also clean up any orphaned user directories
        for user_dir in os.listdir(rom_base):
            user_path = os.path.join(rom_base, user_dir)
            if os.path.isdir(user_path) and not os.listdir(user_path):
                # Remove empty user directories
                os.rmdir(user_path)
                logger.info(f"Removed empty ROM directory: {user_path}")

        logger.info(f"✅ ROM cleanup complete: {cleaned_count} ROMs removed, {freed_space / 1024 / 1024:.2f} MB freed")

        return {
            "status": "success",
            "roms_cleaned": cleaned_count,
            "space_freed_mb": freed_space / 1024 / 1024
        }

    except Exception as e:
        logger.error(f"ROM cleanup failed: {str(e)}")
        db_session.rollback()
        return {"status": "error", "error": str(e)}

    finally:
        db_session.close()


@celery_app.task
def check_time_limits():
    """
    Periodic task to check for time limit violations.
    Runs every minute to check all active lobbies with time limits.

    If a lobby exceeds its time limit and restrict_time_limit is enabled:
    - Marks lobby as finished
    - Broadcasts time_limit_exceeded event
    - Stops the game timer
    """
    from sqlalchemy import create_engine
    from sqlalchemy.orm import sessionmaker

    engine = create_engine(os.getenv('DATABASE_URL'))
    Session = sessionmaker(bind=engine)
    db_session = Session()

    try:
        # Import models
        sys.path.append('/app')
        from models import Lobby, LobbySettings

        # Find all active lobbies with running timers
        active_lobbies = db_session.query(Lobby).filter(
            Lobby.status == 'active',
            Lobby.timer_started_at.isnot(None)
        ).all()

        violations_found = 0

        for lobby in active_lobbies:
            # Get lobby settings
            settings = db_session.query(LobbySettings).filter_by(lobby_id=lobby.id).first()

            # Skip if no time limit set or not in restrict mode
            if not settings or not settings.time_limit_hours or not settings.restrict_time_limit:
                continue

            # Calculate elapsed time
            elapsed_seconds = (datetime.utcnow() - lobby.timer_started_at).total_seconds()
            time_limit_seconds = settings.time_limit_hours * 3600

            # Check if time limit exceeded
            if elapsed_seconds >= time_limit_seconds:
                logger.warning(f"⏰ Time limit exceeded for lobby {lobby.id} ({lobby.name})")

                # Mark lobby as finished
                lobby.status = 'finished'
                db_session.commit()

                violations_found += 1

                # Emit WebSocket event (using Redis pub/sub if available)
                try:
                    emit_time_limit_exceeded(lobby.id, int(elapsed_seconds), settings.time_limit_hours)
                except Exception as e:
                    logger.error(f"Failed to emit time limit exceeded event: {e}")

        if violations_found > 0:
            logger.info(f"⏰ Time limit check complete: {violations_found} lobbies exceeded time limit")

        return {
            "status": "success",
            "lobbies_checked": len(active_lobbies),
            "violations": violations_found
        }

    except Exception as e:
        logger.error(f"Time limit check failed: {str(e)}")
        logger.exception(e)
        return {"status": "error", "error": str(e)}

    finally:
        db_session.close()


def emit_time_limit_exceeded(lobby_id, elapsed_seconds, time_limit_hours):
    """
    Emit time limit exceeded event via WebSocket.
    In production, this would use Redis pub/sub.
    """
    logger.info(f"🚨 TIME LIMIT EXCEEDED: Lobby {lobby_id} - {elapsed_seconds}s / {time_limit_hours}h limit")

    # TODO: Implement Redis pub/sub for real-time WebSocket events from Celery
    # For now, the frontend will detect this when polling /api/lobbies/<id>/timer
    pass


@celery_app.task
def update_server_ratings():
    """
    Periodic task to update server ratings for all users.
    Calculates ratings based on:
    - Total kicks received (-0.2 per kick)
    - Total bans/suspensions (-1.0 per ban)
    - Total warnings (-0.1 per warning)
    - Total syncs completed (+0.05 per completion)
    - Total DNF/forfeits (-0.1 per DNF)

    Rating starts at 5.0 and is capped between 1.0 and 5.0
    """
    from sqlalchemy import create_engine
    from sqlalchemy.orm import sessionmaker

    engine = create_engine(os.getenv('DATABASE_URL'))
    Session = sessionmaker(bind=engine)
    db_session = Session()

    try:
        # Import models
        sys.path.append('/app')
        from models import User, ServerRating, LobbyPlayer, Ban, Warning

        logger.info("📊 Starting server rating calculation...")

        all_users = db_session.query(User).all()
        updated_count = 0

        for user in all_users:
            # Get or create server rating
            server_rating = db_session.query(ServerRating).filter_by(user_id=user.id).first()

            if not server_rating:
                server_rating = ServerRating(
                    user_id=user.id,
                    rating=5.0,
                    total_kicks=0,
                    total_bans=0,
                    total_warnings=0,
                    total_syncs_completed=0,
                    total_dnf=0
                )
                db_session.add(server_rating)

            # Count kicks (from lobby player records where they were kicked)
            # TODO: Add a 'was_kicked' field to LobbyPlayer model in future
            total_kicks = 0  # Placeholder for now

            # Count bans
            total_bans = db_session.query(Ban).filter_by(user_id=user.id).count()

            # Count warnings
            total_warnings = db_session.query(Warning).filter_by(user_id=user.id).count()

            # Count syncs completed
            total_syncs = db_session.query(LobbyPlayer).filter_by(
                user_id=user.id,
                status='finished'
            ).count()

            # Count DNFs/forfeits
            total_dnf = db_session.query(LobbyPlayer).filter(
                LobbyPlayer.user_id == user.id,
                LobbyPlayer.status.in_(['dnf', 'forfeit'])
            ).count()

            # Calculate rating
            # Start at 5.0
            rating = 5.0

            # Penalties
            rating -= (total_kicks * 0.2)
            rating -= (total_bans * 1.0)
            rating -= (total_warnings * 0.1)
            rating -= (total_dnf * 0.1)

            # Bonuses
            rating += (total_syncs * 0.05)

            # Cap between 1.0 and 5.0
            rating = max(1.0, min(5.0, rating))

            # Update server rating
            server_rating.total_kicks = total_kicks
            server_rating.total_bans = total_bans
            server_rating.total_warnings = total_warnings
            server_rating.total_syncs_completed = total_syncs
            server_rating.total_dnf = total_dnf
            server_rating.rating = round(rating, 2)
            server_rating.last_updated = datetime.utcnow()

            updated_count += 1

        db_session.commit()

        logger.info(f"✅ Server ratings updated for {updated_count} users")

        return {
            "status": "success",
            "users_updated": updated_count
        }

    except Exception as e:
        logger.error(f"Server rating calculation failed: {str(e)}")
        logger.exception(e)
        db_session.rollback()
        return {"status": "error", "error": str(e)}

    finally:
        db_session.close()