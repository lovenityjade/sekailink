# SekaiLink UI/UX – ui-context.md (Codex Implementation Guide)

> Objectif: permettre à Codex de reproduire **fidèlement** les mockups (Landing + App UI + Profile Card) en HTML/CSS (et JS minimal), avec un style “glass sci‑fi” propre, lisible et **accessible aux nouveaux joueurs** (ceux qui ne connaissent pas Archipelago).

---

## 0) Assets & chemins (à respecter)

### Images (fond / ambiance)
- `./assets/img/mainpage_bg.png`  
  - Landing page (marketing) – fond vert “waves + étoiles”, **1920×1080** minimum.
- `./assets/img/green_bg.png`  
  - App UI (par défaut) – fond vert plus neutre, discret.
- `./assets/img/purple_bg.png`  
  - App UI (variante mauve) – écrans “options / YAML / advanced”.

> Les backgrounds sont des images “plates” (pas de texte/écrans intégrés). Les écrans mockup se construisent en HTML/CSS par-dessus.

### SFX (UI)
Tous les SFX sont dans:
- `./assets/sfx/`

Fichiers existants (à utiliser partout dans l’UI):
- `ui_appopen.mp3` – ouverture de l’app / navigation majeure
- `ui_confirm.mp3` – validation (OK, confirmer)
- `ui_error.mp3` – erreur (form invalide, action refusée)
- `ui_friendrequest_notification.mp3` – notification: demande d’ami
- `ui_global_notification.mp3` – notification générique (toast)
- `ui_join.mp3` – **Lobby room**: join
- `ui_leave.mp3` – **Lobby room**: leave
- `ui_ready.mp3` – **Lobby room**: ready
- `ui_unready.mp3` – **Lobby room**: unready
- `ui_success.mp3` – succès (seed créé, upload YAML, etc.)

---

## 1) Différence claire: Landing vs App UI

### Landing page (site public)
- “Marketing site”, simple, rassurant, explications rapides.
- Gros Hero (titre + sous-titre + 2 CTA).
- “What you can do” avec 3 cards.
- Nav top (Features / How it Works / Community / Status / Docs + bouton “Open App”).
- Typo grande, beaucoup d’air, très lisible.
- **Background principal:** `mainpage_bg.png`

### App UI (web app + Electron)
- “Tool UI”, dense mais clair.
- Layout avec **sidebar** (icônes + labels) + zone centrale.
- Tables, filtres, badges, états (ready/unready), actions rapides.
- Composants réutilisables (cards, panels, toasts, modals).
- **Background:** `green_bg.png` (par défaut), `purple_bg.png` pour certains écrans.

---

## 2) Typographie (Google Fonts) + hiérarchie

### Google Fonts (à charger via `<link>` dans `<head>`)
- **Londrina Solid** (branding / titres courts / logo)
- **Lexend Deca** (UI, texte, labels)

Recommandé:
```html
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Londrina+Solid:wght@400&family=Lexend+Deca:wght@300;400;500;600&display=swap" rel="stylesheet">
```

### Règles
- Logo / “SEKAILINK”: Londrina Solid, tracking léger (+0.06em).
- UI / paragraphes: Lexend Deca.
- Tailles (guideline):
  - Landing Hero H1: 56–64px (desktop), 36–44px (mobile)
  - Landing H2: 28–34px
  - App titles: 24–28px
  - App body: 14–16px
  - Small meta: 12–13px
- Line-height:
  - titres: 1.05–1.15
  - texte: 1.45–1.6

---

## 3) Design tokens (CSS variables)

Créer/centraliser des variables dans `globalStyles.css` (ou un nouveau `tokens.css` importé partout).

### Palette (thème “green glass”)
```css
:root{
  /* base */
  --bg-0: #05070A;                 /* fond sombre */
  --bg-1: rgba(10, 14, 18, 0.62);  /* glass panel */
  --bg-2: rgba(10, 14, 18, 0.78);  /* glass panel dense */
  --text-0: rgba(255,255,255,0.92);
  --text-1: rgba(255,255,255,0.76);
  --text-2: rgba(255,255,255,0.58);

  /* accents */
  --accent: rgb(92, 231, 181);     /* #5ce7b5 */
  --accent-2: rgb(64, 210, 170);
  --danger: #ff4d4d;
  --warn: #ffcc66;

  /* borders / glow */
  --border: rgba(92,231,181,0.18);
  --border-strong: rgba(92,231,181,0.30);
  --glow: 0 0 24px rgba(92,231,181,0.18);
  --shadow: 0 12px 45px rgba(0,0,0,0.55);

  /* radius / spacing */
  --r-xl: 18px;
  --r-lg: 14px;
  --r-md: 10px;
  --r-sm: 8px;

  --pad-3: 28px;
  --pad-2: 18px;
  --pad-1: 12px;
  --pad-0: 8px;

  /* blur */
  --blur: blur(16px);
}
```

