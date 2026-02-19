(() => {
  const input = document.getElementById("yaml-game-search");
  const clearBtn = document.getElementById("yaml-search-clear");
  const cards = Array.from(document.querySelectorAll(".yaml-game-card"));
  const emptyState = document.getElementById("yaml-game-empty");
  const searchWrap = input ? input.closest(".yaml-search") : null;

  if (!input || !cards.length) return;

  const normalize = (value) => (value || "").toLowerCase().trim();
  const escapeHtml = (value) =>
    String(value)
      .replace(/&/g, "&amp;")
      .replace(/</g, "&lt;")
      .replace(/>/g, "&gt;")
      .replace(/\"/g, "&quot;")
      .replace(/'/g, "&#39;");

  const setHighlight = (element, label, query) => {
    if (!element) return;
    if (!query) {
      element.textContent = label;
      return;
    }
    const lowerLabel = label.toLowerCase();
    const index = lowerLabel.indexOf(query);
    if (index === -1) {
      element.textContent = label;
      return;
    }
    const before = escapeHtml(label.slice(0, index));
    const match = escapeHtml(label.slice(index, index + query.length));
    const after = escapeHtml(label.slice(index + query.length));
    element.innerHTML = `${before}<span class=\"yaml-game-highlight\">${match}</span>${after}`;
  };

  const applyFilter = () => {
    const query = normalize(input.value);
    if (searchWrap) {
      searchWrap.classList.toggle("has-value", Boolean(query));
    }

    let visibleCount = 0;
    cards.forEach((card) => {
      const label = card.dataset.gameLabel || card.dataset.gameName || card.dataset.gameId || "";
      const name = normalize(card.dataset.gameName);
      const id = normalize(card.dataset.gameId);
      const labelNorm = normalize(label);
      const matches = !query || name.includes(query) || id.includes(query) || labelNorm.includes(query);

      const nameEl = card.querySelector(".yaml-game-name");
      setHighlight(nameEl, label, query);

      card.hidden = !matches;
      if (matches) visibleCount += 1;
    });

    if (emptyState) {
      emptyState.hidden = visibleCount !== 0;
    }
  };

  input.addEventListener("input", applyFilter);
  if (clearBtn) {
    clearBtn.addEventListener("click", () => {
      input.value = "";
      applyFilter();
      input.focus();
    });
  }

  applyFilter();
})();
