from __future__ import annotations

import json
import logging
import multiprocessing
import typing
import traceback
from datetime import timedelta, datetime
from threading import Event, Thread
from typing import Any
from uuid import UUID

from pony.orm import db_session, select, commit, count, desc, PrimaryKey

from Utils import restricted_loads
from .locker import Locker, AlreadyRunningException

_stop_event = Event()


def stop() -> None:
    """Stops previously launched threads"""
    global _stop_event
    stop_event = _stop_event
    _stop_event = Event()  # new event for new threads
    stop_event.set()


# Track in-flight generations with their start time for stuck detection
_in_flight_generations: dict[UUID, datetime] = {}
_in_flight_lock = __import__('threading').Lock()


def _mark_generation_complete(gen_id: UUID) -> None:
    """Remove a generation from in-flight tracking when it completes."""
    with _in_flight_lock:
        _in_flight_generations.pop(gen_id, None)


def _mark_generation_started(gen_id: UUID) -> None:
    """Track when a generation starts for stuck detection."""
    with _in_flight_lock:
        _in_flight_generations[gen_id] = datetime.utcnow()


def _get_stuck_generations(threshold: timedelta) -> list[UUID]:
    """Return IDs of generations that have exceeded the stuck threshold."""
    now = datetime.utcnow()
    with _in_flight_lock:
        return [gid for gid, start_time in _in_flight_generations.items()
                if now - start_time > threshold]


def handle_generation_success(seed_id, generation_id=None):
    if seed_id:
        _mark_generation_complete(seed_id)
    logging.info(f"Generation finished for seed {seed_id}")
    if not generation_id or not seed_id:
        return
    try:
        from WebHostLib import to_url
        from WebHostLib.lobbies import _add_system_message, _emit_generation_update
        from .models import LobbyGeneration, Seed, Room, uuid4
        with db_session:
            lobby_gen = LobbyGeneration.get(generation_id=generation_id)
            if not lobby_gen:
                return
            seed = Seed.get(id=seed_id)
            if not seed:
                lobby_gen.status = "error"
                lobby_gen.error = "Seed not found after generation."
                return
            room = Room(seed=seed, owner=lobby_gen.owner_uuid, tracker=uuid4())
            lobby_gen.seed_id = seed_id
            lobby_gen.room_id = room.id
            lobby_gen.status = "done"
            lobby_gen.completed_at = datetime.utcnow()
            seed_url = f"/seed/{to_url(seed_id)}"
            room_url = f"/room/{to_url(room.id)}"
            _add_system_message(lobby_gen.lobby,
                                f"Generation complete. Seed: {seed_url} Room: {room_url}")
            _emit_generation_update(lobby_gen.lobby)
    except Exception:
        logging.exception("Failed to finalize lobby generation.")


def handle_generation_failure(result: BaseException, generation_id=None):
    try:  # hacky way to get the full RemoteTraceback
        raise result
    except Exception as e:
        logging.exception(e)
        error_details = "".join(traceback.format_exception(type(e), e, e.__traceback__)).strip()
        if error_details:
            error_details = " | ".join(error_details.splitlines())
            if len(error_details) > 1200:
                error_details = error_details[:1200] + "…"
        if generation_id:
            try:
                from WebHostLib.lobbies import _add_system_message, _emit_generation_update
                from .models import LobbyGeneration
                with db_session:
                    lobby_gen = LobbyGeneration.get(generation_id=generation_id)
                    if lobby_gen:
                        lobby_gen.status = "error"
                        lobby_gen.error = error_details or str(e)
                        _add_system_message(
                            lobby_gen.lobby,
                            "Generation failed. Details: " + (error_details or f"{e.__class__.__name__}: {e}")
                        )
                        _emit_generation_update(lobby_gen.lobby)
            except Exception:
                logging.exception("Failed to record lobby generation error.")


def _mp_gen_game(
    gen_options: dict,
    meta: dict[str, Any] | None = None,
    owner=None,
    sid=None,
    timeout: int|None = None,
) -> PrimaryKey | None:
    from setproctitle import setproctitle

    setproctitle(f"Generator ({sid})")
    try:
        return gen_game(gen_options, meta=meta, owner=owner, sid=sid, timeout=timeout)
    finally:
        setproctitle(f"Generator (idle)")


def launch_generator(pool: multiprocessing.pool.Pool, generation: Generation, timeout: int|None) -> None:
    try:
        meta = json.loads(generation.meta)
        options = restricted_loads(generation.options)
        logging.info(f"Generating {generation.id} for {len(options)} players")
        gen_id = generation.id
        pool.apply_async(
            _mp_gen_game,
            (options,),
            {
                "meta": meta,
                "sid": generation.id,
                "owner": generation.owner,
                "timeout": timeout,
            },
            lambda seed_id, gen_id=gen_id: handle_generation_success(seed_id, gen_id),
            lambda err, gen_id=gen_id: handle_generation_failure(err, gen_id),
        )
    except Exception as e:
        generation.state = STATE_ERROR
        commit()
        logging.exception(e)
    else:
        generation.state = STATE_STARTED
        _mark_generation_started(generation.id)


