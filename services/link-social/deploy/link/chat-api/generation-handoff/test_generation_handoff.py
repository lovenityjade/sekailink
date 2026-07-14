import importlib.util
import pickle
import tempfile
import unittest
import zipfile
import zlib
from pathlib import Path
from unittest import mock


MODULE_PATH = Path(__file__).with_name("sekailink_generation_handoff.py")
SPEC = importlib.util.spec_from_file_location("sekailink_generation_handoff", MODULE_PATH)
HANDOFF = importlib.util.module_from_spec(SPEC)
assert SPEC and SPEC.loader
SPEC.loader.exec_module(HANDOFF)


class UnsafePicklePayload:
    def __reduce__(self):
        return (eval, ("40 + 2",))


def sync_entry(entry_id: str, slot_index: int, slot_name: str, game: str, config_id: str):
    return {
        "entry_id": entry_id,
        "slot_index": slot_index,
        "slot_name": slot_name,
        "compat_player_name": slot_name,
        "source_slot_name": slot_name,
        "player_file_stem": slot_name,
        "username": "Faruko",
        "user_id": "faruko-user",
        "game": game,
        "game_key": game.lower().replace(" ", "_"),
        "config_id": config_id,
    }


class GenerationHandoffMatchingTests(unittest.TestCase):
    def setUp(self):
        self.entries = [
            sync_entry("1", 1, "Faruko-them-3b9d", "The Minish Cap", "266"),
            sync_entry("2", 2, "Faruko-alin-033f", "A Link to the Past", "211"),
        ]

    def test_safe_multidata_loader_accepts_plain_data(self):
        expected = {"server_options": {"port": 38281}, "locations": {1: {}}}
        payload = zlib.compress(pickle.dumps(expected, protocol=4))
        self.assertEqual(expected, HANDOFF.safe_load_multidata(payload))

    def test_safe_multidata_loader_rejects_arbitrary_globals(self):
        payload = zlib.compress(pickle.dumps(UnsafePicklePayload(), protocol=4))
        with self.assertRaisesRegex(pickle.UnpicklingError, "multidata_global_forbidden"):
            HANDOFF.safe_load_multidata(payload)

    def test_exact_slot_name_wins_over_shared_username(self):
        matched = HANDOFF.match_sync_entry(
            "AP_seed_P2_Faruko-alin-033f.aplttp", 2, self.entries
        )
        self.assertEqual("2", matched["entry_id"])
        self.assertEqual("211", matched["config_id"])

    def test_numeric_slot_wins_when_artifact_has_no_slot_name(self):
        matched = HANDOFF.match_sync_entry("AP_seed_P2_.aplttp", 2, self.entries)
        self.assertEqual("2", matched["entry_id"])

    def test_matching_is_independent_of_sync_entry_order(self):
        matched = HANDOFF.match_sync_entry(
            "AP_seed_P1_Faruko-them-3b9d.aptmc", 1, list(reversed(self.entries))
        )
        self.assertEqual("1", matched["entry_id"])

    def test_zip_index_assigns_each_game_to_its_own_config(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            package = Path(temp_dir) / "AP_seed.zip"
            extract_root = Path(temp_dir) / "artifacts"
            with zipfile.ZipFile(package, "w") as archive:
                archive.writestr("AP_seed_P1_Faruko-them-3b9d.aptmc", b"minish")
                archive.writestr("AP_seed_P2_Faruko-alin-033f.aplttp", b"alttp")
                archive.writestr("AP_seed.archipelago", b"room")

            result = HANDOFF.index_sync_package(
                str(package), self.entries, "test-faruko", extract_root
            )

        launch_entries = sorted(result["launch_entries"], key=lambda row: row["slot"])
        self.assertEqual(["1", "2"], [row["entry_id"] for row in launch_entries])
        self.assertEqual(["266", "211"], [row["config_id"] for row in launch_entries])
        self.assertEqual(
            ["The Minish Cap", "A Link to the Past"],
            [row["game"] for row in launch_entries],
        )
        self.assertTrue(all(row["download"] for row in launch_entries))

    def test_response_exposes_compensator_metadata(self):
        metadata = {"active": True, "slot_name": "Helpful Stranger", "amount": 4}
        response = HANDOFF.response_from_state({"compensator": metadata})
        self.assertEqual(response["compensator"], metadata)

    def test_release_compensator_accepts_all_supported_modes(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            state_path = Path(temp_dir) / "generation_state.json"
            HANDOFF.atomic_write_json(state_path, {
                "generation_id": "test",
                "room_runtime_alive": True,
                "room_runtime_pid": 1,
                "room_port": 38281,
                "room_ap_admin_password": "secret",
                "compensator": {"active": True, "slot_name": "Backup Hero"},
            })
            with mock.patch.object(HANDOFF, "refresh_room_runtime_state"), \
                    mock.patch.object(HANDOFF, "send_compensator_command", new=mock.AsyncMock()) as sender:
                for mode in ("all", "progression", "accelerate"):
                    response = HANDOFF.release_compensator({"mode": mode}, state_path)
                    self.assertTrue(response["ok"])
                    self.assertEqual(response["mode"], mode)
            self.assertEqual(sender.await_count, 3)

    def test_release_compensator_rejects_unknown_mode_without_network(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            state_path = Path(temp_dir) / "generation_state.json"
            HANDOFF.atomic_write_json(state_path, {"compensator": {"active": True}})
            with mock.patch.object(HANDOFF, "send_compensator_command", new=mock.AsyncMock()) as sender:
                response = HANDOFF.release_compensator({"mode": "everything-eventually"}, state_path)
            self.assertFalse(response["ok"])
            sender.assert_not_awaited()

    def test_host_console_reads_bounded_local_logs(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            root = Path(temp_dir)
            generation_log = root / "generation.log"
            room_log = root / "room.log"
            generation_log.write_text("generation line\n", encoding="utf-8")
            room_log.write_text("room line\n", encoding="utf-8")
            state_path = root / "generation_state.json"
            HANDOFF.atomic_write_json(state_path, {
                "generation_id": "test",
                "generation_log_path": str(generation_log),
                "room_runtime_log": str(room_log),
            })
            with mock.patch.object(HANDOFF, "refresh_room_runtime_state"):
                response = HANDOFF.host_console_state({}, state_path)
            self.assertEqual(response["generation_log"], "generation line\n")
            self.assertEqual(response["room_server_log"], "room line\n")

    def test_host_console_command_uses_real_slot_identity(self):
        with tempfile.TemporaryDirectory() as temp_dir:
            state_path = Path(temp_dir) / "generation_state.json"
            HANDOFF.atomic_write_json(state_path, {
                "room_runtime_alive": True,
                "sync_entries": [{"slot_name": "Alice-alin-1234", "game": "A Link to the Past"}],
            })
            with mock.patch.object(HANDOFF, "refresh_room_runtime_state"), \
                    mock.patch.object(HANDOFF, "send_room_admin_command", new=mock.AsyncMock()) as sender:
                response = HANDOFF.host_console_command({"command": "status"}, state_path)
            self.assertTrue(response["ok"])
            sender.assert_awaited_once_with(
                mock.ANY, "/status", "Alice-alin-1234", "A Link to the Past"
            )


if __name__ == "__main__":
    unittest.main()
