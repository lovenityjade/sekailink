"""
Server Manager - Multiprocess Archipelago Server Management

This module provides robust server management with process isolation,
health monitoring, and admin controls. Inspired by WebHostLib's customserver.py
"""

import os
import sys
import subprocess
import psutil
import signal
import time
import logging
from typing import Dict, Optional, List
from datetime import datetime
from dataclasses import dataclass
from enum import Enum

logger = logging.getLogger(__name__)


class ServerStatus(Enum):
    """Server status states"""
    STARTING = "starting"
    RUNNING = "running"
    STOPPING = "stopping"
    STOPPED = "stopped"
    FAILED = "failed"
    CRASHED = "crashed"


@dataclass
class ServerInfo:
    """Information about a running server"""
    lobby_id: int
    pid: int
    port: int
    status: ServerStatus
    started_at: datetime
    last_health_check: Optional[datetime] = None
    multidata_path: str = ""
    log_path: str = ""
    cpu_percent: float = 0.0
    memory_mb: float = 0.0
    player_count: int = 0


class ArchipelagoServerManager:
    """
    Manages Archipelago server processes with isolation and monitoring

    Features:
    - Process isolation (one server crash doesn't affect others)
    - Health monitoring
    - Graceful shutdown
    - Resource tracking
    - Log management
    """

    def __init__(self):
        self.servers: Dict[int, ServerInfo] = {}
        self.archipelago_path = os.path.join(
            os.path.dirname(__file__), '..', 'archipelago_core'
        )
        logger.info("🎮 ArchipelagoServerManager initialized")

    def start_server(self, lobby_id: int, multidata_path: str, port: int) -> Optional[ServerInfo]:
        """
        Start an Archipelago server in a separate process

        Args:
            lobby_id: Lobby database ID
            multidata_path: Path to multidata.zip file
            port: Port number to bind to

        Returns:
            ServerInfo if successful, None otherwise
        """
        # Check if server already running for this lobby
        if lobby_id in self.servers:
            existing = self.servers[lobby_id]
            if existing.status in [ServerStatus.RUNNING, ServerStatus.STARTING]:
                logger.warning(f"⚠️ Server already running for lobby {lobby_id}")
                return existing

        # Find MultiServer script
        server_script = self._find_server_script()
        if not server_script:
            logger.error(f"❌ No Archipelago server script found in {self.archipelago_path}")
            return None

        # Create log directory
        log_dir = f"/tmp/generation/{lobby_id}/logs"
        os.makedirs(log_dir, exist_ok=True)
        log_path = f"{log_dir}/server.log"

        # Build command
        cmd = [
            sys.executable,  # Use same Python as current process
            server_script,
            "--port", str(port),
            multidata_path
        ]

        logger.info(f"🚀 Starting server for lobby {lobby_id}")
        logger.info(f"   Port: {port}")
        logger.info(f"   Multidata: {multidata_path}")
        logger.info(f"   Log: {log_path}")

        try:
            # Open log file
            log_file = open(log_path, 'w')

            # Start process with new session (isolation)
            process = subprocess.Popen(
                cmd,
                cwd=self.archipelago_path,
                stdout=log_file,
                stderr=subprocess.STDOUT,
                start_new_session=True,  # CRITICAL: Isolates from parent
                preexec_fn=os.setpgrp if os.name != 'nt' else None  # Unix process group
            )

            # Create server info
            server_info = ServerInfo(
                lobby_id=lobby_id,
                pid=process.pid,
                port=port,
                status=ServerStatus.STARTING,
                started_at=datetime.utcnow(),
                multidata_path=multidata_path,
                log_path=log_path
            )

            # Store in registry
            self.servers[lobby_id] = server_info

            # Save PID file for recovery
            pid_file = f"/tmp/generation/{lobby_id}/server.pid"
            with open(pid_file, 'w') as f:
                f.write(str(process.pid))

            logger.info(f"✅ Server started (PID: {process.pid})")

            # Wait a moment and check if it started successfully
            time.sleep(2)
            health = self.check_health(lobby_id)

            if health and health.status == ServerStatus.RUNNING:
                logger.info(f"✅ Server confirmed running for lobby {lobby_id}")
            else:
                logger.warning(f"⚠️ Server may have failed to start for lobby {lobby_id}")

            return server_info

        except Exception as e:
            logger.error(f"❌ Failed to start server for lobby {lobby_id}: {e}")
            if lobby_id in self.servers:
                self.servers[lobby_id].status = ServerStatus.FAILED
            return None

    def stop_server(self, lobby_id: int, graceful: bool = True, timeout: int = 10) -> bool:
        """
        Stop an Archipelago server

        Args:
            lobby_id: Lobby database ID
            graceful: If True, send SIGTERM; if False, send SIGKILL
            timeout: Seconds to wait for graceful shutdown

        Returns:
            True if stopped successfully
        """
        if lobby_id not in self.servers:
            logger.warning(f"⚠️ No server found for lobby {lobby_id}")
            return False

        server_info = self.servers[lobby_id]

        if server_info.status in [ServerStatus.STOPPED, ServerStatus.FAILED]:
            logger.info(f"ℹ️ Server already stopped for lobby {lobby_id}")
            return True

        logger.info(f"🛑 Stopping server for lobby {lobby_id} (PID: {server_info.pid})")
        server_info.status = ServerStatus.STOPPING

        try:
            process = psutil.Process(server_info.pid)

            if graceful:
                # Graceful shutdown
                logger.info(f"   Sending SIGTERM to PID {server_info.pid}")
                process.terminate()

                # Wait for process to exit
                try:
                    process.wait(timeout=timeout)
                    logger.info(f"✅ Server stopped gracefully")
                except psutil.TimeoutExpired:
                    logger.warning(f"⚠️ Server did not stop within {timeout}s, forcing...")
                    process.kill()
                    process.wait(timeout=5)
                    logger.info(f"✅ Server force-stopped")
            else:
                # Force kill
                logger.info(f"   Sending SIGKILL to PID {server_info.pid}")
                process.kill()
                process.wait(timeout=5)
                logger.info(f"✅ Server killed")

            server_info.status = ServerStatus.STOPPED
            return True

        except psutil.NoSuchProcess:
            logger.warning(f"⚠️ Process {server_info.pid} not found (already dead)")
            server_info.status = ServerStatus.CRASHED
            return True

        except Exception as e:
            logger.error(f"❌ Error stopping server: {e}")
            server_info.status = ServerStatus.FAILED
            return False

    def restart_server(self, lobby_id: int) -> Optional[ServerInfo]:
        """
        Restart a server (stop then start)

        Args:
            lobby_id: Lobby database ID

        Returns:
            New ServerInfo if successful
        """
        if lobby_id not in self.servers:
            logger.error(f"❌ No server found for lobby {lobby_id}")
            return None

        old_info = self.servers[lobby_id]

        logger.info(f"🔄 Restarting server for lobby {lobby_id}")

        # Stop existing server
        self.stop_server(lobby_id, graceful=True)

        # Wait a moment
        time.sleep(1)

        # Start new server with same configuration
        return self.start_server(lobby_id, old_info.multidata_path, old_info.port)

    def check_health(self, lobby_id: int) -> Optional[ServerInfo]:
        """
        Check if a server is healthy

        Args:
            lobby_id: Lobby database ID

        Returns:
            Updated ServerInfo with health metrics
        """
        if lobby_id not in self.servers:
            return None

        server_info = self.servers[lobby_id]

        try:
            process = psutil.Process(server_info.pid)

            # Check if process is running
            if not process.is_running():
                logger.warning(f"⚠️ Server process {server_info.pid} not running")
                server_info.status = ServerStatus.CRASHED
                return server_info

            # Update metrics
            server_info.cpu_percent = process.cpu_percent(interval=0.1)
            server_info.memory_mb = process.memory_info().rss / 1024 / 1024  # Convert to MB
            server_info.last_health_check = datetime.utcnow()

            # Check if status is still STARTING, upgrade to RUNNING
            if server_info.status == ServerStatus.STARTING:
                # Server has been alive for 2+ seconds, consider it running
                if (datetime.utcnow() - server_info.started_at).total_seconds() > 2:
                    server_info.status = ServerStatus.RUNNING

            return server_info

        except psutil.NoSuchProcess:
            logger.warning(f"⚠️ Process {server_info.pid} not found")
            server_info.status = ServerStatus.CRASHED
            return server_info

        except Exception as e:
            logger.error(f"❌ Error checking health: {e}")
            return server_info

    def check_all_health(self) -> Dict[int, ServerInfo]:
        """
        Check health of all running servers

        Returns:
            Dict of lobby_id -> ServerInfo with updated health
        """
        results = {}

        for lobby_id in list(self.servers.keys()):
            info = self.check_health(lobby_id)
            if info:
                results[lobby_id] = info

        return results

    def get_server_info(self, lobby_id: int) -> Optional[ServerInfo]:
        """
        Get information about a server

        Args:
            lobby_id: Lobby database ID

        Returns:
            ServerInfo if found
        """
        return self.servers.get(lobby_id)

    def list_servers(self) -> List[ServerInfo]:
        """
        List all tracked servers

        Returns:
            List of ServerInfo objects
        """
        return list(self.servers.values())

    def cleanup_dead_servers(self) -> int:
        """
        Remove dead/crashed servers from registry

        Returns:
            Number of servers cleaned up
        """
        cleaned = 0
        to_remove = []

        for lobby_id, info in self.servers.items():
            if info.status in [ServerStatus.CRASHED, ServerStatus.STOPPED, ServerStatus.FAILED]:
                # Verify process is actually dead
                try:
                    process = psutil.Process(info.pid)
                    if not process.is_running():
                        to_remove.append(lobby_id)
                except psutil.NoSuchProcess:
                    to_remove.append(lobby_id)

        for lobby_id in to_remove:
            logger.info(f"🧹 Cleaning up dead server for lobby {lobby_id}")
            del self.servers[lobby_id]
            cleaned += 1

        return cleaned

    def shutdown_all(self, graceful: bool = True):
        """
        Shutdown all running servers

        Args:
            graceful: If True, graceful shutdown; otherwise force kill
        """
        logger.info(f"🛑 Shutting down all servers (graceful={graceful})")

        lobby_ids = list(self.servers.keys())

        for lobby_id in lobby_ids:
            self.stop_server(lobby_id, graceful=graceful)

        logger.info(f"✅ All servers shutdown")

    def _find_server_script(self) -> Optional[str]:
        """
        Find the Archipelago server script

        Returns:
            Path to MultiServer.py or ArchipelagoServer.py
        """
        for script_name in ['MultiServer.py', 'ArchipelagoServer.py']:
            script_path = os.path.join(self.archipelago_path, script_name)
            if os.path.exists(script_path):
                return script_path

        return None

    def get_server_log(self, lobby_id: int, lines: int = 50) -> Optional[str]:
        """
        Get recent log lines from a server

        Args:
            lobby_id: Lobby database ID
            lines: Number of lines to retrieve

        Returns:
            Log content or None
        """
        if lobby_id not in self.servers:
            return None

        log_path = self.servers[lobby_id].log_path

        if not os.path.exists(log_path):
            return None

        try:
            # Use tail to get last N lines efficiently
            result = subprocess.run(
                ['tail', f'-n{lines}', log_path],
                capture_output=True,
                text=True,
                timeout=5
            )
            return result.stdout

        except Exception as e:
            logger.error(f"❌ Error reading log: {e}")
            return None


# Global server manager instance
_server_manager: Optional[ArchipelagoServerManager] = None


def get_server_manager() -> ArchipelagoServerManager:
    """
    Get the global server manager instance (singleton)

    Returns:
        ArchipelagoServerManager instance
    """
    global _server_manager

    if _server_manager is None:
        _server_manager = ArchipelagoServerManager()

    return _server_manager
