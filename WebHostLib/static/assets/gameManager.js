const tabs = document.querySelectorAll(".skl-gm-tab");
const panels = document.querySelectorAll(".skl-gm-panel");

const setTab = (name) => {
  tabs.forEach((tab) => {
    const active = tab.dataset.gmTab === name;
    tab.classList.toggle("active", active);
    tab.setAttribute("aria-selected", active ? "true" : "false");
  });
  panels.forEach((panel) => {
    panel.classList.toggle("active", panel.dataset.gmPanel === name);
  });
};

tabs.forEach((tab) => {
  tab.addEventListener("click", () => setTab(tab.dataset.gmTab));
});

// Search/filter for Add a Game
const searchInput = document.getElementById("yaml-game-search");
const clearBtn = document.getElementById("yaml-search-clear");
const cards = Array.from(document.querySelectorAll(".yaml-game-card"));
const emptyState = document.getElementById("yaml-game-empty");
const searchWrap = searchInput ? searchInput.closest(".yaml-search") : null;

const normalize = (value) => (value || "").toString().toLowerCase().trim();

const filterCards = () => {
  if (!searchInput) return;
  const query = normalize(searchInput.value);
  let visible = 0;
  cards.forEach((card) => {
    const name = normalize(card.dataset.gameName || card.textContent);
    const match = !query || name.includes(query);
    card.style.display = match ? "" : "none";
    if (match) visible += 1;
  });
  if (emptyState) emptyState.hidden = visible > 0;
  if (searchWrap) searchWrap.classList.toggle("has-value", Boolean(query));
};

if (searchInput) {
  searchInput.addEventListener("input", filterCards);
  filterCards();
}
if (clearBtn) {
  clearBtn.addEventListener("click", () => {
    if (!searchInput) return;
    searchInput.value = "";
    filterCards();
    searchInput.focus();
  });
}

// YAML import
const importBtn = document.getElementById("yaml-import-button");
const importInput = document.getElementById("yaml-import-input");
const importForm = document.getElementById("yaml-import-form");
if (importBtn && importInput && importForm) {
  importBtn.addEventListener("click", () => importInput.click());
  importInput.addEventListener("change", () => {
    if (importInput.files && importInput.files.length) {
      importForm.submit();
    }
  });
}

// Advanced YAML editor
const yamlSelect = document.getElementById("gm-yaml-select");
const yamlLoad = document.getElementById("gm-yaml-load");
const yamlEditor = document.getElementById("gm-yaml-editor");
const yamlHighlight = document.getElementById("gm-yaml-highlight");
const yamlSave = document.getElementById("gm-yaml-save");
const yamlStatus = document.getElementById("gm-yaml-status");
const yamlList = document.querySelector(".yaml-list");
const confirmModal = document.getElementById("yaml-custom-modal");
const confirmClose = document.querySelectorAll("[data-custom-close]");
const confirmButton = document.getElementById("gm-yaml-confirm");

const escapeHtml = (value) =>
  value
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;");

