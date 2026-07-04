const SOCIAL_API_BASE = "https://sekailink.com/api/social";
const NOTIFICATION_PREFS_KEY = "sekailink_notification_prefs";

const panelState = {
  me: null,
  security: null,
  sessions: [],
  gameKeys: [],
  patreon: null,
  avatarEditor: {
    image: null,
    imageUrl: null,
    scale: 1,
    offsetX: 0,
    offsetY: 0,
    dragging: false,
    dragStartX: 0,
    dragStartY: 0,
    dragOriginX: 0,
    dragOriginY: 0
  }
};

function $(id) {
  return document.getElementById(id);
}

function text(id, value) {
  const node = $(id);
  if (node) {
    node.textContent = value;
  }
}

function badge(id, label, variant = "") {
  const node = $(id);
  if (!node) {
    return;
  }
  node.textContent = label;
  node.className = `badge ${variant}`.trim();
}

function note(id, message, kind = "info") {
  const node = $(id);
  if (!node) {
    return;
  }
  const colors = {
    info: "var(--muted)",
    success: "#34d399",
    warning: "#fbbf24",
    error: "#fca5a5"
  };
  node.textContent = message || "";
  node.style.color = colors[kind] || colors.info;
}

function makeDefaultAvatar(user) {
  const seed = (user?.username || user?.display_name || "SL").trim();
  const initials = seed
    .split(/[\s_-]+/)
    .filter(Boolean)
    .slice(0, 2)
    .map((part) => part[0]?.toUpperCase() || "")
    .join("") || "SL";

  let hash = 0;
  for (let i = 0; i < seed.length; i += 1) {
    hash = ((hash << 5) - hash) + seed.charCodeAt(i);
    hash |= 0;
  }
  const hue = Math.abs(hash) % 360;
  const hue2 = (hue + 48) % 360;
  const svg = `
    <svg xmlns="http://www.w3.org/2000/svg" width="512" height="512" viewBox="0 0 512 512">
      <defs>
        <linearGradient id="g" x1="0%" y1="0%" x2="100%" y2="100%">
          <stop offset="0%" stop-color="hsl(${hue} 74% 58%)"/>
          <stop offset="100%" stop-color="hsl(${hue2} 78% 44%)"/>
        </linearGradient>
      </defs>
      <rect width="512" height="512" rx="144" fill="#071017"/>
      <rect x="24" y="24" width="464" height="464" rx="128" fill="url(#g)" opacity="0.95"/>
      <circle cx="256" cy="190" r="102" fill="rgba(255,255,255,0.18)"/>
      <path d="M126 420c26-73 81-110 130-110s104 37 130 110" fill="rgba(255,255,255,0.18)"/>
      <text x="50%" y="57%" dominant-baseline="middle" text-anchor="middle"
            font-family="Outfit, Arial, sans-serif" font-size="136" font-weight="700" fill="white">${initials}</text>
    </svg>`;
  return `data:image/svg+xml;charset=utf-8,${encodeURIComponent(svg)}`;
}

function resolvedAvatar(user) {
  return user?.avatar_url || makeDefaultAvatar(user);
}

function setAvatarTargets(user) {
  const src = resolvedAvatar(user);
  ["topbar-avatar", "overview-avatar", "profile-avatar"].forEach((id) => {
    const node = $(id);
    if (node) {
      node.src = src;
    }
  });
}

function itemMetaHtml(lines) {
  return lines.filter(Boolean).map((line) => `<div class="item-meta">${line}</div>`).join("");
}

function escapeHtml(value) {
  return String(value ?? "")
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll("\"", "&quot;")
    .replaceAll("'", "&#39;");
}

function formatDate(value) {
  if (!value) {
    return "-";
  }
  try {
    return new Date(value).toLocaleString("fr-CA", {
      dateStyle: "medium",
      timeStyle: "short"
    });
  } catch (_error) {
    return value;
  }
}

function sessionDisplayName(session) {
  const device = session?.device?.display_name || session?.client_summary || "Session";
  if (session?.session_state === "active") {
    return device;
  }
  return `${device} (${session.session_state || "session"})`;
}

