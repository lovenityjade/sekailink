# SekaiLink Windows Worker

This folder contains the command surface used to drive the dedicated Windows
build/test workbench with as few remote transactions as possible.

## Shell policy

Use MSYS2/UCRT64 as the default shell for Windows workbench operations. Builds,
file staging, local test servers, Git operations, logs, and diagnostics should
run locally on the Windows Box with:

```sh
cd /d/SekaiLink/repos/sekailink-canonical
./tools/windows-worker/sekai-worker-msys.sh status
```

Use PowerShell only for Windows-native integration points such as services,
scheduled tasks, registry, firewall, optional Windows features, shortcuts, or
interactive GUI launch plumbing.

Client Core builds are an exception inside the MSYS2 worker: the worker launches
the official Windows Node/npm through `cmd.exe`. Some Vite/Tailwind native
packages can panic under the MSYS2 Node runtime with `Node-API symbol has not
been loaded`, so JavaScript packaging must use the native Windows Node install.

If Linux needs to trigger a Windows operation remotely, prefer one coarse
command instead of many small SSH commands:

```sh
ssh sekailink-windows 'C:/msys64/usr/bin/bash.exe -lc "cd /d/SekaiLink/repos/sekailink-canonical && ./tools/windows-worker/sekai-worker-msys.sh build sekaiemu Debug"'
```

Common commands:

```sh
./tools/windows-worker/sekai-worker-msys.sh doctor
./tools/windows-worker/sekai-worker-msys.sh build sekaiemu Debug
./tools/windows-worker/sekai-worker-msys.sh test sekaiemu Debug
./tools/windows-worker/sekai-worker-msys.sh build client
./tools/windows-worker/sekai-worker-msys.sh build bootloader
./tools/windows-worker/sekai-worker-msys.sh package client
./tools/windows-worker/sekai-worker-msys.sh lab sekaiemu-preview
./tools/windows-worker/sekai-worker-msys.sh lab runtime-init
./tools/windows-worker/sekai-worker-msys.sh lab runtime-doctor
./tools/windows-worker/sekai-worker-msys.sh lab runtime-list gba
./tools/windows-worker/sekai-worker-msys.sh lab runtime-yamls gba
./tools/windows-worker/sekai-worker-msys.sh lab runtime-generate gba
./tools/windows-worker/sekai-worker-msys.sh lab runtime-plan metroid_zero_mission
./tools/windows-worker/sekai-worker-msys.sh logs
```

The Windows build/client workbench root is `D:\SekaiLink`. Build logs are
written to `D:\SekaiLink\logs\worker`, and artifact metadata is written to
`D:\SekaiLink\artifacts\last-build-result.json`.

The runtime compatibility lab is separate on purpose. Use `D:\RuntimeLab` for
Archipelago test generations, YAMLs, ROM references, generated patches,
PopTracker packs, logs, status matrices, and reports. Do not put this lab state
under `D:\SekaiLink`; that path stays focused on the Windows client/build tree.

## Source Sync

The preferred sync model is one rsync from Linux to Windows, excluding generated
caches and build outputs:

```sh
rsync -az --delete --info=stats2 \
  --exclude '.git/' \
  --exclude '**/node_modules/' \
  --exclude '**/.vite/' \
  --exclude '**/.cache/' \
  --exclude '**/dist/' \
  --exclude '**/release/' \
  --exclude '**/build/' \
  --exclude '**/build-*/' \
  --exclude '**/CMakeFiles/' \
  --exclude '**/CMakeCache.txt' \
  --exclude '**/cmake_install.cmake' \
  --exclude '**/*.o' \
  --exclude '**/*.obj' \
  --exclude '**/*.pdb' \
  --exclude '**/*.ilk' \
  --exclude '**/*.tmp' \
  --exclude 'apps/client-core/tsconfig.tsbuildinfo' \
  -e "sshpass -p 'A3e5t122!' ssh -F /home/thelovenityjade/.ssh/config" \
  /home/thelovenityjade/SekaiLink/canonical/ \
  sekailink-windows:/d/SekaiLink/repos/sekailink-canonical/
```

This intentionally transfers source, assets, docs, runtime manifests, and
third-party source, while leaving regenerable build artifacts on their own side.
