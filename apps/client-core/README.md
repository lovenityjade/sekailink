# SekaiLink Core (Electron + React)

This directory is again the active source of truth for the `SekaiLink Core` desktop shell.

Core responsibilities:

- authentication
- account UX
- social / rooms / lobby
- module installation and updates
- standard module launch orchestration

It is not the source of truth for game runtime logic. That responsibility belongs to native `LinkedWorld` modules.

## Dev (Linux)
```
cd /opt/sekailink/client/app
cp .env.example .env
# set VITE_API_BASE_URL to your SekaiLink host
npm install
npm run electron:dev
```

## Notes
- The renderer uses `fetch` with `credentials: "include"` and Socket.IO with `withCredentials: true`.
- Set `VITE_API_BASE_URL=https://sekailink.com` for file:// builds.
- If CORS blocks requests from file://, use the hosted web origin or add a dedicated desktop origin/CORS rule.