async function socialRequest(path, options = {}) {
  const headers = { ...(options.headers || {}) };
  const token = Auth.getToken();
  if (token) {
    headers.Authorization = `Bearer ${token}`;
  }
  if (options.body && !headers["Content-Type"]) {
    headers["Content-Type"] = "application/json";
  }
  try {
    const response = await fetch(`${SOCIAL_API_BASE}${path}`, { ...options, headers });
    const raw = await response.text();
    const contentType = response.headers.get("content-type") || "";
    const data = raw && contentType.includes("application/json") ? JSON.parse(raw) : { ok: false, raw };
    return { status: response.status, data };
  } catch (_error) {
    return { status: 0, data: { ok: false, error: "network_error" } };
  }
}

function notificationsPrefs() {
  return safeJsonParse(localStorage.getItem(NOTIFICATION_PREFS_KEY)) || {
    account: true,
    social: true,
    sekaiemu: true
  };
}

function saveNotificationPrefs(nextPrefs) {
  localStorage.setItem(NOTIFICATION_PREFS_KEY, JSON.stringify(nextPrefs));
}

async function refreshAccountData() {
  const meResult = await Auth.request("/me");
  if (!(meResult.status === 200 && meResult.data?.ok)) {
    await Auth.logout("login.html");
    return false;
  }
  Auth.setAuthenticatedFromPayload(meResult.data);
  panelState.me = meResult.data;

  const securityResult = await Auth.request("/me/security");
  panelState.security = securityResult.status === 200 && securityResult.data?.ok ? securityResult.data.security : null;

  const sessionsResult = await Auth.request("/me/sessions");
  panelState.sessions = sessionsResult.status === 200 && sessionsResult.data?.ok
    ? (sessionsResult.data.sessions || [])
    : [];

  const keysResult = await Auth.request("/me/game-keys");
  panelState.gameKeys = keysResult.status === 200 && keysResult.data?.ok
    ? (keysResult.data.keys || [])
    : [];

  const patreonResult = await Auth.request("/me/linked-accounts/patreon");
  panelState.patreon = patreonResult.status === 200 && patreonResult.data?.ok
    ? patreonResult.data.patreon
    : null;

  return true;
}

function renderTopbar() {
  const user = panelState.me?.user;
  if (!user) {
    return;
  }
  setAvatarTargets(user);
  text("topbar-displayname", user.display_name || user.username || "Compte");
  text("topbar-subline", user.email || "Session active");
}

function renderOverview() {
  const user = panelState.me?.user;
  const gameKeysSummary = panelState.me?.game_keys || {};
  const security = panelState.security || {};
  if (!user) {
    return;
  }

  setAvatarTargets(user);
  text("ov-username", user.display_name || user.username || "-");
  text("ov-identity-line", user.bio || user.email || "-");
  text("ov-username-raw", user.username || "-");
  text("ov-email", user.email || "-");
  text("ov-locale", user.locale || "-");
  text("ov-role", user.role || "player");
  text("ov-role-pill", user.role || "player");
  text("ov-key-count", String(gameKeysSummary.activated ?? 0));
  text("ov-entitlements", (gameKeysSummary.entitlements || []).join(", ") || "Aucun");
  text("ov-patreon", panelState.patreon?.linked ? (panelState.patreon.tier || "Lié") : "Non lié");
  text("ov-session-count", String(security.session_inventory?.active_sessions ?? panelState.sessions.length));
  text("ov-device-count", String(security.session_inventory?.device_count ?? 0));
  text("ov-2fa", security.two_factor_enabled ? "Activé" : "Désactivé");

  badge("ov-email-badge", user.email_verified ? "Vérifié" : "Non vérifié", user.email_verified ? "badge--success" : "badge--warning");
  badge("ov-emu-badge", gameKeysSummary.sekaiemu_access ? "Autorisé" : "Non autorisé", gameKeysSummary.sekaiemu_access ? "badge--success" : "badge--warning");
  badge("ov-role-pill", user.role || "player", "badge");

  const resendBtn = $("btn-resend-email");
  if (resendBtn) {
    resendBtn.style.display = user.email_verified ? "none" : "inline-flex";
  }
}

