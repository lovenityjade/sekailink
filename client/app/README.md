# SekaiLink Desktop Client (Electron + React)

## Dev (Linux)
```
cd /opt/multiworldgg/client/app
cp .env.example .env
# set VITE_API_BASE_URL to your SekaiLink host
npm install
npm run electron:dev
```

## Notes
- The renderer uses `fetch` with `credentials: "include"` and Socket.IO with `withCredentials: true`.
- Set `VITE_API_BASE_URL=https://sekailink.com` for file:// builds.
- If CORS blocks requests from file://, use the hosted web origin or add a dedicated desktop origin/CORS rule.
