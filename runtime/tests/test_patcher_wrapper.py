import importlib.util
from pathlib import Path
import unittest


WRAPPER_PATH = Path(__file__).resolve().parents[1] / "patcher_wrapper.py"
SPEC = importlib.util.spec_from_file_location("sekailink_patcher_wrapper", WRAPPER_PATH)
assert SPEC is not None and SPEC.loader is not None
PATCHER_WRAPPER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(PATCHER_WRAPPER)


class DirectRomOverrideTests(unittest.TestCase):
    def test_firered_patch_targets_firered_setting(self) -> None:
        self.assertEqual(
            PATCHER_WRAPPER._direct_rom_override("player.apfirered", "base.gba"),
            ("pokemon_frlg_settings", "firered_rom_file"),
        )

    def test_leafgreen_patch_targets_leafgreen_setting(self) -> None:
        self.assertEqual(
            PATCHER_WRAPPER._direct_rom_override("player.apleafgreen", "base.gba"),
            ("pokemon_frlg_settings", "leafgreen_rom_file"),
        )

    def test_unrelated_patch_has_no_special_override(self) -> None:
        self.assertIsNone(PATCHER_WRAPPER._direct_rom_override("player.apemerald", "base.gba"))


if __name__ == "__main__":
    unittest.main()