function renderProfileForm() {
  const user = panelState.me?.user;
  if (!user) {
    return;
  }
  $("prof-displayname").value = user.display_name || "";
  $("prof-bio").value = user.bio || "";
  $("prof-locale").value = user.locale || "";
  $("prof-avatar-url").value = user.avatar_url || "";
}

function renderGameKeys() {
  const summary = panelState.me?.game_keys || {};
  const summaryNode = $("keys-summary");
  const keysList = $("keys-list");
  if (summaryNode) {
    summaryNode.innerHTML = `
      <span class="badge ${summary.sekaiemu_access ? "badge--success" : "badge--warning"}">SekaiEmu ${summary.sekaiemu_access ? "autorisé" : "non autorisé"}</span>
      <span class="badge">activées: ${summary.activated ?? 0}</span>
      <span class="badge">désactivées: ${summary.deactivated ?? 0}</span>
      <span class="badge">free: ${summary.free ?? 0}</span>
    `;
  }
  if (!keysList) {
    return;
  }
  if (!panelState.gameKeys.length) {
    keysList.innerHTML = `<div class="panel-placeholder">Aucune clé liée à ce compte pour le moment.</div>`;
    return;
  }
  keysList.innerHTML = panelState.gameKeys.map((key) => `
    <div class="key-item">
      <div class="item-stack">
        <strong style="font-family:monospace; color: var(--mint);">${escapeHtml(key.key_code)}</strong>
        ${itemMetaHtml([
          (key.entitlements || []).join(", "),
          key.bound_username ? `lié à ${escapeHtml(key.bound_username)}` : null,
          key.activated_at ? `activée le ${formatDate(key.activated_at)}` : null
        ])}
      </div>
      <span class="badge ${key.status === "activated" ? "badge--success" : key.status === "deactivated" ? "badge--danger" : "badge--warning"}">${escapeHtml(key.status)}</span>
    </div>
  `).join("");
}

function renderSecurity() {
  const security = panelState.security || {};
  badge("sec-email-badge", panelState.me?.user?.email_verified ? "Vérifié" : "Non vérifié", panelState.me?.user?.email_verified ? "badge--success" : "badge--warning");
  badge("sec-2fa-badge", security.two_factor_enabled ? "Activé" : "Désactivé", security.two_factor_enabled ? "badge--success" : "badge--warning");
  text("sec-recovery-count", String(security.recovery_code_count ?? 0));
  text("sec-session-count", String(security.session_inventory?.active_sessions ?? panelState.sessions.length));

  const sessionsList = $("sessions-list");
  if (!sessionsList) {
    return;
  }
  if (!panelState.sessions.length) {
    sessionsList.innerHTML = `<div class="panel-placeholder">Aucune session active trouvée.</div>`;
    return;
  }
  sessionsList.innerHTML = panelState.sessions.map((session) => `
    <div class="session-item">
      <div class="item-stack">
        <strong>${escapeHtml(sessionDisplayName(session))}</strong>
        ${itemMetaHtml([
          `Créée le ${formatDate(session.created_at)}`,
          session.expires_at ? `Expire le ${formatDate(session.expires_at)}` : null,
          session.client_summary || null
        ])}
      </div>
      <div class="item-actions">
        ${session.session_state === "active" ? `<button class="btn btn--ghost btn--sm" data-session-revoke="${session.session_id}">Révoquer</button>` : ""}
      </div>
    </div>
  `).join("");
}

