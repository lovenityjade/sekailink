# UI-PLAN.md

Goal: ship a clean, coherent SekaiLink UI for beta that matches `ui-context.md` and keeps work ordered so we do not lose context.

## 0) Ground Rules (always)
- Follow `ui-context.md` as the source of truth for look/feel.
- Keep UI text in English.
- Prioritize accessibility (contrast, focus-visible, min target sizes).
- Prefer shared components and tokens over ad-hoc CSS.

## 1) Inventory + Baseline
- Verify which pages use the new app shell and which still use legacy layouts.
- Audit templates and note which ones must be updated for beta:
  - `WebHostLib/templates/landing.html`
  - `WebHostLib/templates/lobby.html`
  - `WebHostLib/templates/hostRoom.html`
  - `WebHostLib/templates/generate.html`
  - `WebHostLib/templates/account.html`
  - `WebHostLib/templates/admin.html`
  - `WebHostLib/templates/help.html`
  - `WebHostLib/templates/pageWrapper.html`
  - `WebHostLib/templates/header/baseHeader.html`
- Audit CSS: tokens, globals, layout, and per-page styles.

## 2) Design Tokens + Base Theme (critical)
- Create/confirm design tokens (colors, radii, spacing, blur) in `WebHostLib/static/styles/globalStyles.css`.
- Add theme variants (`data-theme="purple"`) for advanced/YAML screens.
- Add base components (glass, buttons, inputs, pills, toasts) using tokens.

## 3) App Shell + Navigation
- Implement consistent app shell (sidebar + main panel) for authenticated pages.
- Ensure header/nav includes Admin when role is admin.
- Confirm background image layer (`green_bg.png` or `purple_bg.png`) on app pages.

## 4) Landing Page (public)
- Build landing hero per `ui-context.md` (nav, hero, 2 CTA, 3 cards, CTA footer).
- Ensure background uses `mainpage_bg.png` with overlay + glow.
- Add beginner-friendly micro-copy.

## 5) Room List (lobby list)
- Align to the Room List spec (search, filters, primary CTA, row layout).
- Implement empty state.
- Ensure fast scanning and clear status badges.

## 6) Lobby Room UI
- Layout: panels for chat, members, room info, settings, terminal, tracker.
- Ensure readiness + release UI is clear for new users.
- Confirm SFX mapping and feedback toasts.

## 7) Friends + DMs
- Three-column layout as in `ui-context.md`.
- Profile card style distinct from Discord.
- Toasts for friend events and DMs.

## 8) Admin Panel
- Ensure admin panels are readable, consistent, and use the shared components.
- Use glass panels, table layout, and clear action states.

## 9) Help / Support
- Style the help page to match app theme.
- Ensure the support ticket form is clear and accessible.

## 10) Audio System (SFX)
- Add or confirm a central audio manager (preload SFX, respect mute).
- Map SFX to lobby events (join/leave/ready/unready, success, error).

## 11) Boot / Loading Screen
- Create a simple boot screen template with glass panel and animated loader.
- Tie to app startup and large transitions (if relevant).

## 12) Polish + QA
- Cross-check typography, spacing, and contrast.
- Ensure keyboard navigation works across all UI.
- Verify responsive breakpoints (mobile and tablet).

## 13) Beta Freeze Checklist
- All critical pages visually consistent.
- No broken layouts on main flows (login, lobby, generate, admin, help).
- No obvious contrast or focus issues.
- SFX are correct and not spammy.
- UI strings are in English.

