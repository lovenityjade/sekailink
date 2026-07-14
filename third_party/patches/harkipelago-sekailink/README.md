# Harkipelago SekaiLink overlay

`0001-sekailink-managed-launch.patch` adds the managed Client Core launch
contract to the upstream Harkipelago source. Apply it through:

```bash
scripts/prepare-harkipelago-sekailink.sh /path/to/harkipelago-source
```

The preparation script also installs the repository's known-compatible
`apclientpp` 0.6.4 header and the GCC 16 `<cstdint>` compatibility fix.

Distribution builds must use an x86-64 toolchain end to end. In particular,
SDL2, SDL2_net, Ogg, Vorbis, Opus, and Opusfile must all be ELF64. Build the
portable Linux artifact in an Ubuntu 22.04 x86-64 container on Gaming PC;
building directly on Nobara currently introduces a `GLIBC_2.43` requirement.