function renderPatreon() {
  const patreon = panelState.patreon;
  if (!patreon) {
    badge("pat-badge", "Indisponible", "badge--warning");
    text("pat-tier", "-");
    text("pat-supporter", "-");
    return;
  }
  badge("pat-badge", patreon.linked ? "Lié" : "Non lié", patreon.linked ? "badge--success" : "badge--warning");
  text("pat-tier", patreon.tier || "-");
  text("pat-supporter", patreon.is_supporter ? "Oui" : "Non");
  $("btn-patreon-link").style.display = patreon.linked ? "none" : "inline-flex";
  $("btn-patreon-unlink").style.display = patreon.linked ? "inline-flex" : "none";
}

function socialPlaceholder(title, subtitle, actions = "") {
  return `
    <div class="panel-placeholder">
      <strong>${escapeHtml(title)}</strong>
      <div class="item-meta">${escapeHtml(subtitle)}</div>
      ${actions}
    </div>
  `;
}

async function renderSocial() {
  const [friendsRes, blockedRes, dmRes] = await Promise.all([
    socialRequest("/friends"),
    socialRequest("/blocks"),
    socialRequest("/messages")
  ]);

  const friendsNode = $("friends-list");
  const blockedNode = $("blocked-list");
  const dmNode = $("dm-list");

  if (friendsRes.status === 200 && friendsRes.data?.ok && Array.isArray(friendsRes.data.friends)) {
    friendsNode.innerHTML = friendsRes.data.friends.length
      ? friendsRes.data.friends.map((friend) => `
        <div class="social-item">
          <div class="item-stack">
            <strong>${escapeHtml(friend.display_name || friend.username)}</strong>
            ${itemMetaHtml([friend.status || null, friend.note || null])}
          </div>
          <div class="item-actions">
            <button class="btn btn--ghost btn--sm" data-social-action="remove-friend" data-username="${escapeHtml(friend.username)}">Retirer</button>
            <button class="btn btn--ghost btn--sm" data-social-action="block-user" data-username="${escapeHtml(friend.username)}">Bloquer</button>
          </div>
        </div>
      `).join("")
      : socialPlaceholder("Aucun ami pour le moment", "La structure UI est prête. Les données live apparaîtront ici dès que l'API sociale sera active.");
  } else {
    friendsNode.innerHTML = socialPlaceholder("API sociale en attente", "La liste d'amis sera affichée ici dès que le backend sera exposé publiquement.");
  }

  if (blockedRes.status === 200 && blockedRes.data?.ok && Array.isArray(blockedRes.data.blocked)) {
    blockedNode.innerHTML = blockedRes.data.blocked.length
      ? blockedRes.data.blocked.map((entry) => `
        <div class="social-item">
          <div class="item-stack">
            <strong>${escapeHtml(entry.display_name || entry.username)}</strong>
            ${itemMetaHtml([entry.reason || "Utilisateur bloqué"])}
          </div>
          <div class="item-actions">
            <button class="btn btn--ghost btn--sm" data-social-action="unblock-user" data-username="${escapeHtml(entry.username)}">Débloquer</button>
          </div>
        </div>
      `).join("")
      : socialPlaceholder("Aucun utilisateur bloqué", "Les utilisateurs bloqués seront listés ici.");
  } else {
    blockedNode.innerHTML = socialPlaceholder("Blocage prêt à brancher", "La gestion des utilisateurs bloqués apparaîtra ici quand l'API sociale sera disponible.");
  }

  if (dmRes.status === 200 && dmRes.data?.ok && Array.isArray(dmRes.data.threads)) {
    dmNode.innerHTML = dmRes.data.threads.length
      ? dmRes.data.threads.map((thread) => `
        <div class="dm-item">
          <div class="item-stack">
            <strong>${escapeHtml(thread.title || thread.username || "Conversation")}</strong>
            ${itemMetaHtml([thread.last_message || "Aucun message", thread.updated_at ? formatDate(thread.updated_at) : null])}
          </div>
          <div class="item-actions">
            <button class="btn btn--ghost btn--sm">Ouvrir</button>
          </div>
        </div>
      `).join("")
      : socialPlaceholder("Aucun message privé", "Les conversations privées apparaîtront ici.");
  } else {
    dmNode.innerHTML = socialPlaceholder("Messagerie privée prête", "Le panneau de messages est prêt côté web. L'API backend manque encore.");
  }
}

