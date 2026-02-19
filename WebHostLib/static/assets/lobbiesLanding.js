const lobbyList = document.querySelector("#lobby-list");
const lobbyEmpty = document.querySelector("#lobby-empty");
const lobbyError = document.querySelector("#lobby-error");
const lobbySearch = document.querySelector("#lobby-search");
const lobbyForm = document.querySelector("#lobby-create-form");
const lobbyModal = document.querySelector("#lobby-create-modal");
const lobbyModalOpen = document.querySelector("#open-lobby-create");
const lobbyModalClose = document.querySelectorAll("[data-modal-close]");
const infoModal = document.querySelector("#lobby-info-modal");
const infoModalClose = document.querySelectorAll("[data-info-close]");
const infoContent = document.querySelector("#lobby-info-content");
let cachedLobbies = [];

const openModal = () => {
  if (!lobbyModal) return;
  lobbyModal.classList.add("open");
  lobbyModal.setAttribute("aria-hidden", "false");
};

const closeModal = () => {
  if (!lobbyModal) return;
  lobbyModal.classList.remove("open");
  lobbyModal.setAttribute("aria-hidden", "true");
};

const openInfoModal = (lobby) => {
  if (!infoModal || !infoContent) return;
  infoContent.innerHTML = "";
  const addRow = (label, value) => {
    const row = document.createElement("div");
    row.className = "skl-roominfo-row";
    const key = document.createElement("span");
    key.textContent = label;
    const val = document.createElement("span");
    val.textContent = value || "—";
    row.appendChild(key);
    row.appendChild(val);
    infoContent.appendChild(row);
  };
  addRow("Name", lobby.name);
  addRow("Owner", lobby.owner || "Unknown");
  addRow("Access", lobby.is_private ? "Private" : "Public");
  addRow("Release", lobby.release_mode);
  addRow("Collect", lobby.collect_mode);
  addRow("Remaining", lobby.remaining_mode);
  addRow("Countdown", lobby.countdown_mode);
  addRow("Item cheat", lobby.item_cheat ? "Enabled" : "Disabled");
  addRow("Spoiler log", lobby.spoiler);
  addRow("Hint cost", `${lobby.hint_cost || 0}%`);
  addRow("Plando items", lobby.plando_items ? "On" : "Off");
  addRow("Plando bosses", lobby.plando_bosses ? "On" : "Off");
  addRow("Plando connections", lobby.plando_connections ? "On" : "Off");
  addRow("Plando texts", lobby.plando_texts ? "On" : "Off");
  addRow("Custom YAMLs", lobby.allow_custom_yamls ? "Allowed" : "Blocked");
  infoModal.classList.add("open");
  infoModal.setAttribute("aria-hidden", "false");
};

const closeInfoModal = () => {
  if (!infoModal) return;
  infoModal.classList.remove("open");
  infoModal.setAttribute("aria-hidden", "true");
};

const formatTimestamp = (value) => {
  if (!value) return "";
  const date = new Date(value);
  if (Number.isNaN(date.getTime())) return "";
  return date.toLocaleString(undefined, { dateStyle: "medium", timeStyle: "short" });
};

const formatMode = (mode) => {
  if (!mode) return "Standard";
  if (mode === "auto") return "Auto";
  if (mode === "enabled") return "Enabled";
  if (mode === "disabled") return "Disabled";
  return mode.replace("-", " ");
};

const setError = (message) => {
  if (!lobbyError) return;
  lobbyError.textContent = message || "";
  lobbyError.style.display = message ? "block" : "none";
};

