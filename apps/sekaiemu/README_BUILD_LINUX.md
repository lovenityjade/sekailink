# Building Sekaiemu On Linux

This document describes the current Linux build path for `Sekaiemu`.

## Scope

This build guide covers the current production-oriented runtime worktree:

- SDL2 windowing, input, and audio
- software rendering
- `OpenGL` hardware rendering
- libretro core loading

It does not yet cover packaging, installers, or distribution-specific bundles.

## Dependencies

You need:

- `cmake` 3.20 or newer
- a C++20 compiler
  - `gcc` or `clang`
- `pkg-config`
- `SDL2` development files
- `OpenGL` development files
- `make` or `ninja`

### Fedora / Nobara

Recommended packages:

```bash
sudo dnf install \
  cmake \
  gcc-c++ \
  make \
  pkgconf-pkg-config \
  SDL2-devel \
  mesa-libGL-devel
```

If you prefer `clang` and `ninja`:

```bash
sudo dnf install clang ninja-build
```

## Vendored Headers

`Sekaiemu` currently vendors the minimal `libretro` API header under:

- `extern/libretro/include/libretro.h`

This removes the old dependency on a local `RetroArch` checkout just to build the runtime.

## Build

From the project root:

```bash
cmake -S . -B build
cmake --build build -j$(nproc)
```

With `Ninja`:

```bash
cmake -S . -B build -G Ninja
cmake --build build
```

## Output

The runtime binary is produced at:

```text
build/sekaiemu_libretro_spike
```

## Notes

- `OpenGL` is currently the first hardware-render backend.
- `Vulkan` has an architectural slot in the codebase but is not yet a completed production backend.
- Core `.so` files are not bundled by this worktree.