def init_generator(config: dict[str, Any]) -> None:
    from setproctitle import setproctitle

    setproctitle("Generator (idle)")

    try:
        import resource
    except ModuleNotFoundError:
        pass  # unix only module
    else:
        # set soft limit for memory to from config (default 4GiB)
        soft_limit = config["GENERATOR_MEMORY_LIMIT"]
        old_limit, hard_limit = resource.getrlimit(resource.RLIMIT_AS)
        if soft_limit != old_limit:
            resource.setrlimit(resource.RLIMIT_AS, (soft_limit, hard_limit))
            logging.debug(f"Changed AS mem limit {old_limit} -> {soft_limit}")
        del resource, soft_limit, hard_limit

    pony_config = config["PONY"]
    db.bind(**pony_config)
    db.generate_mapping()


def cleanup():
    """delete unowned user-content"""
    with db_session:
        # >>> bool(uuid.UUID(int=0))
        # True
        rooms = Room.select(lambda room: room.owner == UUID(int=0)).delete(bulk=True)
        seeds = Seed.select(lambda seed: seed.owner == UUID(int=0) and not seed.rooms).delete(bulk=True)
        slots = Slot.select(lambda slot: not slot.seed).delete(bulk=True)
        # Command gets deleted by ponyorm Cascade Delete, as Room is Required
    if rooms or seeds or slots:
        logging.info(f"{rooms} Rooms, {seeds} Seeds and {slots} Slots have been deleted.")


def cleanup_lobbies(now: datetime, lobby_timeout: timedelta, empty_timeout: timedelta) -> None:
    from WebHostLib import to_url, socketio
    from .models import Lobby, LobbyMember
    from .lobbies import _delete_lobby_voice, _shutdown_lobby_room
    cutoff = now - min(lobby_timeout, empty_timeout)
    with db_session:
        stale = select(l for l in Lobby if l.last_activity < cutoff)[:200]
        for lobby in stale:
            member_count = count(m for m in LobbyMember if m.lobby == lobby)
            idle_for = now - lobby.last_activity
            should_close = False
            if member_count == 0 and idle_for >= empty_timeout:
                should_close = True
            elif idle_for >= lobby_timeout:
                should_close = True
            else:
                latest = select(
                    lg for lg in LobbyGeneration if lg.lobby == lobby and lg.room_id is not None
                ).order_by(desc(LobbyGeneration.created_at)).first()
                if latest and latest.room_id:
                    room = Room.get(id=latest.room_id)
                    if room and room.last_activity < now - timedelta(seconds=room.timeout + 5):
                        should_close = True
            if not should_close:
                continue

            socketio.emit(
                "lobby_closed",
                {"message": "Lobby closed due to inactivity.", "redirect_url": "/"},
                to=f"lobby:{to_url(lobby.id)}",
            )
            _delete_lobby_voice(lobby)
            _shutdown_lobby_room(lobby)
            lobby.delete()


def autohost(config: dict):
    def keep_running():
        stop_event = _stop_event
        try:
            with Locker("autohost"):
                cleanup()
                last_lobby_cleanup = datetime.utcnow()
                hosters = []
                for x in range(config["HOSTERS"]):
                    hoster = MultiworldInstance(config, x)
                    hosters.append(hoster)
                    hoster.start()

                while not stop_event.wait(0.1):
                    with db_session:
                        rooms = select(
                            room for room in Room if
                            room.last_activity >= datetime.utcnow() - timedelta(days=3))
                        for room in rooms:
                            # we have to filter twice, as the per-room timeout can't currently be PonyORM transpiled.
                            if room.last_activity >= datetime.utcnow() - timedelta(seconds=room.timeout + 5):
                                hosters[room.id.int % len(hosters)].start_room(room.id)
                    now = datetime.utcnow()
                    if now - last_lobby_cleanup >= timedelta(seconds=300):
                        lobby_timeout = timedelta(seconds=config.get("LOBBY_TIMEOUT_SECONDS", 6 * 60 * 60))
                        empty_timeout = timedelta(seconds=config.get("LOBBY_EMPTY_TIMEOUT_SECONDS", 60 * 60))
                        cleanup_lobbies(now, lobby_timeout, empty_timeout)
                        last_lobby_cleanup = now

        except AlreadyRunningException:
            logging.info("Autohost reports as already running, not starting another.")

    Thread(target=keep_running, name="AP_Autohost").start()