### Variante “purple”
- Garder le même design, mais remplacer l’accent par un mauve doux.
```css
[data-theme="purple"]{
  --accent: rgb(190, 140, 255);
  --accent-2: rgb(150, 110, 255);
  --border: rgba(190,140,255,0.20);
  --border-strong: rgba(190,140,255,0.32);
  --glow: 0 0 26px rgba(190,140,255,0.18);
}
```

---

## 4) Effets visuels (signature SekaiLink)

### A) Background overlay (partout)
Sur `body`, superposer:
1) l’image (`mainpage_bg.png` ou `green_bg.png` ou `purple_bg.png`)
2) un vignettage doux (sombre aux bords)
3) une couche grain légère (optionnel)

```css
body{
  background: var(--bg-0);
  color: var(--text-0);
}
.page-bg{
  position: fixed;
  inset: 0;
  z-index: -2;
  background: url("/assets/img/green_bg.png") center/cover no-repeat;
}
.page-bg::after{
  content:"";
  position:absolute; inset:0;
  background:
    radial-gradient(1200px 600px at 40% 35%, rgba(92,231,181,0.12), transparent 60%),
    radial-gradient(1000px 500px at 70% 70%, rgba(92,231,181,0.08), transparent 55%),
    linear-gradient(to bottom, rgba(0,0,0,0.35), rgba(0,0,0,0.75));
}
```

### B) Glass panel
```css
.glass{
  background: var(--bg-1);
  border: 1px solid var(--border);
  backdrop-filter: var(--blur);
  -webkit-backdrop-filter: var(--blur);
  box-shadow: var(--shadow);
  border-radius: var(--r-xl);
}
.glass.dense{ background: var(--bg-2); }
```

### C) Glow focus (accessibilité)
- Toute interaction clavier doit avoir un focus visible.
```css
:focus-visible{
  outline: 2px solid rgba(92,231,181,0.85);
  outline-offset: 2px;
  box-shadow: 0 0 0 6px rgba(92,231,181,0.12);
  border-radius: 10px;
}
```

---

## 5) Composants UI (à réutiliser partout)

### 5.1 Buttons
- **Primary**: fond accent + texte sombre
- **Secondary**: glass + border
- **Ghost**: transparent, hover glow

```css
.btn{
  display:inline-flex; align-items:center; justify-content:center;
  gap:10px;
  padding: 12px 18px;
  border-radius: 12px;
  font-weight: 600;
  font-family: "Lexend Deca", system-ui;
  border: 1px solid transparent;
  transition: transform .12s ease, box-shadow .12s ease, background .12s ease;
  user-select:none;
}
.btn:active{ transform: translateY(1px); }

.btn-primary{
  background: linear-gradient(180deg, rgba(92,231,181,1), rgba(64,210,170,1));
  color: #06110D;
  box-shadow: 0 10px 35px rgba(92,231,181,0.18);
}
.btn-primary:hover{ box-shadow: 0 12px 42px rgba(92,231,181,0.24); }

.btn-secondary{
  background: rgba(10,14,18,0.55);
  color: var(--text-0);
  border-color: var(--border);
}
.btn-secondary:hover{ box-shadow: var(--glow); }

.btn-ghost{
  background: transparent;
  color: var(--text-1);
  border-color: rgba(255,255,255,0.08);
}
```

### 5.2 Inputs / Search
- Fond glass + icône à gauche + placeholder lisible.
- La recherche de Room List doit être rapide visuellement.
```css
.input{
  background: rgba(10,14,18,0.52);
  border: 1px solid rgba(255,255,255,0.10);
  border-radius: 12px;
  padding: 10px 12px;
  color: var(--text-0);
}
.input::placeholder{ color: rgba(255,255,255,0.40); }
```

### 5.3 Badge / Status Pill
- Petits pills (Ready, In progress, Online, etc.)
```css
.pill{
  display:inline-flex; align-items:center; gap:8px;
  padding: 6px 10px;
  border-radius: 999px;
  font-size: 12px;
  background: rgba(255,255,255,0.06);
  border: 1px solid rgba(255,255,255,0.10);
  color: var(--text-1);
}
.pill.ok{ border-color: rgba(92,231,181,0.35); color: rgba(92,231,181,0.95); }
.pill.warn{ border-color: rgba(255,204,102,0.35); color: rgba(255,204,102,0.95); }
.pill.danger{ border-color: rgba(255,77,77,0.35); color: rgba(255,77,77,0.95); }
```

