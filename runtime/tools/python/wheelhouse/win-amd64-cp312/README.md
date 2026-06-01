SekaiLink Windows patcher wheelhouse
====================================

This directory is shipped inside the desktop runtime for Windows x64 builds.
It contains the Python 3.12 wheels required by the private AP/MWGG patcher
adapter.

Rules:

- Packaged Windows builds must install patcher dependencies from this directory
  with `pip --no-index --find-links`.
- Packaged Windows builds must not depend on a user-installed Python.
- Packaged Windows builds must not download Python packages while patching.
- Linux/macOS can use their platform-specific private runtimes later; this
  wheelhouse exists to make the Windows Pre-BETA3 path self-contained first.

This is intentionally a progress path, not a shortcut: the game-specific patch
logic stays in the AP/MWGG world modules, while SekaiLink owns the private
runtime packaging so players do not need to install Python.