const highlightYaml = (text) => {
  let html = escapeHtml(text);
  html = html.replace(/(^|\n)(\s*#.*)/g, '$1<span class="yaml-comment">$2</span>');
  html = html.replace(/(^|\n)(\s*[^:\n]+:)/g, '$1<span class="yaml-key">$2</span>');
  html = html.replace(/(:\s*)("[^"\n]*"|'[^'\n]*')/g, '$1<span class="yaml-string">$2</span>');
  html = html.replace(/\b(true|false|null)\b/gi, '<span class="yaml-boolean">$1</span>');
  html = html.replace(/\b\d+(\.\d+)?\b/g, '<span class="yaml-number">$&</span>');
  return html;
};

const syncHighlight = () => {
  if (!yamlEditor || !yamlHighlight) return;
  yamlHighlight.innerHTML = highlightYaml(yamlEditor.value);
};

if (yamlEditor && yamlHighlight) {
  yamlEditor.addEventListener("input", syncHighlight);
  yamlEditor.addEventListener("scroll", () => {
    yamlHighlight.scrollTop = yamlEditor.scrollTop;
    yamlHighlight.scrollLeft = yamlEditor.scrollLeft;
  });
}

const openModal = () => {
  if (!confirmModal) return;
  confirmModal.classList.add("open");
  confirmModal.setAttribute("aria-hidden", "false");
};

const closeModal = () => {
  if (!confirmModal) return;
  confirmModal.classList.remove("open");
  confirmModal.setAttribute("aria-hidden", "true");
};

if (confirmClose && confirmClose.length) {
  confirmClose.forEach((btn) => btn.addEventListener("click", closeModal));
}

const loadYaml = async () => {
  if (!yamlSelect || !yamlEditor) return;
  const yamlId = yamlSelect.value;
  if (!yamlId) {
    if (yamlStatus) yamlStatus.textContent = "Select a YAML first.";
    return;
  }
  try {
    if (yamlStatus) yamlStatus.textContent = "Loading...";
    const res = await fetch(`/api/yamls/${encodeURIComponent(yamlId)}/raw`);
    if (!res.ok) throw new Error("Unable to load YAML.");
    const data = await res.json();
    yamlEditor.value = data.yaml || "";
    syncHighlight();
    if (yamlStatus) yamlStatus.textContent = "";
  } catch (err) {
    if (yamlStatus) yamlStatus.textContent = err.message || "Unable to load YAML.";
  }
};

const appendYamlRow = (entry) => {
  if (!yamlList) return;
  const row = document.createElement("div");
  row.className = "yaml-row";
  const meta = document.createElement("div");
  meta.className = "yaml-meta";
  const title = document.createElement("div");
  title.className = "yaml-title";
  title.textContent = entry.title;
  if (entry.custom) {
    const badge = document.createElement("span");
    badge.className = "yaml-badge";
    badge.textContent = "Custom";
    title.appendChild(badge);
  }
  const sub = document.createElement("div");
  sub.className = "yaml-sub";
  sub.textContent = `Game: ${entry.game || "Unknown"} • Player: ${entry.player_name || "-"}`;
  meta.appendChild(title);
  meta.appendChild(sub);
  const actions = document.createElement("div");
  actions.className = "yaml-actions";
  const edit = document.createElement("a");
  edit.href = `/dashboard/yaml/${encodeURIComponent(entry.game)}/edit/${encodeURIComponent(entry.id)}`;
  edit.textContent = "Edit";
  const duplicate = document.createElement("button");
  duplicate.type = "button";
  duplicate.className = "yaml-duplicate";
  duplicate.dataset.yamlDuplicate = entry.id;
  duplicate.textContent = "Duplicate";
  const download = document.createElement("a");
  download.href = `/dashboard/yaml/${encodeURIComponent(entry.id)}/download`;
  download.textContent = "Download";
  const form = document.createElement("form");
  form.method = "post";
  form.action = `/dashboard/yaml/${encodeURIComponent(entry.id)}/delete`;
  form.className = "yaml-delete-form";
  form.dataset.yamlId = entry.id;
  const delBtn = document.createElement("button");
  delBtn.type = "submit";
  delBtn.textContent = "Delete";
  form.appendChild(delBtn);
  actions.appendChild(edit);
  actions.appendChild(duplicate);
  actions.appendChild(download);
  actions.appendChild(form);
  row.appendChild(meta);
  row.appendChild(actions);
  yamlList.appendChild(row);
};

const saveYaml = async () => {
  if (!yamlSelect || !yamlEditor) return;
  const yamlId = yamlSelect.value;
  if (!yamlId) {
    if (yamlStatus) yamlStatus.textContent = "Select a YAML first.";
    return;
  }
  try {
    if (yamlStatus) yamlStatus.textContent = "Saving...";
    const res = await fetch(`/api/yamls/${encodeURIComponent(yamlId)}/raw`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ content: yamlEditor.value, save_as_new: true }),
    });
    if (!res.ok) {
      const data = await res.json().catch(() => ({}));
      throw new Error(data.error || "Unable to save YAML.");
    }
    const data = await res.json().catch(() => ({}));
    if (data && data.yaml) {
      const entry = data.yaml;
      if (yamlStatus) yamlStatus.textContent = "Saved as Custom.";
      const option = document.createElement("option");
      option.value = entry.id;
      option.dataset.custom = "1";
      option.textContent = `${entry.title} · ${entry.game || "Unknown game"}`;
      yamlSelect.appendChild(option);
      yamlSelect.value = entry.id;
      if (yamlList) appendYamlRow(entry);
    }
  } catch (err) {
    if (yamlStatus) yamlStatus.textContent = err.message || "Unable to save YAML.";
  }
};

const handleDuplicate = async (yamlId) => {
  try {
    if (yamlStatus) yamlStatus.textContent = "Duplicating...";
    const res = await fetch(`/api/yamls/${encodeURIComponent(yamlId)}/duplicate`, { method: "POST" });
    if (!res.ok) {
      const data = await res.json().catch(() => ({}));
      throw new Error(data.error || "Unable to duplicate YAML.");
    }
    const data = await res.json().catch(() => ({}));
    if (data && data.yaml) {
      const entry = data.yaml;
      const option = document.createElement("option");
      option.value = entry.id;
      option.dataset.custom = entry.custom ? "1" : "0";
      option.textContent = `${entry.title} · ${entry.game || "Unknown game"}${entry.custom ? " (Custom)" : ""}`;
      yamlSelect.appendChild(option);
      if (yamlList) appendYamlRow(entry);
      if (yamlStatus) yamlStatus.textContent = "YAML duplicated.";
    }
  } catch (err) {
    if (yamlStatus) yamlStatus.textContent = err.message || "Unable to duplicate YAML.";
  }
};

document.addEventListener("click", (event) => {
  const button = event.target.closest(".yaml-duplicate");
  if (!button) return;
  const yamlId = button.dataset.yamlDuplicate;
  if (!yamlId) return;
  handleDuplicate(yamlId);
});

document.addEventListener("submit", async (event) => {
  const form = event.target.closest(".yaml-delete-form");
  if (!form) return;
  event.preventDefault();
  const yamlId = form.dataset.yamlId;
  if (!yamlId) return;
  try {
    if (yamlStatus) yamlStatus.textContent = "Deleting...";
    const res = await fetch(`/api/yamls/${encodeURIComponent(yamlId)}/delete`, { method: "POST" });
    if (!res.ok) throw new Error("Unable to delete YAML.");
    const row = form.closest(".yaml-row");
    if (row) row.remove();
    if (yamlSelect) {
      const option = yamlSelect.querySelector(`option[value="${CSS.escape(yamlId)}"]`);
      if (option) option.remove();
    }
    if (yamlStatus) yamlStatus.textContent = "YAML deleted.";
  } catch (err) {
    if (yamlStatus) yamlStatus.textContent = err.message || "Unable to delete YAML.";
  }
});

if (yamlLoad) yamlLoad.addEventListener("click", loadYaml);
if (yamlSave) {
  yamlSave.addEventListener("click", () => {
    if (!yamlEditor || !yamlEditor.value.trim()) {
      if (yamlStatus) yamlStatus.textContent = "YAML content is empty.";
      return;
    }
    openModal();
  });
}
if (confirmButton) {
  confirmButton.addEventListener("click", async () => {
    closeModal();
    await saveYaml();
  });
}
