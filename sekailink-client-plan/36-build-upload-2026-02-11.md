# Build + Upload sprint (2026-02-11)

## Objectif
Construire les binaires client Linux + Windows puis les publier sur le VPS dans `WebHostLib/static/downloads`.

## Builds exécutés
- Dossier: `client/app`
- Commande Linux: `npm run electron:pack`
- Commande Windows: `npm run electron:pack:win`

## Artefacts générés
- `client/app/release/SekaiLink-client-0.0.2-beta.0.2-topaz.AppImage`
- `client/app/release/SekaiLink-client-0.0.2-beta.0.2-topaz.exe`
- `client/app/release/SekaiLink-client-0.0.2-beta.0.2-topaz.exe.blockmap`

## Checksums locaux
- `848fe67facf581328c08369a610461220acb092d9bffa2f59efe72a34f5cb960  SekaiLink-client-0.0.2-beta.0.2-topaz.AppImage`
- `e3f4bc6e3c8c9365c82ccceffc9ba6bdf38a813b7dba4833f7a9d29e27893f52  SekaiLink-client-0.0.2-beta.0.2-topaz.exe`

## Upload VPS
- Cible: `/opt/multiworldgg/WebHostLib/static/downloads/`
- Méthode: `scp` (root@sekailink.com)

## Checksums vérifiés côté serveur
- `848fe67facf581328c08369a610461220acb092d9bffa2f59efe72a34f5cb960  SekaiLink-client-0.0.2-beta.0.2-topaz.AppImage`
- `e3f4bc6e3c8c9365c82ccceffc9ba6bdf38a813b7dba4833f7a9d29e27893f52  SekaiLink-client-0.0.2-beta.0.2-topaz.exe`
- `3ce78bf40bd6ec57c1cf809bede540993b53f7c760fd8f8798cafde2cf602147  SekaiLink-client-0.0.2-beta.0.2-topaz.exe.blockmap`

## État final
Publication terminée. Les binaires Linux/Windows actuels sont disponibles sur le VPS dans `static/downloads`.