function renderNotifications() {
  const prefs = notificationsPrefs();
  $("notif-account").checked = !!prefs.account;
  $("notif-social").checked = !!prefs.social;
  $("notif-sekaiemu").checked = !!prefs.sekaiemu;

  const feed = $("notifications-feed");
  const user = panelState.me?.user;
  const entries = [
    {
      title: "Compte chargé",
      detail: user ? `Bienvenue ${user.display_name || user.username}.` : "Session restaurée."
    },
    {
      title: "Accès SekaiEmu",
      detail: panelState.me?.game_keys?.sekaiemu_access ? "Votre compte possède un entitlement actif." : "Aucun entitlement SekaiEmu actif."
    },
    {
      title: "Patreon",
      detail: panelState.patreon?.linked ? `Compte lié${panelState.patreon.tier ? ` (${panelState.patreon.tier})` : ""}.` : "Compte Patreon non lié."
    }
  ];

  feed.innerHTML = entries.map((entry) => `
    <div class="notification-item">
      <div class="item-stack">
        <strong>${escapeHtml(entry.title)}</strong>
        <div class="item-meta">${escapeHtml(entry.detail)}</div>
      </div>
    </div>
  `).join("");
}

function initNavigation() {
  const links = document.querySelectorAll(".panel-nav-link");
  const sections = document.querySelectorAll(".panel-section");
  links.forEach((link) => {
    link.addEventListener("click", () => {
      links.forEach((item) => item.classList.remove("is-active"));
      sections.forEach((item) => item.classList.remove("is-active"));
      link.classList.add("is-active");
      $(link.dataset.target)?.classList.add("is-active");
    });
  });
}

function initLogout() {
  $("btn-logout")?.addEventListener("click", async () => {
    await Auth.logout("index.html");
  });
}

async function handlePatreonCallback() {
  const url = new URL(window.location.href);
  const code = url.searchParams.get("code");
  const state = url.searchParams.get("state");
  const error = url.searchParams.get("error");
  const errorDescription = url.searchParams.get("error_description");

  if (error) {
    note("pat-msg", errorDescription || "Liaison Patreon annulée.", "error");
    url.searchParams.delete("error");
    url.searchParams.delete("error_description");
    history.replaceState({}, "", `${url.pathname}${url.search ? url.search : ""}`);
    return;
  }
  if (!code || !state) {
    return;
  }

  note("pat-msg", "Finalisation de la liaison Patreon...", "info");
  const { status, data } = await Auth.request("/me/linked-accounts/patreon/complete", {
    method: "POST",
    body: JSON.stringify({ code, state })
  });

  url.searchParams.delete("code");
  url.searchParams.delete("state");
  history.replaceState({}, "", `${url.pathname}${url.search ? url.search : ""}`);

  if (status === 200 && data?.ok) {
    note("pat-msg", "Compte Patreon lié avec succès.", "success");
  } else {
    note("pat-msg", authErrorMessage(data, "Impossible de finaliser Patreon."), "error");
  }
}

function initEmailActions() {
  $("btn-resend-email")?.addEventListener("click", async () => {
    const btn = $("btn-resend-email");
    btn.disabled = true;
    const { status, data } = await Auth.request("/me/email-verification/request", { method: "POST" });
    if (status === 200 && data?.ok) {
      alert(data.already_verified ? "Email déjà vérifié." : "Email de vérification envoyé.");
    } else {
      alert(authErrorMessage(data, "Impossible d'envoyer l'email de vérification."));
    }
    btn.disabled = false;
  });
}

