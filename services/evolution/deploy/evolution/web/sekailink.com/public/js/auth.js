const API_BASE = "https://sekailink.com/api/identity";
const AUTH_TOKEN_KEY = "sekailink_session_token";
const AUTH_USER_KEY = "sekailink_user";
const AUTH_SESSION_KEY = "sekailink_session";

function safeJsonParse(value) {
  if (!value) {
    return null;
  }
  try {
    return JSON.parse(value);
  } catch (_error) {
    return null;
  }
}

function escapeText(value) {
  return String(value ?? "");
}

const Auth = {
  getToken() {
    return localStorage.getItem(AUTH_TOKEN_KEY);
  },

  setToken(token) {
    if (token) {
      localStorage.setItem(AUTH_TOKEN_KEY, token);
    } else {
      localStorage.removeItem(AUTH_TOKEN_KEY);
    }
  },

  getUser() {
    return safeJsonParse(localStorage.getItem(AUTH_USER_KEY));
  },

  setUser(user) {
    if (user) {
      localStorage.setItem(AUTH_USER_KEY, JSON.stringify(user));
    } else {
      localStorage.removeItem(AUTH_USER_KEY);
    }
  },

  getSession() {
    return safeJsonParse(localStorage.getItem(AUTH_SESSION_KEY));
  },

  setSession(session) {
    if (session) {
      localStorage.setItem(AUTH_SESSION_KEY, JSON.stringify(session));
    } else {
      localStorage.removeItem(AUTH_SESSION_KEY);
    }
  },

  setAuthenticatedFromPayload(data) {
    if (data?.session?.session_token) {
      this.setToken(data.session.session_token);
      this.setSession(data.session);
    }
    if (data?.user) {
      this.setUser(data.user);
    }
    window.dispatchEvent(new Event("sekailink:authchange"));
  },

  clearAuth() {
    this.setToken(null);
    this.setSession(null);
    this.setUser(null);
    window.dispatchEvent(new Event("sekailink:authchange"));
  },

  async logout(redirectTo = "index.html") {
    const token = this.getToken();
    const session = this.getSession();
    if (token && session?.session_id) {
      try {
        await this.request("/me/sessions/revoke", {
          method: "POST",
          body: JSON.stringify({ session_id: session.session_id })
        });
      } catch (_error) {
      }
    }
    this.clearAuth();
    if (redirectTo) {
      window.location.href = redirectTo;
    }
  },

  async request(path, options = {}) {
    const url = `${API_BASE}${path}`;
    const headers = {
      ...options.headers
    };
    const hasBody = options.body !== undefined && options.body !== null;
    if (hasBody && !headers["Content-Type"]) {
      headers["Content-Type"] = "application/json";
    }
    const token = this.getToken();
    if (token && !headers.Authorization) {
      headers.Authorization = `Bearer ${token}`;
    }

    try {
      const res = await fetch(url, {
        ...options,
        headers
      });
      const contentType = res.headers.get("content-type") || "";
      const raw = await res.text();
      let data;
      if (raw && contentType.includes("application/json")) {
        data = JSON.parse(raw);
      } else if (!raw) {
        data = {};
      } else {
        data = {
          ok: false,
          error: "non_json_response",
          desc: raw
        };
      }
      return { status: res.status, data };
    } catch (error) {
      console.error("Identity API error", error);
      return {
        status: 0,
        data: {
          ok: false,
          error: "network_error",
          desc: "Impossible de joindre le serveur d'identité."
        }
      };
    }
  },

  async refreshCurrentUser() {
    if (!this.getToken()) {
      return { ok: false, unauthorized: true };
    }
    const { status, data } = await this.request("/me");
    if (status === 200 && data?.ok) {
      this.setAuthenticatedFromPayload(data);
      return { ok: true, data };
    }
    if (status === 401) {
      this.clearAuth();
      return { ok: false, unauthorized: true };
    }
    return { ok: false, data };
  },

  async checkAuth() {
    const result = await this.refreshCurrentUser();
    return result.ok;
  }
};

function authErrorMessage(data, fallback = "Une erreur est survenue.") {
  if (!data) {
    return fallback;
  }
  switch (data.error) {
    case "invalid_credentials":
      return "Identifiants invalides.";
    case "account_disabled":
      return "Ce compte est désactivé.";
    case "two_factor_required":
      return "Ce compte nécessite un code 2FA ou un recovery code.";
    case "identity_user_conflict":
      return "Ce nom d'utilisateur ou cet email est déjà utilisé.";
    case "missing_identity":
      return "Le champ identité est requis.";
    case "missing_username":
      return "Le nom d'utilisateur est requis.";
    case "missing_email":
      return "L'email est requis.";
    case "missing_password":
      return "Le mot de passe est requis.";
    case "unauthorized":
      return "Votre session n'est plus valide.";
    case "network_error":
      return data.desc || "Impossible de joindre le serveur.";
    default:
      return data.desc || data.error || fallback;
  }
}

function updateAuthUI() {
  const user = Auth.getUser();
  const ctaContainer = document.querySelector(".topbar__cta");
  if (!ctaContainer) {
    return;
  }

  let authBlock = document.getElementById("auth-block");
  if (!authBlock) {
    authBlock = document.createElement("div");
    authBlock.id = "auth-block";
    authBlock.className = "auth-block";
    ctaContainer.appendChild(authBlock);
  }

  authBlock.replaceChildren();

  if (user) {
    const link = document.createElement("a");
    link.href = "panel.html";
    link.className = "auth-user-link";
    const span = document.createElement("span");
    span.className = "auth-user-name";
    span.textContent = escapeText(user.display_name || user.username || "Compte");
    link.appendChild(span);
    authBlock.appendChild(link);
  } else {
    const login = document.createElement("a");
    login.href = "login.html";
    login.className = "btn btn--ghost btn--sm";
    login.setAttribute("data-i18n", "nav.login");
    login.textContent = "Connexion";

    const register = document.createElement("a");
    register.href = "register.html";
    register.className = "btn btn--primary btn--sm";
    register.setAttribute("data-i18n", "nav.register");
    register.textContent = "Inscription";

    authBlock.append(login, register);
  }

  if (window.SekaiLinkI18n && typeof window.SekaiLinkI18n.translate === "function") {
    window.SekaiLinkI18n.translate();
  }
}

document.addEventListener("DOMContentLoaded", () => {
  updateAuthUI();
  if (Auth.getToken()) {
    Auth.checkAuth();
  }
  window.addEventListener("sekailink:authchange", updateAuthUI);
});
