# UI Implementation Notes — SekaiLink

## Overview

Full UI/UX implementation aligned to `ui-context.md`, `CONTEXT.md`, and `TODO.md`. All design system tokens, layouts, components, and assets follow the spec mockups (`mainpage_example.png`, `profilecard_example.png`, `differentwindows_ui_example.png`).

---

## Pages Modified/Added

| Page | Template | CSS | Status |
|------|----------|-----|--------|
| **Landing** | `templates/landing.html` | `styles/landing.css` | Rewritten |
| **Boot/Loading** | `templates/boot.html` | `styles/boot.css` | Rewritten |
| **Room List** | `templates/roomList.html` | `styles/roomList.css` | Rewritten |
| **Lobby** | `templates/lobby.html` | `styles/lobby.css` | Updated (sidebar icons, SFX hooks) |
| **Admin** | `templates/admin.html` | `styles/admin.css` | Updated (sidebar icons, Settings tab) |
| **Electron Update** | `templates/electronUpdate.html` | `styles/electronUpdate.css` | **New** |
| **Global Styles** | — | `styles/globalStyles.css` | Rewritten |
| **App Shell** | — | `styles/appShell.css` | Rewritten |
| **Social/Friends** | (in `pageWrapper.html`) | `styles/social.css` | Rewritten |

## Backend Changes

| File | Change |
|------|--------|
| `WebHostLib/misc.py` | Added `/assets/<path:filename>` route to serve root `assets/` dir |
| `WebHostLib/templates/pageWrapper.html` | Added Google Fonts preconnect + viewport meta |

---

## Asset Paths

### Images (served via `/assets/` route)
- `/assets/img/mainpage_bg.png` — Landing page background
- `/assets/img/green_bg.png` — App UI background (default)
- `/assets/img/purple_bg.png` — DM/Profile variant background

### Sound Effects (served via `/assets/` route)
All SFX in `/assets/sfx/`:

| File | Key | Usage |
|------|-----|-------|
| `ui_appopen.mp3` | `appopen` | Boot screen on load |
| `ui_confirm.mp3` | `confirm` | Button clicks, form submit, chat send |
| `ui_error.mp3` | `error` | Generation error, validation failure |
| `ui_friendrequest_notification.mp3` | `friendrequest` | Friend request received |
| `ui_global_notification.mp3` | `global` | Toast notifications |
| `ui_join.mp3` | `join` | Player joins lobby |
| `ui_leave.mp3` | `leave` | Player leaves lobby |
| `ui_ready.mp3` | `ready` | Player marks ready |
| `ui_unready.mp3` | `unready` | Player marks unready |
| `ui_success.mp3` | `success` | Generation success, update complete |

### SFX Manager (`static/assets/sfx.js`)
- Anti-spam cooldown: 120-250ms per sound
- `prefers-reduced-motion`: suppresses non-essential sounds
- Mute toggle: persisted in `localStorage` (`skl_sfx_muted`)
- Volume: persisted in `localStorage` (`skl_sfx_volume`), default 0.3
- API: `window.SKL_SFX.play(name, volume)`, `.toggleMuted()`, `.getMuted()`, `.setMuted(bool)`, `.getBaseVolume()`, `.setBaseVolume(float)`, `.preload()`

---

## Interaction → SFX Mapping

| Interaction | SFX Key | Location |
|-------------|---------|----------|
| Boot screen load | `appopen` | `boot.html` |
| Create Room button | `confirm` | `roomList.html` |
| Ready toggle | `ready` / `unready` | `lobby.html` inline script |
| Generate button | `confirm` | `lobby.html` inline script |
| Chat send | `confirm` | `lobby.html` inline script |
| Terminal send | `confirm` | `lobby.html` inline script |
| Player joins lobby | `join` | `lobby.html` via `skl:lobby-joined` event |
| Player leaves lobby | `leave` | `lobby.html` via `skl:lobby-left` event |
| Generation success | `success` | `lobby.html` via `skl:generation-success` event |
| Generation error | `error` | `lobby.html` via `skl:generation-error` event |
| Friend request notification | `friendrequest` | `social.js` |
| Global notification | `global` | `social.js` |
| Update complete (Electron) | `success` | `electronUpdate.html` |