function initProfileForm() {
  const form = $("profile-form");
  if (!form) {
    return;
  }
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const btn = form.querySelector("button[type='submit']");
    btn.disabled = true;
    btn.textContent = "Sauvegarde...";

    const payload = {
      display_name: $("prof-displayname").value.trim(),
      bio: $("prof-bio").value.trim(),
      locale: $("prof-locale").value.trim(),
      avatar_url: $("prof-avatar-url").value.trim() || null
    };

    const { status, data } = await Auth.request("/me/profile", {
      method: "PATCH",
      body: JSON.stringify(payload)
    });

    btn.disabled = false;
    btn.textContent = "Sauvegarder";

    if (status === 200 && data?.ok) {
      await refreshAndRenderAll();
      note("avatar-msg", "Profil mis à jour.", "success");
    } else {
      note("avatar-msg", authErrorMessage(data, "Impossible de mettre à jour le profil."), "error");
    }
  });
}

function initGameKeys() {
  const form = $("key-form");
  if (!form) {
    return;
  }
  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    const gameKey = $("game-key").value.trim();
    if (!gameKey) {
      note("key-msg", "Entrez une clé.", "warning");
      return;
    }
    const { status, data } = await Auth.request("/me/game-keys/activate", {
      method: "POST",
      body: JSON.stringify({ game_key: gameKey })
    });
    if (status === 200 && data?.ok) {
      note("key-msg", data.activated ? "Clé activée avec succès." : "Cette clé est déjà liée à ce compte.", data.activated ? "success" : "warning");
      $("game-key").value = "";
      await refreshAndRenderAll();
    } else {
      note("key-msg", authErrorMessage(data, "Impossible d'activer la clé."), "error");
    }
  });

  $("btn-key-check")?.addEventListener("click", async () => {
    const gameKey = $("game-key").value.trim();
    if (!gameKey) {
      note("key-msg", "Entrez une clé avant la vérification.", "warning");
      return;
    }
    const { status, data } = await Auth.request("/me/game-keys/check", {
      method: "POST",
      body: JSON.stringify({ game_key: gameKey })
    });
    if (status === 200 && data?.ok) {
      if (!data.exists) {
        note("key-msg", "Cette clé n'existe pas.", "warning");
      } else if (data.linked_to_other_account) {
        note("key-msg", "Cette clé est déjà liée à un autre compte.", "error");
      } else if (data.linked_to_current_account) {
        note("key-msg", "Cette clé est déjà liée à ce compte.", "success");
      } else {
        note("key-msg", `Clé réelle: statut ${data.status}.`, data.is_active ? "success" : "warning");
      }
    } else {
      note("key-msg", authErrorMessage(data, "Impossible de vérifier la clé."), "error");
    }
  });
}

function initSecurityActions() {
  $("btn-revoke-others")?.addEventListener("click", async () => {
    const button = $("btn-revoke-others");
    button.disabled = true;
    const { status, data } = await Auth.request("/me/sessions/revoke-others", { method: "POST" });
    button.disabled = false;
    if (status === 200 && data?.ok) {
      note("sec-msg", "Autres sessions révoquées.", "success");
      await refreshAndRenderAll();
    } else {
      note("sec-msg", authErrorMessage(data, "Impossible de révoquer les autres sessions."), "error");
    }
  });

  $("sessions-list")?.addEventListener("click", async (event) => {
    const button = event.target.closest("[data-session-revoke]");
    if (!button) {
      return;
    }
    const sessionId = Number(button.getAttribute("data-session-revoke"));
    if (!sessionId) {
      return;
    }
    const { status, data } = await Auth.request("/me/sessions/revoke", {
      method: "POST",
      body: JSON.stringify({ session_id: sessionId })
    });
    if (status === 200 && data?.ok) {
      note("sec-msg", "Session révoquée.", "success");
      await refreshAndRenderAll();
    } else {
      note("sec-msg", authErrorMessage(data, "Impossible de révoquer cette session."), "error");
    }
  });
}