const renderLobbies = (lobbies) => {
  if (!lobbyList) return;
  lobbyList.innerHTML = "";
  if (!lobbies.length) {
    if (lobbyEmpty) lobbyEmpty.style.display = "block";
    return;
  }
  if (lobbyEmpty) lobbyEmpty.style.display = "none";
  lobbies.forEach((lobby) => {
    const card = document.createElement("div");
    card.className = "skl-lobby-card";

    const infoBlock = document.createElement("div");
    infoBlock.className = "skl-lobby-card-info";

    const title = document.createElement("h3");
    title.textContent = lobby.name;
    if (lobby.is_private) {
      const badge = document.createElement("span");
      badge.className = "skl-lobby-private";
      badge.textContent = "Private";
      title.appendChild(badge);
    }

    const desc = document.createElement("p");
    desc.textContent = lobby.description || "No description yet.";

    const sub = document.createElement("p");
    sub.textContent = `Owner: ${lobby.owner || "Unknown"} · Last active: ${formatTimestamp(lobby.last_activity) || "—"}`;

    const titleRow = document.createElement("div");
    titleRow.className = "skl-room-title";
    const avatar = document.createElement("div");
    avatar.className = "skl-room-avatar";
    avatar.textContent = (lobby.name || "S").trim().slice(0, 1).toUpperCase();
    titleRow.appendChild(avatar);
    titleRow.appendChild(title);

    infoBlock.appendChild(titleRow);
    infoBlock.appendChild(desc);
    infoBlock.appendChild(sub);

    const players = document.createElement("div");
    players.className = "skl-room-col-value";
    players.textContent = `${lobby.member_count || 0} players`;

    const access = document.createElement("div");
    access.className = "skl-room-col-value dim";
    access.textContent = lobby.is_private ? "Private" : "Public";

    const mode = document.createElement("div");
    mode.className = "skl-room-col-value dim";
    mode.textContent = formatMode(lobby.countdown_mode);

    const progress = document.createElement("div");
    progress.className = "skl-room-col-value dim";
    progress.textContent = lobby.message_count > 0 ? "In progress" : "No progress";

    const actions = document.createElement("div");
    actions.className = "skl-lobby-actions";
    const join = document.createElement("a");
    join.className = "skl-btn ghost";
    join.href = lobby.url;
    join.textContent = "Open lobby";
    const copy = document.createElement("button");
    copy.className = "skl-btn ghost";
    copy.type = "button";
    copy.textContent = "Copy link";
    copy.addEventListener("click", (event) => {
      event.stopPropagation();
      if (!navigator.clipboard) return;
      const fullUrl = new URL(lobby.url || "", window.location.origin).toString();
      navigator.clipboard.writeText(fullUrl);
    });
    const infoBtn = document.createElement("button");
    infoBtn.className = "skl-btn ghost";
    infoBtn.type = "button";
    infoBtn.textContent = "Room info";
    infoBtn.addEventListener("click", (event) => {
      event.stopPropagation();
      openInfoModal(lobby);
    });
    actions.appendChild(join);
    actions.appendChild(copy);
    actions.appendChild(infoBtn);

    card.appendChild(infoBlock);
    card.appendChild(players);
    card.appendChild(access);
    card.appendChild(mode);
    card.appendChild(progress);
    card.appendChild(actions);

    lobbyList.appendChild(card);
  });
};

const applyLobbyFilter = () => {
  const term = (lobbySearch?.value || "").toLowerCase().trim();
  if (!term) {
    renderLobbies(cachedLobbies);
    return;
  }
  const filtered = cachedLobbies.filter((lobby) => {
    const name = (lobby.name || "").toLowerCase();
    const owner = (lobby.owner || "").toLowerCase();
    return name.includes(term) || owner.includes(term);
  });
  renderLobbies(filtered);
};

const loadLobbies = async () => {
  try {
    const response = await fetch("/api/lobbies");
    if (!response.ok) throw new Error("Failed to load lobbies.");
    const data = await response.json();
    cachedLobbies = data.lobbies || [];
    applyLobbyFilter();
    setError("");
  } catch (err) {
    setError(err.message || "Failed to load lobbies.");
  }
};

if (lobbyForm) {
  lobbyForm.addEventListener("submit", async (event) => {
    event.preventDefault();
    setError("");
    const auth = lobbyForm.dataset.auth === "1";
    if (!auth) {
      setError("Sign in to create a lobby.");
      return;
    }
    const formData = new FormData(lobbyForm);
    const payload = {
      name: (formData.get("name") || "").toString().trim(),
      description: (formData.get("description") || "").toString().trim(),
      release_mode: (formData.get("release_mode") || "").toString(),
      collect_mode: (formData.get("collect_mode") || "").toString(),
      remaining_mode: (formData.get("remaining_mode") || "").toString(),
      countdown_mode: (formData.get("countdown_mode") || "").toString(),
      item_cheat: formData.get("item_cheat") === "1",
      spoiler: (formData.get("spoiler") || "0").toString(),
      hint_cost: (formData.get("hint_cost") || "5").toString(),
      server_password: (formData.get("server_password") || "").toString(),
      plando_items: formData.get("plando_items") === "on",
      plando_bosses: formData.get("plando_bosses") === "on",
      plando_connections: formData.get("plando_connections") === "on",
      plando_texts: formData.get("plando_texts") === "on",
      allow_custom_yamls: formData.get("allow_custom_yamls") !== "0",
    };
    try {
      const response = await fetch("/api/lobbies", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(payload),
      });
      if (response.status === 401) {
        setError("Sign in to create a lobby.");
        return;
      }
      if (!response.ok) {
        const data = await response.json().catch(() => ({}));
        throw new Error(data.error || "Unable to create lobby.");
      }
      const data = await response.json();
      if (data.url) {
        window.location.href = data.url;
        return;
      }
      lobbyForm.reset();
      closeModal();
      await loadLobbies();
    } catch (err) {
      setError(err.message || "Unable to create lobby.");
    }
  });
}

if (lobbyModalOpen) {
  lobbyModalOpen.addEventListener("click", () => {
    setError("");
    openModal();
  });
}

if (lobbyModalClose && lobbyModalClose.length) {
  lobbyModalClose.forEach((button) => {
    button.addEventListener("click", () => closeModal());
  });
}

if (infoModalClose && infoModalClose.length) {
  infoModalClose.forEach((button) => {
    button.addEventListener("click", () => closeInfoModal());
  });
}

if (lobbySearch) {
  lobbySearch.addEventListener("input", () => applyLobbyFilter());
}

document.addEventListener("keydown", (event) => {
  if (event.key === "Escape") closeModal();
  if (event.key === "Escape") closeInfoModal();
});

loadLobbies();
setInterval(loadLobbies, 20000);