---

## How to View Each Page

| Page | URL |
|------|-----|
| Landing | `/` |
| Boot | `/boot` (redirects to `/rooms` after 1.2s) |
| Room List | `/rooms` |
| Lobby | `/lobby/<suuid>` (requires existing lobby) |
| Admin | `/admin` (requires admin role) |
| Electron Update | `/electron-update` (needs route; currently template-only) |

---

## Design System Summary

### CSS Variables (`:root`)
```
--bg-0: #05070A
--bg-1: rgba(10,14,18,0.62)
--bg-2: rgba(10,14,18,0.78)
--text-0/1/2: white at 92%/76%/58% opacity
--accent: rgb(92,231,181) (#5ce7b5)
--accent-2: rgb(64,210,170)
--border: rgba(92,231,181,0.18)
--border-strong: rgba(92,231,181,0.30)
--r-xl/lg/md/sm: 18/14/10/8px
--blur: blur(16px)
--font-brand: "Sora"
--font-ui: "Inter"
--font-mono: "Courier New", monospace
```

### Purple Variant (`[data-theme="purple"]`)
Overrides `--accent` to `rgb(190,140,255)` and related border/glow values.

### Glass Component (`.glass`)
- Background: `var(--bg-1)` with `backdrop-filter: blur(16px)`
- Border: `1px solid var(--border)`
- Grain: SVG feTurbulence noise via `::before` pseudo-element at 2.5% opacity

### Fonts
- **Google Fonts**: Loaded via `<link>` in `pageWrapper.html` (`Sora` + `Inter`)
- **Local fallbacks**: TTF files in `static/static/fonts/` (legacy fallbacks)

---

## Visual Checklist (ui-context.md compliance)

- [x] **Design tokens**: CSS variables for colors, spacing, radius, blur, typography
- [x] **Glass panels**: backdrop-filter + border + grain noise overlay
- [x] **Page backgrounds**: `mainpage_bg.png` (landing), `green_bg.png` (app), `purple_bg.png` (DM/profile)
- [x] **Topbar**: 72px sticky, glass backdrop, brand + nav + CTA
- [x] **Hero section**: Headline, subtitle, dual CTA buttons, mockup illustration
- [x] **Feature cards**: 3-column grid, glass style, icon + title + description
- [x] **Footer**: Brand + version + "Powered by Archipelago" + links
- [x] **App Shell sidebar**: 220px sticky, brand, nav links with SVG icons, mute toggle
- [x] **Room List**: Search bar + filter chips + table-style rows + empty state + create modal
- [x] **Lobby**: Hero banner + chat panel + players panel + terminal + settings + modals
- [x] **Friends drawer**: Slide-from-left panel, friend rows with avatars + status dots
- [x] **DM panel**: 2-column layout (chat + profile card)
- [x] **Profile card**: Banner + avatar (neon frame) + badges + About + Games + Mutuals sections
- [x] **Boot screen**: Scanline sweep + shimmer overlay + progress dots + version label
- [x] **Electron Update**: Progress bar + percentage + changelog + restart button
- [x] **Loading overlay**: Reusable `.skl-loading-overlay` component in globalStyles
- [x] **SFX manager**: Cooldown, reduced-motion, mute toggle, localStorage persistence
- [x] **SFX wiring**: All specified interactions trigger correct sounds
- [x] **Sidebar icons**: All sidebar links have SVG icons in Room List, Lobby, and Admin
- [x] **Mute toggle**: In sidebar with visual state sync
- [x] **Responsive**: All pages handle mobile breakpoints (1024/900/720/640px)
- [x] **prefers-reduced-motion**: Animations disabled, SFX limited
- [x] **Admin Settings tab**: Timeout configuration UI affordance (placeholder, endpoint not yet wired)
- [x] **Assets route**: Flask route `/assets/<path:filename>` serves img and sfx files
- [x] **No external assets**: Only Google Fonts loaded externally
- [x] **Spec palette**: Only ui-context.md colors used, no improvisation
