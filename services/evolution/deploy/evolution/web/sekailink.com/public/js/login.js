document.addEventListener("DOMContentLoaded", () => {
  const form = document.getElementById("login-form");
  const errorDiv = document.getElementById("auth-error");

  if (!form || !errorDiv) {
    return;
  }

  if (Auth.getToken()) {
    window.location.href = "panel.html";
    return;
  }

  form.addEventListener("submit", async (event) => {
    event.preventDefault();
    errorDiv.style.display = "none";
    errorDiv.textContent = "";

    const identity = document.getElementById("identity").value.trim();
    const password = document.getElementById("password").value;
    const submitBtn = form.querySelector("button[type='submit']");
    const submitLabel = submitBtn?.querySelector("span");

    if (!identity || !password) {
      errorDiv.textContent = "Email/nom d'utilisateur et mot de passe requis.";
      errorDiv.style.display = "block";
      return;
    }

    submitBtn.disabled = true;
    if (submitLabel) {
      submitLabel.textContent = "Connexion...";
    }

    const { status, data } = await Auth.request("/login", {
      method: "POST",
      body: JSON.stringify({ identity, password })
    });

    submitBtn.disabled = false;
    if (submitLabel) {
      submitLabel.textContent = "Se connecter";
    }

    if (status === 200 && data?.ok) {
      Auth.setAuthenticatedFromPayload(data);
      window.location.href = "panel.html";
      return;
    }

    errorDiv.textContent = authErrorMessage(data, "Connexion impossible.");
    errorDiv.style.display = "block";
  });

  window.addEventListener("sekailink:languagechange", () => {
    if (window.SekaiLinkI18n) {
      window.SekaiLinkI18n.translate(document.body);
    }
  });
});
