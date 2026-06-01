# External Dependencies

This directory holds small vendored dependencies that are required to build `Sekaiemu`
without assuming a local checkout of unrelated projects.

## Current Contents

- `libretro/include/libretro.h`
  - minimal libretro frontend API header used by the runtime

## Policy

- keep vendored code small and explicit
- prefer system libraries for large dependencies like `SDL2` and `OpenGL`
- document licensing for vendored files in `../LICENSES/`