function initPatreonActions() {
  $("btn-patreon-link")?.addEventListener("click", async () => {
    const { status, data } = await Auth.request("/me/linked-accounts/patreon/begin", {
      method: "POST",
      body: JSON.stringify({})
    });
    if (status === 200 && data?.ok && data.authorization_url) {
      window.location.href = data.authorization_url;
    } else {
      note("pat-msg", authErrorMessage(data, "Impossible d'initialiser Patreon."), "error");
    }
  });

  $("btn-patreon-unlink")?.addEventListener("click", async () => {
    if (!window.confirm("Délier le compte Patreon ?")) {
      return;
    }
    const { status, data } = await Auth.request("/me/linked-accounts/patreon/unlink", { method: "POST" });
    if (status === 200 && data?.ok) {
      note("pat-msg", "Compte Patreon délié.", "success");
      await refreshAndRenderAll();
    } else {
      note("pat-msg", authErrorMessage(data, "Impossible de délier Patreon."), "error");
    }
  });
}

function initSocialActions() {
  $("friend-form")?.addEventListener("submit", async (event) => {
    event.preventDefault();
    const username = $("friend-username").value.trim();
    if (!username) {
      note("friend-msg", "Entrez un nom d'utilisateur.", "warning");
      return;
    }
    const { status, data } = await socialRequest("/friends/request", {
      method: "POST",
      body: JSON.stringify({ username })
    });
    if (status === 200 && data?.ok) {
      note("friend-msg", "Demande d'ami envoyée.", "success");
      $("friend-username").value = "";
      await renderSocial();
    } else if (status === 404 || status === 0) {
      note("friend-msg", "L'API sociale n'est pas encore active publiquement.", "warning");
    } else {
      note("friend-msg", data?.error || "Impossible d'envoyer la demande.", "error");
    }
  });

  document.addEventListener("click", async (event) => {
    const button = event.target.closest("[data-social-action]");
    if (!button) {
      return;
    }
    const action = button.getAttribute("data-social-action");
    const username = button.getAttribute("data-username");
    if (!action || !username) {
      return;
    }
    const map = {
      "remove-friend": { path: "/friends/remove", label: "Ami retiré." },
      "block-user": { path: "/blocks/add", label: "Utilisateur bloqué." },
      "unblock-user": { path: "/blocks/remove", label: "Utilisateur débloqué." }
    };
    const config = map[action];
    if (!config) {
      return;
    }
    const { status, data } = await socialRequest(config.path, {
      method: "POST",
      body: JSON.stringify({ username })
    });
    if (status === 200 && data?.ok) {
      note("friend-msg", config.label, "success");
      await renderSocial();
    } else if (status === 404 || status === 0) {
      note("friend-msg", "L'API sociale n'est pas encore active publiquement.", "warning");
    } else {
      note("friend-msg", data?.error || "Action sociale impossible.", "error");
    }
  });
}

function initNotificationPrefs() {
  const prefs = notificationsPrefs();
  ["account", "social", "sekaiemu"].forEach((key) => {
    const node = $(`notif-${key}`);
    if (!node) {
      return;
    }
    node.checked = !!prefs[key];
    node.addEventListener("change", () => {
      const next = notificationsPrefs();
      next[key] = node.checked;
      saveNotificationPrefs(next);
      note("notif-msg", "Préférences locales sauvegardées.", "success");
      renderNotifications();
    });
  });
}

function openAvatarModal() {
  $("avatar-modal").hidden = false;
}

function closeAvatarModal() {
  $("avatar-modal").hidden = true;
}

function drawAvatarCanvas() {
  const canvas = $("avatar-canvas");
  const ctx = canvas.getContext("2d");
  const editor = panelState.avatarEditor;
  ctx.clearRect(0, 0, canvas.width, canvas.height);
  ctx.fillStyle = "#060b11";
  ctx.fillRect(0, 0, canvas.width, canvas.height);
  if (!editor.image) {
    return;
  }

  const image = editor.image;
  const baseScale = Math.max(canvas.width / image.width, canvas.height / image.height);
  const scale = baseScale * editor.scale;
  const drawWidth = image.width * scale;
  const drawHeight = image.height * scale;
  const x = (canvas.width - drawWidth) / 2 + editor.offsetX;
  const y = (canvas.height - drawHeight) / 2 + editor.offsetY;
  ctx.drawImage(image, x, y, drawWidth, drawHeight);

  ctx.strokeStyle = "rgba(255,255,255,0.85)";
  ctx.lineWidth = 2;
  ctx.strokeRect(1, 1, canvas.width - 2, canvas.height - 2);
}