### 5.4 Toast notifications
- À droite en haut (App).
- Son: `ui_global_notification.mp3` (et friend request: `ui_friendrequest_notification.mp3`).
- Auto-dismiss 4–6s + hover pause.

---

## 6) Landing page – Spécification (mockup “Host Archipelago seeds without the pain.”)

### 6.1 Structure
`landing.html` (ou route `/`) doit suivre:

1) **Top Nav** (sticky léger)
   - Left: “SEKAILINK” logo
   - Center links: Features / How It Works / Community / Status / Docs
   - Right: bouton `Open App` (primary)
2) **Hero section** (grid 2 colonnes)
   - Left: H1 (2 lignes) + tagline + 2 boutons (Open App / Create Account)
   - Right: zone “faux screenshot” optionnelle (si on veut), mais **dans la version background-only**, on garde l’espace vide / illustration légère.
3) **What You Can Do** (3 cards)
   - Generate seeds from YAMLs
   - Manage rooms with friends
   - Track, patch, and play together
4) CTA bas (bouton Create Account)
5) Footer minimal

### 6.2 Background
Landing utilise `mainpage_bg.png`.
- Ajouter un voile sombre + glow discret derrière le Hero pour la lisibilité.

### 6.3 Layout (desktop)
- Max width: 1200–1280px
- Hero padding top: ~110px
- Buttons: larges, 48px de hauteur min.

### 6.4 Responsive
- < 900px: Hero passe en 1 colonne, boutons stacked.

### 6.5 UX “nouveau joueur”
Inclure en dessous du tagline un micro-copy (1 ligne):
- “No need to learn server setup — SekaiLink handles generation, hosting and tracking.”

---

## 7) App UI – Layout global (mockups Room List + Friends)

### 7.1 Base shell
Pages app (authenticated) doivent utiliser un layout commun:

- **Sidebar** (left, 220px desktop)
  - SekaiLink logo en haut
  - Items:
    - Dashboard
    - Room List
    - Create YAML
    - Friends
    - Admin (si rôle)
  - Un item actif = highlight (fond + glow accent)
- **Main content**
  - Header interne optionnel (titre page + actions)
  - Contenu dans un grand panel glass
- **Background**: `green_bg.png` par défaut

### 7.2 Spacing & density
- Large panel central a un padding 18–24px
- Les listes (rooms, friends) ont des rows de 56–64px.

---

## 8) Room List page – Spécification (isolé en HD)

Page: `Room List`

### 8.1 Header / actions
Dans le panel central:
- Titre “Room List”
- Barre de recherche à gauche
- À droite: filtres (pills / dropdowns):
  - Status (Any / Ready / In progress)
  - Mode (Async / Sync)
  - Tags (casual, hardcore, etc.)
  - Ping (optionnel)
- Bouton principal en bas à gauche: **Create Room** (primary)

### 8.2 Table/rows
Chaque room row:
- Left: avatar/logo + room name
- Subline: seed info + host name + version
- Columns:
  - Players (ex: 0 / 6)
  - Ready state
  - Mode
  - Progress
  - Actions (icônes):
    - View / Join
    - Copy link
    - More (…)
- Hover row: glow léger + background plus clair

### 8.3 Empty state (important)
Si aucune room:
- Message simple + bouton “Create Room”
- Mini explication: “A room is where players meet, share settings and track progress.”

### 8.4 Sounds
- Click “Create Room” → `ui_confirm.mp3`
- Join room success → `ui_success.mp3`
- Join room fail → `ui_error.mp3`

---

## 9) Lobby Room page (où `join/leave/ready/unready` s’appliquent)

### 9.1 Objectif
Une page “hub” de la room, où les joueurs:
- rejoignent (join) / quittent (leave)
- togglent ready/unready
- voient la liste des joueurs + statuts
- voient le seed status (generated / hosting / released)
- actions: “Release items”, “Start session”, “Open tracker”, etc. (selon permissions)

### 9.2 Audio mapping (obligatoire)
- Join → `ui_join.mp3`
- Leave → `ui_leave.mp3`
- Ready → `ui_ready.mp3`
- Unready → `ui_unready.mp3`
- Action success (release, generate) → `ui_success.mp3`
- Error → `ui_error.mp3`

---

## 10) Friends / DMs (inspiration: “Facebook + LoL baby”, sans copier Discord)

### 10.1 Layout (3 colonnes)
- Colonne A (left, 260px): **Friends list**
  - Search “Find or start a conversation”
  - Tabs: Friends / Requests / Blocked (optionnel)
  - Chaque friend row: avatar + name + status (Online/Away/Offline) + small note (current game/room)
- Colonne B (center): **Conversation view**
  - Header: friend name + actions (call icon placeholder / invite to room)
  - Message list (bubbles très soft, pas style Discord):
    - Bubbles arrondies 16px, background ~ rgba(255,255,255,0.06)
    - L’heure en petit, compact
  - Composer: input + attach (future)
