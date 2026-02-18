# WINDOWS-BUILD.md

## Windows ZIP build (from Linux)

### Prereqs
- Wine must be installed (needed by electron-builder on Linux).
- Node deps installed in `client/app`.

### Commands
```
cd client/app
npm run build
npx electron-builder --win --x64 -c.win.target=zip
```

### Output
- `client/app/release/SekaiLink-client-alpha-0.0.1.zip`

### Upload to VPS
```
scp client/app/release/SekaiLink-client-alpha-0.0.1.zip \
  root@sekailink.com:/opt/multiworldgg/WebHostLib/static/downloads/
```

### Public URL
```
https://sekailink.com/static/downloads/SekaiLink-client-alpha-0.0.1.zip
```

## Notes
- If electron-builder fails with “wine is required”, install via:
```
sudo dnf -y install wine
```
- Build requires network access to download Electron win32 artifacts.
