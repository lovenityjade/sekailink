document.addEventListener("DOMContentLoaded", () => {
  const form = document.getElementById("register-form");
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

    const username = document.getElementById("username").value.trim();
    const email = document.getElementById("email").value.trim();
    const password = document.getElementById("password").value;
    const passwordConfirm = document.getElementById("password_confirm").value;
    const submitBtn = form.querySelector("button[type='submit']");
    const submitLabel = submitBtn?.querySelector("span");

    if (password !== passwordConfirm) {
      errorDiv.textContent = "Les mots de passe ne correspondent pas.";
      errorDiv.style.display = "block";
      return;
    }
    if (username.length < 3) {
      errorDiv.textContent = "Le nom d'utilisateur doit contenir au moins 3 caractères.";
      errorDiv.style.display = "block";
      return;
    }
    if (password.length < 8) {
      errorDiv.textContent = "Le mot de passe doit contenir au moins 8 caractères.";
      errorDiv.style.display = "block";
      return;
    }

    submitBtn.disabled = true;
    if (submitLabel) {
      submitLabel.textContent = "Inscription...";
    }

    const { status, data } = await Auth.request("/register", {
      method: "POST",
      body: JSON.stringify({ username, email, password })
    });

    submitBtn.disabled = false;
    if (submitLabel) {
      submitLabel.textContent = "S'inscrire";
    }

    if ((status === 200 || status === 201) && data?.ok) {
      Auth.setAuthenticatedFromPayload(data);
      window.location.href = "panel.html";
      return;
    }

    errorDiv.textContent = authErrorMessage(data, "Inscription impossible.");
    errorDiv.style.display = "block";
  });

  window.addEventListener("sekailink:languagechange", () => {
    if (window.SekaiLinkI18n) {
      window.SekaiLinkI18n.translate(document.body);
    }
  });
});