async function applyAvatarCrop() {
  const canvas = $("avatar-canvas");
  const dataUrl = canvas.toDataURL("image/jpeg", 0.92);
  $("prof-avatar-url").value = dataUrl;
  setAvatarTargets({ ...panelState.me?.user, avatar_url: dataUrl });
  note("avatar-msg", "Avatar préparé. N'oubliez pas de sauvegarder le profil.", "success");
  closeAvatarModal();
}

function initAvatarEditor() {
  const fileInput = $("avatar-file-input");
  const modal = $("avatar-modal");
  const canvas = $("avatar-canvas");
  const zoom = $("avatar-zoom");
  const editor = panelState.avatarEditor;

  fileInput?.addEventListener("change", async () => {
    const file = fileInput.files?.[0];
    if (!file) {
      return;
    }
    if (!["image/png", "image/jpeg"].includes(file.type)) {
      note("avatar-msg", "Formats acceptés: PNG ou JPG.", "error");
      return;
    }
    if (file.size > 1024 * 1024) {
      note("avatar-msg", "L'image dépasse 1 MB.", "error");
      return;
    }

    const url = URL.createObjectURL(file);
    const image = new Image();
    image.onload = () => {
      if (editor.imageUrl) {
        URL.revokeObjectURL(editor.imageUrl);
      }
      editor.image = image;
      editor.imageUrl = url;
      editor.scale = 1;
      editor.offsetX = 0;
      editor.offsetY = 0;
      zoom.value = "1";
      drawAvatarCanvas();
      openAvatarModal();
    };
    image.src = url;
  });

  $("btn-avatar-remove")?.addEventListener("click", () => {
    $("prof-avatar-url").value = "";
    const user = panelState.me?.user;
    setAvatarTargets({ ...user, avatar_url: "" });
    note("avatar-msg", "Avatar retiré. Sauvegardez le profil pour appliquer.", "warning");
  });

  $("btn-avatar-close")?.addEventListener("click", closeAvatarModal);
  modal?.querySelector(".avatar-modal__backdrop")?.addEventListener("click", closeAvatarModal);

  $("btn-avatar-apply")?.addEventListener("click", applyAvatarCrop);

  zoom?.addEventListener("input", () => {
    editor.scale = Number(zoom.value);
    drawAvatarCanvas();
  });

  canvas?.addEventListener("mousedown", (event) => {
    editor.dragging = true;
    editor.dragStartX = event.clientX;
    editor.dragStartY = event.clientY;
    editor.dragOriginX = editor.offsetX;
    editor.dragOriginY = editor.offsetY;
    canvas.classList.add("is-dragging");
  });

  window.addEventListener("mousemove", (event) => {
    if (!editor.dragging) {
      return;
    }
    editor.offsetX = editor.dragOriginX + (event.clientX - editor.dragStartX);
    editor.offsetY = editor.dragOriginY + (event.clientY - editor.dragStartY);
    drawAvatarCanvas();
  });

  window.addEventListener("mouseup", () => {
    editor.dragging = false;
    canvas?.classList.remove("is-dragging");
  });
}

async function refreshAndRenderAll() {
  const ok = await refreshAccountData();
  if (!ok) {
    return;
  }
  renderTopbar();
  renderOverview();
  renderProfileForm();
  renderGameKeys();
  renderSecurity();
  renderPatreon();
  renderNotifications();
  await renderSocial();
}

document.addEventListener("DOMContentLoaded", async () => {
  if (!Auth.getToken()) {
    window.location.href = "login.html";
    return;
  }

  initNavigation();
  initLogout();
  initEmailActions();
  initProfileForm();
  initGameKeys();
  initSecurityActions();
  initPatreonActions();
  initSocialActions();
  initNotificationPrefs();
  initAvatarEditor();

  await handlePatreonCallback();
  await refreshAndRenderAll();
});