def autogen(config: dict):
    def keep_running():
        stop_event = _stop_event
        try:
            with Locker("autogen"):

                with multiprocessing.Pool(config["GENERATORS"], initializer=init_generator,
                                          initargs=(config,), maxtasksperchild=10) as generator_pool:
                    job_time = config["JOB_TIME"]
                    # Grace period: JOB_TIME * 3
                    # When worker is killed and neither the success nor error callback fires.
                    stuck_threshold = timedelta(seconds=(job_time * 3))
                    last_stuck_check = datetime.utcnow()

                    with db_session:
                        to_start = select(generation for generation in Generation if generation.state == STATE_STARTED)

                        if to_start:
                            logging.info("Resuming generation")
                            for generation in to_start:
                                sid = Seed.get(id=generation.id)
                                if sid:
                                    generation.delete()
                                else:
                                    launch_generator(generator_pool, generation, timeout=job_time)

                            commit()
                        select(generation for generation in Generation if generation.state == STATE_ERROR).delete()

                    while not stop_event.wait(0.1):
                        try:
                            now = datetime.utcnow()

                            # Check for stuck generations every 2 mins
                            if now - last_stuck_check > timedelta(seconds=120):
                                last_stuck_check = now
                                stuck_ids = _get_stuck_generations(stuck_threshold)
                                if stuck_ids:
                                    with db_session:
                                        for gid in stuck_ids:
                                            gen = Generation.get(id=gid)
                                            if gen is not None and gen.state == STATE_STARTED:
                                                # Worker died without completing - mark as error
                                                logging.warning(f"Generation {gid} appears stuck (worker may have died), marking as error")
                                                gen.state = STATE_ERROR
                                                meta = json.loads(gen.meta)
                                                meta["error"] = "Generation worker died unexpectedly. Please try again."
                                                gen.meta = json.dumps(meta)
                                            _mark_generation_complete(gid)
                                        commit()

                            with db_session:
                                # for update locks the database row(s) during transaction, preventing writes from elsewhere
                                to_start = select(
                                    generation for generation in Generation
                                    if generation.state == STATE_QUEUED).for_update()
                                for generation in to_start:
                                    launch_generator(generator_pool, generation, timeout=job_time)
                        except Exception as e:
                            logging.exception(e)
                            stop_event.wait(5)
        except AlreadyRunningException:
            logging.info("Autogen reports as already running, not starting another.")

    Thread(target=keep_running, name="AP_Autogen").start()


class MultiworldInstance():
    def __init__(self, config: dict, id: int):
        self.room_ids = set()
        self.process: typing.Optional[multiprocessing.Process] = None
        self.ponyconfig = config["PONY"]
        self.cert = config["SELFLAUNCHCERT"]
        self.key = config["SELFLAUNCHKEY"]
        self.host = config["HOST_ADDRESS"]
        self.rooms_to_start = multiprocessing.Queue()
        self.rooms_shutting_down = multiprocessing.Queue()
        self.name = f"MultiHoster{id}"
        self.process_start_time = None
        self.restart_interval = timedelta(hours=12)

    def start(self):
        if self.process and self.process.is_alive():
            return False

        process = multiprocessing.Process(group=None, target=run_server_process,
                                          args=(self.name, self.ponyconfig, get_static_server_data(),
                                                self.cert, self.key, self.host,
                                                self.rooms_to_start, self.rooms_shutting_down),
                                          name=self.name)
        process.start()
        self.process = process
        self.process_start_time = datetime.utcnow()

    def should_restart(self) -> bool:
        """Check if process should be restarted to reload fresh APWorld data"""
        if not self.process_start_time:
            return False
        
        time_for_restart = datetime.utcnow() - self.process_start_time > self.restart_interval
        is_idle = len(self.room_ids) == 0
        return time_for_restart and is_idle

    def start_room(self, room_id):
        while not self.rooms_shutting_down.empty():
            self.room_ids.remove(self.rooms_shutting_down.get(block=True, timeout=None))

        if self.should_restart():
            logging.info(f"{self.name} restarting to load fresh APWorld data (process was idle, no rooms were interrupted")
            self.stop(wait=True)  # Wait for old process to fully terminate before starting new one
            self.start()

        if room_id in self.room_ids:
            pass  # should already be hosted currently.
        else:
            self.room_ids.add(room_id)
            self.rooms_to_start.put(room_id)

    def stop(self, wait: bool = False):
        if self.process:
            self.process.terminate()
            if wait:
                self.process.join(timeout=5)
                if self.process.is_alive():
                    self.process.kill()
                    self.process.join(timeout=2)
            self.process = None
            self.process_start_time = None
            self.rooms_to_start = multiprocessing.Queue()
            self.rooms_shutting_down = multiprocessing.Queue()

    def done(self):
        return self.process and not self.process.is_alive()

    def collect(self):
        self.process.join()  # wait for process to finish
        self.process = None


from .models import Room, Generation, STATE_QUEUED, STATE_STARTED, STATE_ERROR, db, Seed, Slot
from .customserver import run_server_process, get_static_server_data
from .generate import gen_game
