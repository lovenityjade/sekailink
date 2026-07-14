import argparse
import json
import tempfile
import unittest
from pathlib import Path

from sekailink_compensation import compensator_name, run_generation_with_compensation


class CapacityFailure(RuntimeError):
    def __init__(self, code, deficits):
        super().__init__(code)
        self.code = code
        self.deficits = deficits


class SekaiLinkCompensationTests(unittest.TestCase):
    def base_args(self, outputpath):
        return argparse.Namespace(
            multi=2,
            name={1: "Alice", 2: "Bob"},
            game={1: "Game A", 2: "Game B"},
            plando=object(),
            player_options={},
            outputpath=str(outputpath),
        )

    @staticmethod
    def roll_settings(weights, _plando):
        return argparse.Namespace(
            game=weights["game"],
            name=weights["name"],
            compensation_mode=weights["SekaiLink Compensator"]["compensation_mode"],
            compensation_deficits=weights["SekaiLink Compensator"]["compensation_deficits"],
            compensation_location_count=weights["SekaiLink Compensator"]["compensation_location_count"],
            original_player_count=weights["SekaiLink Compensator"]["original_player_count"],
        )

    def test_retries_once_with_same_seed_and_real_slot(self):
        with tempfile.TemporaryDirectory() as tmp:
            calls = []

            def ermain(args, seed):
                calls.append((args, seed))
                if len(calls) == 1:
                    raise CapacityFailure("item_location_deficit", {1: 2, 2: 1})
                return "generated"

            result, metadata = run_generation_with_compensation(
                self.base_args(Path(tmp)), 123456, ermain, self.roll_settings
            )
            self.assertEqual(result, "generated")
            self.assertEqual([call[1] for call in calls], [123456, 123456])
            self.assertEqual(calls[1][0].multi, 3)
            self.assertEqual(calls[1][0].game[3], "SekaiLink Compensator")
            self.assertEqual(metadata["amount"], 3)
            self.assertEqual(metadata["slot"], 3)
            persisted = json.loads((Path(tmp) / "sekailink_compensator.json").read_text("utf-8"))
            self.assertEqual(persisted, metadata)

    def test_unknown_failure_is_not_retried(self):
        calls = 0

        def ermain(_args, _seed):
            nonlocal calls
            calls += 1
            raise RuntimeError("rom_missing")

        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaisesRegex(RuntimeError, "rom_missing"):
                run_generation_with_compensation(self.base_args(Path(tmp)), 1, ermain, self.roll_settings)
        self.assertEqual(calls, 1)

    def test_retry_uses_pristine_options_after_generator_mutation(self):
        with tempfile.TemporaryDirectory() as tmp:
            calls = []

            def ermain(args, seed):
                calls.append((args, seed))
                if len(calls) == 1:
                    args.name[1] = "Mutated by failed generation"
                    raise CapacityFailure("filler_item_deficit", {2: 1})
                return args.name[1]

            result, metadata = run_generation_with_compensation(
                self.base_args(Path(tmp)), 44, ermain, self.roll_settings
            )
            self.assertEqual(result, "Alice")
            self.assertEqual(metadata["mode"], "fillers")

    def test_name_is_deterministic_and_collision_safe(self):
        first = compensator_name(99, [])
        self.assertEqual(first, compensator_name(99, []))
        self.assertNotEqual(first.casefold(), compensator_name(99, [first]).casefold())
        self.assertLessEqual(len(first), 16)


if __name__ == "__main__":
    unittest.main()