- Colonne C (right, 320px): **Profile / quick actions**
  - Profile card “floating” (voir section 11)
  - “Invite to Room”, “View rooms”, “Mutual friends”, etc.

### 10.2 Différenciation vs Discord
- Pas de “server list” à gauche type Discord.
- Nav principale reste dans la sidebar SekaiLink.
- Dans Friends: style plus “card-based”, plus “social panel” que “chat app”.

### 10.3 SFX
- Nouvelle notification DM → `ui_global_notification.mp3`
- Friend request → `ui_friendrequest_notification.mp3`

---

## 11) Profile Card (style “Discord-ish” mais différent)

### 11.1 Principes
- Card verticale dans un panel glass, bordures accent + coins plus “sci-fi frame”.
- Header = bannière image (optionnel), mais simple.
- Avatar grand + status dot.
- Boutons: Message / Add friend / More.

### 11.2 Sections
- About me (texte court)
- Member since (date)
- Favorite games (mini covers + “+N”)
- Mutual servers/rooms (texte + chevron)

### 11.3 Différences de design (pour éviter “trop Discord”)
- Utiliser des **panneaux rectangulaires** imbriqués (pas de grosses cartes rondes partout).
- Garder une frame sci‑fi (petits angles, liseré lumineux).
- Typo plus “product / tool” (Lexend) au lieu du look Discord.

---

## 12) Boot / Loading screen (CSS animation propre)

### 12.1 Où
- Web app: sur les transitions “cold start” (loading session / auth).
- Electron: au boot avant de charger la webview.

### 12.2 Look
- Fond = `green_bg.png` (ou purple si thème).
- Center: Logo “SEKAILINK”
- Sous-texte: “Connecting to SekaiLink…” (ou “Checking session…”)
- Animation CSS clean:
  - 3 petites barres lumineuses qui pulsent
  - OU ring spinner + particules qui drift lentement

### 12.3 CSS (guideline)
- Durée anim: 1.2–1.6s loop
- Easing: ease-in-out
- Pas de flashy agressif (accessibilité)

### 12.4 Son (option)
- Au moment où l’app devient interactive: `ui_appopen.mp3` (faible volume)

---

## 13) Electron – Update / Download screen

### 13.1 Écran requis
Avant de charger l’app:
- “Checking for updates…”
- Si update:
  - “Downloading update vX.Y.Z”
  - Progress bar
  - Vitesse / taille (option)
  - Bouton “Retry” si fail
- Si up-to-date:
  - “You’re up to date” puis auto transition vers app.

### 13.2 Design
- Même identité: glass panel centré, background green/purple.
- Progress bar accent.
- Une ligne “Release notes” collapsible (option future).

### 13.3 Son
- Success: `ui_success.mp3`
- Error / retry: `ui_error.mp3`
- Transition vers app: `ui_appopen.mp3`

---

## 14) Accessibilité (non‑négociable)

- Contraste: texte principal toujours > ~4.5:1 sur panel.
- Navigation clavier complète.
- Focus visible (déjà défini).
- Boutons min-height 44px (mobile), target size.
- `aria-label` sur icônes-only buttons.
- Éviter animations rapides / clignotantes.

---

## 15) Pages minimum à styliser (MVP)

### Public
- Landing (`/`)
- Login / Create account (si page distincte)

### App
- Dashboard (cards rapides)
- Room List
- Room Lobby (join/leave/ready)
- Create YAML (UI sliders / options, peut être “purple theme”)
- Friends / DMs + Profile Card (intégré)

---

## 16) Implementation notes (Codex)
1) Refactoriser CSS vers tokens (variables) + composants communs.
2) Landing: mettre `mainpage_bg.png` + hero/CTA conformes au mockup.
3) App: layout sidebar + panel central + Room List conforme.
4) Ajouter un mini “audio manager” JS:
   - Preload des mp3
   - `playSfx(name, volume=0.35)`
   - Respecter “mute” setting dans user prefs
5) Ajouter Boot screen CSS + Electron update screen (template HTML séparé côté Electron).

---

## 17) Naming / conventions
- Classes: `kebab-case` (ex: `room-list`, `profile-card`)
- BEM optionnel si utile (ex: `room-row__actions`)
- Utiliser des composants partials (templates) quand possible:
  - `sidebar.html`, `topbar.html`, `toast.html`, `modal.html`

---

## 18) Quick visual checklist (à valider)
- Glass panels: blur + border + shadow
- Accent glow discret, jamais néon agressif
- Texte lisible, pas “techno illisible”
- UI claire pour newbies (micro‑copy + empty states)
- SFX mappés aux bons événements (join/leave/ready)

