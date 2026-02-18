# SekaiLink Dev Setup

## 1. Prerequisites
- Node.js `20.x`
- npm `10+`
- Python `3.11` (or `3.12`) with `pip`
- Git

Optional (only if you work on desktop runtime/emulator flows):
- Mono (Linux) for BizHawk runtime checks
- system dependencies required by Electron on your distro

## 2. Clone and install
From repo root:

```bash
cd client/app
npm ci
```

Install minimum Python deps for wrappers/smoke:

```bash
cd /path/to/sekailink
python -m pip install --upgrade pip
pip install colorama websockets PyYAML jellyfish jinja2 schema platformdirs certifi typing_extensions
```

## 3. Run app locally
UI only:

```bash
cd client/app
npm run dev
```

Electron desktop:

```bash
cd client/app
npm run electron:dev
```

UI prototype:

```bash
cd client/app
npm run electron:dev:ui-prototype
```

## 4. Quality checks
From `client/app`:

```bash
npm run lint
npm run build
```

Headless smoke from repo root:

```bash
bash scripts/headless-smoke.sh --mode integration --wrapper common
```

If local environment blocks sockets, run:

```bash
bash scripts/headless-smoke.sh --mode compat --wrapper common
```

## 5. Git hooks
Pre-commit runs `lint-staged` (eslint + prettier on staged files).

If hooks are missing:

```bash
cd client/app
npm run prepare
```

## 6. Crash diagnostics (opt-in)
- In client settings, enable `Crash reporting (opt-in)`.
- Use `Send diagnostics now` to:
  - collect environment + recent log tail
  - copy the payload to clipboard
  - upload it when opt-in is enabled
