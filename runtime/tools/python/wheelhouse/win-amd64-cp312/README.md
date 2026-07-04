SekaiLink Windows Python wheelhouse
===================================

This directory is shipped inside the desktop runtime for Windows x64 builds.
It contains the Python 3.12 wheels required by the private AP/MWGG patcher and
Archipelago client wrapper adapter.

Rules:

- Build/release packaging may use this directory to prepare the bundled Python
  runtime or a prebuilt venv.
- Packaged Windows builds must not depend on a user-installed Python.
- Packaged Windows builds must not download Python packages while patching or
  launching client wrappers.
- Packaged Windows builds must not run a first-launch dependency installer for
  players. If the bundled Python runtime cannot import required modules, the
  build is invalid.
- Linux/macOS can use their platform-specific private runtimes later; this
  wheelhouse exists to make the Windows Pre-BETA3 path self-contained first.

This is intentionally a progress path, not a shortcut: the game-specific patch
logic stays in the AP/MWGG world modules, while SekaiLink owns the private
runtime packaging so players do not need to install Python.

As of 2026-06-17 this wheelhouse also includes custom-world wrapper extras such
as `docutils`, `dolphin-memory-engine`, and `pyevermizer`; these are release
requirements for zero-friction APWorld/client compatibility, not optional user
installs.
