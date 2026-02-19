const lobbyRoot = document.querySelector("#lobby-room");
if (lobbyRoot) {
  const lobbyId = lobbyRoot.dataset.lobbyId;
  const isAuthed = lobbyRoot.dataset.auth === "1";
  let lastMessageId = 0;
  const socket = window.io ? window.io({ transports: ["websocket"] }) : null;

  const chatLog = document.querySelector("#lobby-chat-log");
  const chatForm = document.querySelector("#lobby-chat-form");
  const chatInput = chatForm ? chatForm.querySelector("textarea") : null;
  const membersList = document.querySelector("#lobby-members");

  const activeYamlId = lobbyRoot.dataset.activeYaml || "";
  let readyState = lobbyRoot.dataset.ready === "1";
  let generationState = "idle";
  let generationLocked = false;
  const isOwner = lobbyRoot.dataset.owner === "1";
  let isPrivate = lobbyRoot.dataset.private === "1";
  let allowCustomYamls = lobbyRoot.dataset.allowCustom === "1";
  let isMember = lobbyRoot.dataset.member === "1";
  const currentUserName = lobbyRoot.dataset.currentUser || "";
  const playSfx = (name, volume) => window.SKL_SFX && window.SKL_SFX.play ? window.SKL_SFX.play(name, volume) : false;
  const yamlSelect = document.querySelector("#lobby-yaml-select");
  const yamlAddButton = document.querySelector("#lobby-yaml-add");
  const yamlStatus = document.querySelector("#lobby-yaml-status");
  const activeYamlLabel = document.querySelector("#lobby-active-yaml-label");
  const activeYamlListEl = document.querySelector("#lobby-yaml-active-list");
  const readyToggle = document.querySelector("#lobby-ready-toggle");
  const readyStatus = document.querySelector("#lobby-ready-status");
  const generateButton = document.querySelector("#lobby-generate");
  const generateStatus = document.querySelector("#lobby-generate-status");
  const generateBoxState = document.querySelector("#lobby-generate-state");
  const roomInfoLink = document.querySelector("#lobby-room-link");
  const seedTimer = document.querySelector("#lobby-seed-timer");
  const roomInfoServer = document.querySelector("#lobby-room-server");
  const seedInfoLink = document.querySelector("#lobby-seed-link");
  const roomPlayers = document.querySelector("#lobby-room-players");
  const roomChecks = document.querySelector("#lobby-room-checks");
  const roomComplete = document.querySelector("#lobby-room-complete");
  const terminalLog = document.querySelector("#lobby-terminal-log");
  const terminalForm = document.querySelector("#lobby-terminal-form");
  const terminalInput = document.querySelector("#lobby-terminal-input");
  const terminalStatus = document.querySelector("#lobby-terminal-status");
  const passwordForm = document.querySelector("#lobby-password-form");
  const passwordInput = document.querySelector("#lobby-password-input");
  const passwordStatus = document.querySelector("#lobby-password-status");
  const settingsForm = document.querySelector("#lobby-settings-form");
  const settingsStatus = document.querySelector("#lobby-settings-status");
  const hostStatus = document.querySelector("#lobby-host-status");
  const hostPresence = document.querySelector("#lobby-host-presence");
  const hostCandidate = document.querySelector("#lobby-host-candidate");
  const hostVoteButton = document.querySelector("#lobby-host-vote");
  const hostVoteStatus = document.querySelector("#lobby-host-vote-status");
  const hostActions = document.querySelector("#lobby-host-actions");
  const ownerName = document.querySelector("#lobby-owner-name");
  const settingsModal = document.querySelector("#lobby-settings-modal");
  const settingsOpen = document.querySelector("#lobby-settings-open");
  const yamlOpen = document.querySelector("#lobby-yaml-open");
  const settingsCloseButtons = settingsModal ? settingsModal.querySelectorAll("[data-settings-close]") : [];
  const yamlModal = document.querySelector("#lobby-yaml-modal");
  const yamlCloseButtons = yamlModal ? yamlModal.querySelectorAll("[data-yaml-close]") : [];
  const playerModal = document.querySelector("#lobby-player-modal");
  const playerCloseButtons = playerModal ? playerModal.querySelectorAll("[data-player-close]") : [];
  const playerNameLabel = document.querySelector("#lobby-player-name");
  const playerYamlLabel = document.querySelector("#lobby-player-yaml");
  const terminalDrawer = document.querySelector("#lobby-terminal-drawer");
  const terminalOpen = document.querySelector("#lobby-terminal-open");
  const terminalCloseButtons = terminalDrawer ? terminalDrawer.querySelectorAll("[data-terminal-close]") : [];
  const profileLink = document.querySelector("#skl-profile-link");
  const generateModal = document.querySelector("#lobby-generate-modal");
  const generateConfirm = document.querySelector("#lobby-generate-confirm");
  const generateCancel = document.querySelector("#lobby-generate-cancel");
  const modalCloseButtons = generateModal ? generateModal.querySelectorAll("[data-modal-close]") : [];
  const closeLobbyButton = document.querySelector("#lobby-close");
  const closeModal = document.querySelector("#lobby-close-modal");
  const closeConfirm = document.querySelector("#lobby-close-confirm");
  const closeCancel = document.querySelector("#lobby-close-cancel");
  const closeModalButtons = closeModal ? closeModal.querySelectorAll("[data-modal-close]") : [];
  const trackerDrawer = document.querySelector("#tracker-drawer");
  const trackerFrame = document.querySelector("#tracker-frame");
  const trackerTitle = document.querySelector("#tracker-title");
  const trackerOpenNew = document.querySelector("#tracker-open-new");

  const openPanel = (panel) => {
    if (!panel) return;
    panel.classList.add("open");
    panel.setAttribute("aria-hidden", "false");
  };

  const closePanel = (panel) => {
    if (!panel) return;
    panel.classList.remove("open");
    panel.setAttribute("aria-hidden", "true");
  };

  let roomInfo = null;
  let downloadsBySlot = new Map();
  let playersByName = new Map();
  let trackerStats = null;
  let currentMembers = [];
  let roomId = null;
  let socketConnected = false;
  let pollTimer = null;
  let seedCompletedAt = null;
  let seedTimerTick = null;
  let terminalBuffer = "";
  const releasedPlayers = new Set();
  const chatHistory = [];
  const chatHistoryMax = 10;
  let chatHistoryIndex = -1;
  let chatDraft = "";

  const formatTime = (value) => {
    if (!value) return "";
    let parsedValue = value;
    if (typeof value === "string" && !value.includes("Z") && !value.includes("+")) {
      parsedValue = `${value}Z`;
    }
    const date = new Date(parsedValue);
    if (Number.isNaN(date.getTime())) return "";
    return date.toLocaleTimeString(undefined, { hour: "2-digit", minute: "2-digit" });
  };

  const renderMessage = (message) => {
    if (!chatLog) return;
    const content = message.content || "";
    const isSystem = (message.author || "").toLowerCase() === "sekailink";
    const isGenerationLog = isSystem && /generation|seed|server:|room:|queued|complete/i.test(content);
    if (isGenerationLog && typeof appendTerminalText === "function") {
      appendTerminalText(`[${formatTime(message.created_at)}] ${content}\n`);
    }
    const wasNearBottom = chatLog.scrollTop + chatLog.clientHeight >= chatLog.scrollHeight - 40;
    const messageEl = document.createElement("div");
    messageEl.className = "skl-chat-message";

    if (message.avatar_url) {
      const avatar = document.createElement("img");
      avatar.className = "skl-chat-avatar";
      avatar.src = message.avatar_url;
      avatar.alt = "Avatar";
      messageEl.appendChild(avatar);
    } else {
      const spacer = document.createElement("div");
      spacer.style.width = "28px";
      messageEl.appendChild(spacer);
    }

    const bodyWrap = document.createElement("div");
    bodyWrap.className = "skl-chat-body";

    const header = document.createElement("div");
    header.className = "skl-chat-header";
    const author = document.createElement("span");
    author.className = "skl-chat-author";
    author.textContent = message.author || "Unknown";
    const time = document.createElement("span");
    time.textContent = formatTime(message.created_at);
    header.appendChild(author);
    header.appendChild(time);

    const body = document.createElement("div");
    body.textContent = content;

    bodyWrap.appendChild(header);
    bodyWrap.appendChild(body);

    messageEl.appendChild(bodyWrap);
    chatLog.appendChild(messageEl);
    lastMessageId = Math.max(lastMessageId, message.id || 0);
    if (wasNearBottom) {
      chatLog.scrollTop = chatLog.scrollHeight;
    }
  };

  const loadMessages = async (initial = false) => {
    if (!chatLog) return;
    const url = new URL(`/api/lobbies/${lobbyId}/messages`, window.location.origin);
    if (!initial && lastMessageId) {
      url.searchParams.set("after_id", lastMessageId.toString());
    }
    const wasNearBottom = chatLog.scrollTop + chatLog.clientHeight >= chatLog.scrollHeight - 40;
    try {
      const response = await fetch(url.toString());
      if (!response.ok) return;
      const data = await response.json();
      (data.messages || []).forEach((message) => {
        lastMessageId = Math.max(lastMessageId, message.id || 0);
        renderMessage(message);
      });
      if (initial || wasNearBottom) {
        chatLog.scrollTop = chatLog.scrollHeight;
      }
    } catch {
      // ignore network errors for polling
    }
  };

  const joinLobby = async (password = "") => {
    if (!isAuthed) return;
    try {
      const response = await fetch(`/api/lobbies/${lobbyId}/join`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(password ? { password } : {}),
      });
      if (response.status === 403) {
        const data = await response.json().catch(() => ({}));
        if (data.reason === "ban") {
          handleKickNotice(data.error || "You are banned from this lobby.");
          return;
        }
        if (data.reason === "password") {
          setPasswordStatus(data.error || "Lobby password required.", true);
          return;
        }
      }
      if (!response.ok) return;
      isMember = true;
      if (passwordStatus) passwordStatus.textContent = "";
      if (passwordForm) passwordForm.style.display = "none";
      playSfx("join", 0.35);
      if (socket && socketConnected) {
        socket.emit("join_lobby", { lobby_id: lobbyId });
      }
    } catch {
      // ignore
    }
  };

  const loadMembers = async () => {
    if (!membersList) return;
    try {
      const response = await fetch(`/api/lobbies/${lobbyId}/members`);
      if (!response.ok) return;
      const data = await response.json();
      const members = data.members || [];
      renderMembers(members);
    } catch {
      // ignore
    }
  };

  // --- Context menu ---
  let ctxMenu = null;
  let ctxTarget = null;
  let ctxTargetEl = null;
  const buildContextMenu = () => {
    ctxMenu = document.createElement("div");
    ctxMenu.className = "skl-ctx-menu";
    ctxMenu.style.display = "none";
    document.body.appendChild(ctxMenu);
    document.addEventListener("click", () => hideContextMenu());
    document.addEventListener("contextmenu", (e) => {
      if (!ctxMenu || ctxMenu.style.display !== "block") return;
      if (ctxMenu.contains(e.target)) return;
      if (ctxTargetEl && (ctxTargetEl === e.target || ctxTargetEl.contains(e.target))) return;
      hideContextMenu();
    });
  };
  const hideContextMenu = () => {
    if (ctxMenu) {
      ctxMenu.style.display = "none";
      ctxTarget = null;
      ctxTargetEl = null;
    }
  };
  const showContextMenu = (e, member) => {
    e.preventDefault();
    if (!ctxMenu) buildContextMenu();
    ctxTarget = member;
    ctxTargetEl = e.currentTarget || e.target;
    ctxMenu.innerHTML = "";
    const isSelf = member.name && member.name === currentUserName;
    const isReleased = member.active_yaml_player && releasedPlayers.has(member.active_yaml_player);
    const canSocial = member.discord_id && window.SKL_SOCIAL;
    if ((isOwner || isSelf) && roomId && member.active_yaml_player) {
      addCtxItem(isSelf ? "Release me" : "Release player", () => {
        if (!isReleased) requestRelease(member.active_yaml_player, "release");
      });
      addCtxItem(isSelf ? "Slow release me" : "Slow release", () => {
        if (!isReleased) requestRelease(member.active_yaml_player, "slow");
      });
    }
    if (isOwner && !isSelf) {
      addCtxItem("Make host", () => transferHost(member.name));
    }
    addCtxItem("Player info", () => openPlayerInfo(member));
    if (isOwner) {
      addCtxItem("Kick from lobby", () => kickMember(member.name));
    }
    if (canSocial && !isSelf) {
      addCtxItem("Message", () => window.SKL_SOCIAL.openConversation(member.discord_id, member.name));
      addCtxItem("Add friend", () => window.SKL_SOCIAL.sendFriendRequest(member.discord_id));
    }
    addCtxItem("Report user", () => reportMember(member.name));
    ctxMenu.style.display = "block";
    const x = Math.min(e.clientX, window.innerWidth - 180);
    const y = Math.min(e.clientY, window.innerHeight - ctxMenu.offsetHeight - 10);
    ctxMenu.style.left = `${x}px`;
    ctxMenu.style.top = `${y}px`;
  };

  let activeYamlList = [];

  const syncYamlButton = (hasYaml) => {
    if (!yamlOpen) return;
    if (hasYaml) {
      yamlOpen.classList.add("has-yaml");
      yamlOpen.textContent = "Set Active YAML ✓";
    } else {
      yamlOpen.classList.remove("has-yaml");
      yamlOpen.textContent = "Set Active YAML";
    }
  };
  const updateActiveYamlLabel = () => {
    if (!activeYamlLabel) return;
    if (!activeYamlList || !activeYamlList.length) {
      activeYamlLabel.textContent = "Active YAML: -";
      syncYamlButton(false);
      return;
    }
    if (activeYamlList.length === 1) {
      const entry = activeYamlList[0];
      const label = entry.game || entry.title || "YAML";
      const customTag = entry.custom ? " (Custom)" : "";
      activeYamlLabel.textContent = `Active YAML: ${label}${customTag}`;
      syncYamlButton(true);
      return;
    }
    activeYamlLabel.textContent = `Active YAMLs: ${activeYamlList.length}`;
    syncYamlButton(true);
  };
  const renderActiveYamlList = () => {
    if (!activeYamlListEl) return;
    activeYamlListEl.innerHTML = "";
    if (!activeYamlList || !activeYamlList.length) {
      const empty = document.createElement("div");
      empty.className = "skl-empty-note";
      empty.textContent = "No active YAMLs yet.";
      activeYamlListEl.appendChild(empty);
      updateActiveYamlLabel();
      return;
    }
    activeYamlList.forEach((entry) => {
      const row = document.createElement("div");
      row.className = "skl-yaml-active-row";
      const text = document.createElement("div");
      text.className = "skl-yaml-active-text";
      const title = entry.title || entry.id || "YAML";
      const player = entry.player_name ? ` (${entry.player_name})` : "";
      const customTag = entry.custom ? " · Custom" : "";
      text.textContent = `${title}${player}${customTag}`;
      const removeBtn = document.createElement("button");
      removeBtn.type = "button";
      removeBtn.className = "skl-yaml-remove";
      removeBtn.textContent = "Remove";
      removeBtn.addEventListener("click", () => {
        setActiveYaml(entry.id, "remove");
      });
      row.appendChild(text);
      row.appendChild(removeBtn);
      activeYamlListEl.appendChild(row);
    });
    updateActiveYamlLabel();
    if (yamlStatus) {
      const hasCustom = activeYamlList.some((entry) => entry.custom);
      yamlStatus.textContent = hasCustom
        ? "Custom YAML active. This may cause generation errors."
        : "";
    }
  };
  const addCtxItem = (label, action) => {
    const item = document.createElement("div");
    item.className = "skl-ctx-item";
    item.textContent = label;
    item.addEventListener("click", (e) => { e.stopPropagation(); hideContextMenu(); action(); });
    ctxMenu.appendChild(item);
  };

  const releasePlayer = (playerName) => {
    requestRelease(playerName, "release");
  };
  const allowSlowRelease = (playerName) => {
    requestRelease(playerName, "slow");
  };
  const requestRelease = async (playerName, mode) => {
    if (releasedPlayers.has(playerName)) return;
    try {
      const response = await fetch(`/api/lobbies/${lobbyId}/release`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ player_name: playerName, mode }),
      });
      if (!response.ok) {
        const data = await response.json().catch(() => ({}));
        alert(data.error || "Release failed.");
        playSfx("error", 0.4);
        return;
      }
      playSfx("success", 0.35);
    } catch {
      alert("Release failed.");
      playSfx("error", 0.4);
    }
  };
  const transferHost = async (memberName) => {
    try {
      const response = await fetch(`/api/lobbies/${lobbyId}/transfer-host`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name: memberName }),
      });
      if (!response.ok) {
        const data = await response.json().catch(() => ({}));
        alert(data.error || "Unable to transfer host.");
        return;
      }
      const data = await response.json();
      if (ownerName && data.host_name) {
        ownerName.textContent = data.host_name;
      }
      updateHostStatus();
    } catch {
      alert("Unable to transfer host.");
    }
  };
  const kickMember = async (memberName) => {
    if (!isOwner) return;
    try {
      await fetch(`/api/lobbies/${lobbyId}/kick`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ name: memberName }),
      });
    } catch { /* ignore */ }
  };
  const reportMember = (memberName) => {
    alert(`Report for "${memberName}" noted. This feature is coming soon.`);
  };

  const openPlayerInfo = (member) => {
    if (!playerModal) return;
    const name = member && member.name ? member.name : "Unknown";
    let yamlDisplay = "No YAML selected";
    if (member && Array.isArray(member.active_yamls) && member.active_yamls.length) {
      yamlDisplay = member.active_yamls
        .map((entry) => `${entry.title || entry.id}${entry.player_name ? ` (${entry.player_name})` : ""}`)
        .join(", ");
    } else if (member && member.active_yaml_title) {
      const yamlPlayer = member.active_yaml_player ? ` (${member.active_yaml_player})` : "";
      yamlDisplay = `${member.active_yaml_title}${yamlPlayer}`;
    }
    if (playerNameLabel) playerNameLabel.textContent = name;
    if (playerYamlLabel) playerYamlLabel.textContent = yamlDisplay;
    openPanel(playerModal);
  };

  const setPasswordStatus = (message, isError = false) => {
    if (!passwordStatus) return;
    passwordStatus.textContent = message || "";
    passwordStatus.style.color = isError ? "#ffb3b3" : "";
  };

  const setSettingsStatus = (message, isError = false) => {
    if (!settingsStatus) return;
    settingsStatus.textContent = message || "";
    settingsStatus.style.color = isError ? "#ffb3b3" : "";
  };

  const setSettingsLocked = (locked) => {
    if (!settingsForm) return;
    settingsForm.querySelectorAll("select, input, button").forEach((el) => {
      if (el.id === "lobby-generate" || el.id === "lobby-generate-confirm") return;
      if (el.getAttribute("data-keep-enabled") === "1") return;
      if (locked) {
        el.setAttribute("disabled", "disabled");
      } else {
        if (settingsForm.dataset.owner === "1") {
          el.removeAttribute("disabled");
        }
      }
    });
  };

  const closeTrackerDrawer = () => {
    if (!trackerDrawer) return;
    trackerDrawer.classList.remove("open");
    trackerDrawer.setAttribute("aria-hidden", "true");
    if (trackerFrame) trackerFrame.removeAttribute("src");
  };

  const openTrackerDrawer = (url, label, fullUrl) => {
    if (!trackerDrawer || !trackerFrame) {
      window.open(url, "_blank", "noopener");
      return;
    }
    trackerDrawer.classList.add("open");
    trackerDrawer.setAttribute("aria-hidden", "false");
    trackerFrame.src = url;
    if (trackerTitle) trackerTitle.textContent = label || "Tracker";
    if (trackerOpenNew) trackerOpenNew.href = fullUrl || url;
  };

  if (trackerDrawer) {
    trackerDrawer.querySelectorAll("[data-tracker-close]").forEach((btn) => {
      btn.addEventListener("click", closeTrackerDrawer);
    });
    document.addEventListener("keydown", (event) => {
      if (event.key === "Escape") closeTrackerDrawer();
    });
  }

  const setHostVoteStatus = (message, isError = false) => {
    if (!hostVoteStatus) return;
    hostVoteStatus.textContent = message || "";
    hostVoteStatus.style.color = isError ? "#ffb3b3" : "";
  };

  const updateHostStatus = async () => {
    if (!hostStatus) return;
    try {
      const response = await fetch(`/api/lobbies/${lobbyId}/host-status`);
      if (!response.ok) return;
      const data = await response.json();
      if (ownerName && data.host_name) {
        ownerName.textContent = data.host_name;
      }
      if (hostPresence) {
        if (data.host_absent) {
          const mins = Math.floor((data.host_absent_seconds || 0) / 60);
          hostPresence.textContent = `Absent (${mins} min)`;
          hostPresence.classList.add("absent");
        } else {
          hostPresence.textContent = "Active";
          hostPresence.classList.remove("absent");
        }
      }
      if (hostCandidate) {
        hostCandidate.textContent = data.candidate_name || "-";
      }
      if (hostActions) {
        const isOwnerView = hostActions.dataset.owner === "1";
        const canVote = data.host_absent && !isOwnerView && data.candidate_name;
        hostActions.style.display = canVote ? "flex" : "none";
        if (hostVoteButton) {
          hostVoteButton.disabled = !canVote;
        }
        if (hostVoteStatus && data.vote_needed) {
          hostVoteStatus.textContent = `${data.vote_count || 0}/${data.vote_needed} votes`;
        }
      }
    } catch {
      // ignore
    }
  };

  const openGenerateModal = () => {
    if (!generateModal) return;
    generateModal.classList.add("open");
    generateModal.setAttribute("aria-hidden", "false");
  };

  const closeGenerateModal = () => {
    if (!generateModal) return;
    generateModal.classList.remove("open");
    generateModal.setAttribute("aria-hidden", "true");
  };

  const openCloseModal = () => {
    if (!closeModal) return;
    closeModal.classList.add("open");
    closeModal.setAttribute("aria-hidden", "false");
  };

  const closeCloseModal = () => {
    if (!closeModal) return;
    closeModal.classList.remove("open");
    closeModal.setAttribute("aria-hidden", "true");
  };

  const applySettings = (data) => {
    if (!settingsForm || !data) return;
    const setSelect = (name, value) => {
      const field = settingsForm.querySelector(`select[name="${name}"]`);
      if (field && value !== undefined && value !== null) {
        field.value = value;
      }
    };
    const setInput = (name, value) => {
      const field = settingsForm.querySelector(`input[name="${name}"]`);
      if (field && value !== undefined && value !== null) {
        field.value = value;
      }
    };
    const setCheckbox = (name, value) => {
      const field = settingsForm.querySelector(`input[name="${name}"]`);
      if (field && value !== undefined && value !== null) {
        field.checked = Boolean(value);
      }
    };
    setSelect("release_mode", data.release_mode);
    setSelect("collect_mode", data.collect_mode);
    setSelect("remaining_mode", data.remaining_mode);
    setSelect("countdown_mode", data.countdown_mode);
    setSelect("item_cheat", data.item_cheat ? "1" : "0");
    setSelect("spoiler", `${data.spoiler ?? 0}`);
    setInput("hint_cost", data.hint_cost);
    setCheckbox("plando_items", data.plando_items);
    setCheckbox("plando_bosses", data.plando_bosses);
    setCheckbox("plando_connections", data.plando_connections);
    setCheckbox("plando_texts", data.plando_texts);
    if (typeof data.allow_custom_yamls === "boolean") {
      allowCustomYamls = data.allow_custom_yamls;
      const field = settingsForm.querySelector('select[name="allow_custom_yamls"]');
      if (field) field.value = allowCustomYamls ? "1" : "0";
    }
    if (typeof data.is_private === "boolean") {
      isPrivate = data.is_private;
      lobbyRoot.dataset.private = data.is_private ? "1" : "0";
      if (!isPrivate && !isMember) {
        joinLobby();
      }
      if (!isPrivate && passwordForm) {
        passwordForm.style.display = "none";
      }
    }
  };

  const handleKickNotice = (message) => {
    const notice = document.createElement("div");
    notice.className = "skl-kick-banner";
    notice.textContent = message || "You were removed from this lobby.";
    if (lobbyRoot && !document.querySelector(".skl-kick-banner")) {
      lobbyRoot.prepend(notice);
    }
    setTimeout(() => {
      window.location.href = "/";
    }, 1400);
  };

  const markPlayerReleased = (playerName) => {
    if (!playerName) return;
    if (releasedPlayers.has(playerName)) return;
    releasedPlayers.add(playerName);
    renderMembers(currentMembers);
    updateRoomInfoBox();
  };

  const processTerminalLine = (line) => {
    const releaseMatch = line.match(/Notice \\(all\\): (.+) \\(Team #\\d+\\) has released all remaining items from their world\\./);
    if (releaseMatch && releaseMatch[1]) {
      markPlayerReleased(releaseMatch[1].trim());
    }
  };

  buildContextMenu();

  if (passwordForm) {
    passwordForm.addEventListener("submit", (event) => {
      event.preventDefault();
      if (!passwordInput) return;
      const password = passwordInput.value.trim();
      if (!password) {
        setPasswordStatus("Lobby password required.", true);
        return;
      }
      joinLobby(password);
    });
  }

  if (settingsForm && settingsForm.dataset.owner === "1") {
    settingsForm.addEventListener("submit", async (event) => {
      event.preventDefault();
      setSettingsStatus("");
      const formData = new FormData(settingsForm);
      const payload = {
        release_mode: (formData.get("release_mode") || "").toString(),
        collect_mode: (formData.get("collect_mode") || "").toString(),
        remaining_mode: (formData.get("remaining_mode") || "").toString(),
        countdown_mode: (formData.get("countdown_mode") || "").toString(),
        item_cheat: formData.get("item_cheat") === "1",
        spoiler: (formData.get("spoiler") || "0").toString(),
        hint_cost: (formData.get("hint_cost") || "5").toString(),
        plando_items: formData.get("plando_items") === "on",
        plando_bosses: formData.get("plando_bosses") === "on",
        plando_connections: formData.get("plando_connections") === "on",
        plando_texts: formData.get("plando_texts") === "on",
        allow_custom_yamls: formData.get("allow_custom_yamls") !== "0",
        clear_password: formData.get("clear_password") === "on",
      };
      const serverPassword = (formData.get("server_password") || "").toString().trim();
      if (serverPassword) {
        payload.server_password = serverPassword;
      }
      try {
        const response = await fetch(`/api/lobbies/${lobbyId}/settings`, {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(payload),
        });
        if (!response.ok) {
          const data = await response.json().catch(() => ({}));
          setSettingsStatus(data.error || "Failed to save settings.", true);
          return;
        }
        const data = await response.json();
        if (data && data.settings) {
          applySettings(data.settings);
        }
        setSettingsStatus("Settings saved.");
        if (passwordInput) passwordInput.value = "";
      } catch {
        setSettingsStatus("Failed to save settings.", true);
      }
    });
  }

  const renderMembers = (members) => {
    if (!membersList) return;
    membersList.innerHTML = "";
    currentMembers = members;
    let selfActiveYamls = null;
    const membersEmpty = document.querySelector("#lobby-members-empty");
    if (membersEmpty) {
      membersEmpty.textContent = members.length ? "" : "No members yet.";
    }
    let allReady = members.length > 0;
    members.forEach((member) => {
      if (!member.ready) allReady = false;
    });
    members.forEach((member) => {
      if (member.name && member.name === currentUserName && Array.isArray(member.active_yamls)) {
        selfActiveYamls = member.active_yamls;
      }
      if (member.name && member.name === currentUserName) {
        readyState = Boolean(member.ready);
      }
      const row = document.createElement("div");
      row.className = "skl-member-row";
      row.addEventListener("contextmenu", (e) => showContextMenu(e, member));

      const left = document.createElement("div");
      left.className = "skl-member-left";

      const isHost = ownerName && ownerName.textContent.trim() === (member.name || "").trim();
      if (isHost) {
        const crown = document.createElement("span");
        crown.className = "skl-member-crown";
        crown.textContent = "👑";
        left.appendChild(crown);
      }

      const name = document.createElement("div");
      name.className = "skl-member-name";
      name.textContent = member.name || "Unknown";
      left.appendChild(name);

      let yamlInfo = "YAML: not set";
      if (Array.isArray(member.active_yamls) && member.active_yamls.length) {
        yamlInfo = member.active_yamls
          .map((entry) => `${entry.title || entry.id}${entry.player_name ? ` (${entry.player_name})` : ""}${entry.custom ? " · Custom" : ""}`)
          .join(", ");
      } else if (member.active_yaml_title) {
        yamlInfo = `YAML: ${member.active_yaml_title} (${member.active_yaml_game || "Game"})`;
      }
      const yamlIcon = document.createElement("span");
      yamlIcon.className = "skl-member-yaml-icon";
      yamlIcon.title = yamlInfo;
      yamlIcon.textContent = "Y";
      left.appendChild(yamlIcon);

      const actions = document.createElement("div");
      actions.className = "skl-member-actions";
      const isReleased = member.active_yaml_player && releasedPlayers.has(member.active_yaml_player);

      if ((isOwner || (member.name && member.name === currentUserName)) && member.active_yaml_player && roomId) {
        const releaseBtn = document.createElement("button");
        releaseBtn.className = "skl-btn-release";
        releaseBtn.type = "button";
        releaseBtn.textContent = "Release";
        releaseBtn.addEventListener("click", (e) => {
          e.stopPropagation();
          if (isOwner) releasePlayer(member.active_yaml_player);
          else requestRelease(member.active_yaml_player, "release");
        });
        if (isReleased) releaseBtn.disabled = true;
        actions.appendChild(releaseBtn);

        const slowBtn = document.createElement("button");
        slowBtn.className = "skl-btn-release slow";
        slowBtn.type = "button";
        slowBtn.textContent = "Slow release";
        slowBtn.addEventListener("click", (e) => {
          e.stopPropagation();
          if (isOwner) allowSlowRelease(member.active_yaml_player);
          else requestRelease(member.active_yaml_player, "slow");
        });
        if (isReleased) slowBtn.disabled = true;
        actions.appendChild(slowBtn);
      }

      if (roomInfo && roomInfo.tracker_id && member.active_yaml_player) {
        const slotId = playersByName.get(member.active_yaml_player) || playersByName.get((member.active_yaml_player || "").toLowerCase());
        if (slotId) {
          const trackerBase = `/tracker/${roomInfo.tracker_id}/0/${slotId}`;
          const trackerUrl = `${trackerBase}?embed=1&theme=purple`;
          const trackerBtn = document.createElement("button");
          trackerBtn.className = "skl-btn-release";
          trackerBtn.type = "button";
          trackerBtn.textContent = "Tracker";
          trackerBtn.addEventListener("click", (e) => {
            e.stopPropagation();
            openTrackerDrawer(trackerUrl, `${member.name || "Player"} tracker`, trackerBase);
          });
          actions.appendChild(trackerBtn);
        }
      }

      const right = document.createElement("div");
      right.className = "skl-member-right";

      const readyWrap = document.createElement("div");
      readyWrap.className = "skl-member-ready";

      if (member.name && member.name === currentUserName) {
        const readyBtn = document.createElement("button");
        readyBtn.className = "skl-btn-release";
        readyBtn.type = "button";
        readyBtn.textContent = member.ready ? "Unready" : "Ready";
        readyBtn.addEventListener("click", (e) => {
          e.stopPropagation();
          toggleReady();
        });
        readyWrap.appendChild(readyBtn);
      }

      const badge = document.createElement("span");
      if (isReleased) {
        badge.className = "skl-ready-badge completed";
        badge.textContent = "Completed";
      } else {
        badge.className = `skl-ready-badge ${member.ready ? "" : "not-ready"}`;
        badge.textContent = member.ready ? "Ready" : "Not ready";
      }
      readyWrap.appendChild(badge);

      const downloadBtn = document.createElement("a");
      downloadBtn.className = "skl-member-download";
      if (roomInfo && member.active_yaml_player) {
        const slotId = playersByName.get(member.active_yaml_player) || playersByName.get((member.active_yaml_player || "").toLowerCase());
        const download = slotId ? downloadsBySlot.get(slotId) : null;
        if (download) {
          downloadBtn.href = download;
          downloadBtn.title = "Download patch";
          downloadBtn.textContent = "⬇";
        } else {
          downloadBtn.classList.add("disabled");
          downloadBtn.textContent = "⬇";
          downloadBtn.title = "No patch available";
        }
      } else {
        downloadBtn.classList.add("disabled");
        downloadBtn.textContent = "⬇";
        downloadBtn.title = "No patch available";
      }

      if (actions.childElementCount) {
        right.appendChild(actions);
      }
      right.appendChild(readyWrap);
      right.appendChild(downloadBtn);

      row.appendChild(left);
      row.appendChild(right);
      membersList.appendChild(row);
    });
    if (selfActiveYamls) {
      activeYamlList = selfActiveYamls;
      renderActiveYamlList();
    }
    updateReadyUI();
    updateGenerateStatus(allReady);
  };

  const isAllReady = () => {
    if (!currentMembers.length) return false;
    return currentMembers.every((member) => member.ready);
  };

  const updateGenerateStatus = (allReady) => {
    if (!generateButton || !isOwner) return;
    const shouldDisable = generationLocked || !allReady;
    generateButton.disabled = shouldDisable;
    if (generateStatus) {
      if (generationState === "in_progress") {
        generateStatus.textContent = "Generation in progress…";
      } else if (generationState === "queued") {
        generateStatus.textContent = "Generation queued.";
      } else if (generationState === "done") {
        generateStatus.textContent = "Generation complete.";
      } else if (generationState === "error") {
        generateStatus.textContent = "Generation failed.";
      } else {
        generateStatus.textContent = allReady ? "All members ready." : "Waiting for all members to be ready.";
      }
    }
    if (generateBoxState) {
      if (generationState === "in_progress") {
        generateBoxState.textContent = "In progress";
      } else if (generationState === "queued") {
        generateBoxState.textContent = "Queued";
      } else if (generationState === "done") {
        generateBoxState.textContent = "Complete";
      } else if (generationState === "error") {
        generateBoxState.textContent = "Error";
      } else {
        generateBoxState.textContent = allReady ? "Ready" : "Waiting";
      }
    }
  };

  const updateRoomInfoBox = () => {
    if (!roomInfo) return;
    if (roomInfoLink) {
      roomInfoLink.innerHTML = roomInfo.room_url
        ? `<a href="${roomInfo.room_url}">Open room</a>`
        : "Not generated";
    }
    if (seedInfoLink) {
      seedInfoLink.innerHTML = roomInfo.seed_url
        ? `<a href="${roomInfo.seed_url}">View seed</a>`
        : "-";
    }
    if (roomInfoServer) {
      if (roomInfo.last_port) {
        roomInfoServer.textContent = `${window.location.host}:${roomInfo.last_port}`;
      } else {
        roomInfoServer.textContent = "Starting…";
      }
    }
    if (seedTimer) {
      if (!seedCompletedAt) {
        seedTimer.textContent = "-";
      }
    }
    if (roomPlayers) {
      roomPlayers.textContent = roomInfo.players_total ? `${roomInfo.players_total} players` : "-";
    }
    if (roomChecks) {
      if (trackerStats && trackerStats.total_locations) {
        const done = trackerStats.checks_done || 0;
        const total = trackerStats.total_locations || 0;
        const percent = total ? Math.round((done / total) * 100) : 0;
        roomChecks.textContent = `${percent}% (${done}/${total})`;
      } else {
        roomChecks.textContent = "-";
      }
    }
    if (roomComplete) {
      const trackerCompleted = trackerStats && trackerStats.completed_players !== null
        ? trackerStats.completed_players
        : 0;
      const releasedCompleted = releasedPlayers.size;
      const completedTotal = Math.max(trackerCompleted, releasedCompleted);
      if (roomInfo.players_total) {
        roomComplete.textContent = `${completedTotal}/${roomInfo.players_total}`;
      } else {
        roomComplete.textContent = completedTotal ? `${completedTotal}` : "-";
      }
    }
  };

  const formatDuration = (seconds) => {
    const safe = Math.max(0, Math.floor(seconds));
    const hrs = Math.floor(safe / 3600);
    const mins = Math.floor((safe % 3600) / 60);
    const secs = safe % 60;
    const parts = [];
    if (hrs) parts.push(`${hrs}h`);
    parts.push(`${mins}m`);
    parts.push(`${secs}s`);
    return parts.join(" ");
  };

  const updateSeedTimer = () => {
    if (!seedTimer) return;
    if (!seedCompletedAt) {
      seedTimer.textContent = "-";
      return;
    }
    const diff = (Date.now() - seedCompletedAt.getTime()) / 1000;
    seedTimer.textContent = formatDuration(diff);
  };

  const setSeedCompletedAt = (value) => {
    if (!value) {
      seedCompletedAt = null;
      updateSeedTimer();
      return;
    }
    const parsed = new Date(value);
    if (Number.isNaN(parsed.getTime())) return;
    seedCompletedAt = parsed;
    updateSeedTimer();
    if (!seedTimerTick) {
      seedTimerTick = setInterval(updateSeedTimer, 1000);
    }
  };

  let terminalHasContent = false;
  const appendTerminalText = (text) => {
    if (!terminalLog) return;
    if (!text) return;
    if (!terminalHasContent) {
      terminalLog.textContent = "";
      terminalHasContent = true;
    }
    terminalLog.textContent += text;
    terminalBuffer += text;
    const lines = terminalBuffer.split("\n");
    terminalBuffer = lines.pop() || "";
    lines.forEach((line) => {
      processTerminalLine(line);
    });
    requestAnimationFrame(() => {
      terminalLog.scrollTop = terminalLog.scrollHeight;
    });
  };

  const fetchTrackerStats = async (trackerId) => {
    try {
      const [dynamicResp, staticResp] = await Promise.all([
        fetch(`/api/tracker/${trackerId}`),
        fetch(`/api/static_tracker/${trackerId}`),
      ]);
      if (!dynamicResp.ok || !staticResp.ok) return;
      const dynamicData = await dynamicResp.json();
      const staticData = await staticResp.json();
      const totalChecksEntry = (dynamicData.total_checks_done || []).find((entry) => entry.team === 0);
      const playerStatus = (dynamicData.player_status || []).filter((entry) => entry.team === 0);
      const completedPlayers = playerStatus.filter((entry) => entry.status === 30).length;
      const totalLocations = (staticData.player_locations_total || [])
        .filter((entry) => entry.team === 0)
        .reduce((sum, entry) => sum + (entry.total_locations || 0), 0);
      trackerStats = {
        checks_done: totalChecksEntry ? totalChecksEntry.checks_done : 0,
        total_locations: totalLocations,
        completed_players: completedPlayers,
      };
      updateRoomInfoBox();
    } catch {
      // ignore
    }
  };

  const fetchRoomStatus = async (roomUrl, seedUrl) => {
    if (!roomUrl) return;
    const parts = roomUrl.split("/").filter(Boolean);
    const nextRoomId = parts[parts.length - 1];
    if (roomId !== nextRoomId) {
      roomId = nextRoomId;
      if (terminalLog) terminalLog.textContent = "Loading room output…";
      if (socket) {
        socket.emit("watch_room", { lobby_id: lobbyId, room_id: roomId });
      }
    }
    try {
      const response = await fetch(`/api/room_status/${roomId}`);
      if (!response.ok) return;
      const data = await response.json();
      roomInfo = {
        room_url: roomUrl,
        seed_url: seedUrl || "",
        last_port: data.last_port,
        tracker_id: data.tracker,
        players_total: (data.players || []).length,
      };
      downloadsBySlot = new Map((data.downloads || []).map((entry) => [entry.slot, entry.download]));
      playersByName = new Map();
      (data.players || []).forEach((player) => {
        playersByName.set(player.name, player.slot);
        playersByName.set((player.name || "").toLowerCase(), player.slot);
      });
      updateRoomInfoBox();
      if (data.tracker) {
        fetchTrackerStats(data.tracker);
      }
      loadMembers();
      if (terminalLog && roomId) {
        if (!terminalLog.textContent || terminalLog.textContent.startsWith("Waiting")) {
          terminalLog.textContent = "Loading room output…";
        }
      }
    } catch {
      // ignore
    }
  };

  const pollGeneration = async () => {
    try {
      const response = await fetch(`/api/lobbies/${lobbyId}/generation`);
      if (!response.ok) return;
      const data = await response.json();
      if (!data) return;
      if (data.status === "error") {
        generationState = "error";
        generationLocked = false;
        setSettingsLocked(false);
        setSeedCompletedAt(null);
      } else if (data.status === "done") {
        generationState = "done";
        generationLocked = true;
        setSettingsLocked(true);
        setSeedCompletedAt(data.completed_at);
      } else if (data.status === "queued") {
        generationState = "queued";
        generationLocked = true;
        setSettingsLocked(true);
      } else if (data.status === "none") {
        generationState = "idle";
        generationLocked = false;
        setSettingsLocked(false);
        setSeedCompletedAt(null);
      } else {
        generationState = "in_progress";
        generationLocked = true;
        setSettingsLocked(true);
      }
      if (data.room_url) {
        fetchRoomStatus(data.room_url, data.seed_url || "");
      }
      updateGenerateStatus(isAllReady());
    } catch {
      // ignore
    }
  };

  const startPolling = () => {
    if (pollTimer) return;
    pollTimer = setInterval(() => {
      loadMessages();
      loadMembers();
      pollGeneration();
      updateHostStatus();
    }, 4000);
  };

  const stopPolling = () => {
    if (pollTimer) {
      clearInterval(pollTimer);
      pollTimer = null;
    }
  };

  const isCustomYaml = (yamlId) => {
    if (!yamlSelect || !yamlId) return false;
    const option = yamlSelect.querySelector(`option[value="${yamlId}"]`);
    return option && option.dataset.custom === "1";
  };

  const setActiveYaml = async (yamlId, action = "add") => {
    if (!isAuthed) {
      alert("Sign in to select a YAML.");
      return;
    }
    if (action === "add" || action === "set") {
      const custom = isCustomYaml(yamlId);
      if (custom && !allowCustomYamls) {
        if (yamlStatus) {
          yamlStatus.textContent = "Custom YAMLs are blocked in this lobby.";
        }
        return;
      }
    }
    try {
      const response = await fetch(`/api/lobbies/${lobbyId}/active-yaml`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ yaml_id: yamlId, action }),
      });
      if (!response.ok) {
        const data = await response.json().catch(() => ({}));
        if (yamlStatus) yamlStatus.textContent = data.error || "Failed to set YAML.";
        return;
      }
      const data = await response.json().catch(() => ({}));
      if (data && Array.isArray(data.active_yamls)) {
        activeYamlList = data.active_yamls;
        renderActiveYamlList();
      }
      await loadMembers();
    } catch {
      // ignore
    }
  };

  const updateReadyUI = () => {
    if (readyToggle) {
      readyToggle.textContent = readyState ? "Ready ✓" : "Ready";
      readyToggle.classList.toggle("ghost", readyState);
      readyToggle.classList.toggle("primary", !readyState);
    }
    if (readyStatus) {
      readyStatus.textContent = readyState ? "Status: Ready" : "Status: Not ready";
    }
  };

  const toggleReady = async () => {
    if (!isAuthed) {
      alert("Sign in to ready up.");
      return;
    }
    if (!activeYamlList.length && (!yamlSelect || !yamlSelect.value)) {
      if (yamlStatus) yamlStatus.textContent = "Select an active YAML before readying up.";
      openPanel(yamlModal);
      return;
    }
    try {
      const response = await fetch(`/api/lobbies/${lobbyId}/ready`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ ready: !readyState }),
      });
      const data = await response.json().catch(() => ({}));
      if (!response.ok) {
        if (readyStatus && data.error) {
          readyStatus.textContent = data.error;
        }
        return;
      }
      readyState = Boolean(data.ready);
      updateReadyUI();
      playSfx(readyState ? "ready" : "unready", 0.35);
      await loadMembers();
    } catch {
      // ignore
    }
  };

  const sendChat = async () => {
    if (!isAuthed || !chatInput) return;
    const content = chatInput.value.trim();
    if (!content) return;
    chatHistory.unshift(content);
    if (chatHistory.length > chatHistoryMax) chatHistory.pop();
    chatHistoryIndex = -1;
    chatDraft = "";
    chatInput.value = "";
    if (socket) {
      socket.emit("chat_send", { lobby_id: lobbyId, content });
      return;
    }
    try {
      const response = await fetch(`/api/lobbies/${lobbyId}/messages`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ content }),
      });
      if (!response.ok) return;
      await loadMessages();
    } catch {
      // ignore
    }
  };

  if (chatForm && chatInput) {
    if (!isAuthed) {
      chatInput.disabled = true;
      chatInput.placeholder = "Sign in to chat.";
      const submitButton = chatForm.querySelector("button");
      if (submitButton) submitButton.disabled = true;
    }
    chatForm.addEventListener("submit", (event) => {
      event.preventDefault();
      sendChat();
    });
    chatInput.addEventListener("keydown", (event) => {
      if (event.key === "Enter" && !event.shiftKey) {
        event.preventDefault();
        sendChat();
        return;
      }
      if (event.key === "ArrowUp") {
        if (chatHistoryIndex < chatHistory.length - 1) {
          if (chatHistoryIndex === -1) chatDraft = chatInput.value;
          chatHistoryIndex++;
          chatInput.value = chatHistory[chatHistoryIndex];
          event.preventDefault();
        }
        return;
      }
      if (event.key === "ArrowDown") {
        if (chatHistoryIndex > -1) {
          chatHistoryIndex--;
          chatInput.value = chatHistoryIndex === -1 ? chatDraft : chatHistory[chatHistoryIndex];
          event.preventDefault();
        }
        return;
      }
    });
  }

  if (yamlSelect) {
    if (activeYamlId) {
      yamlSelect.value = activeYamlId;
    }
    if (activeYamlId) {
      const selectedOption = yamlSelect.options[yamlSelect.selectedIndex];
      activeYamlList = [
        {
          id: activeYamlId,
          title: selectedOption ? selectedOption.dataset.title || selectedOption.textContent : activeYamlId,
          game: selectedOption ? selectedOption.dataset.game || "" : "",
          player_name: selectedOption ? selectedOption.dataset.player || "" : "",
          custom: selectedOption ? selectedOption.dataset.custom === "1" : false,
        },
      ];
    }
    renderActiveYamlList();
    if (!isAuthed) {
      yamlSelect.disabled = true;
      if (yamlAddButton) yamlAddButton.disabled = true;
    }
  }

  if (readyToggle) {
    if (!isAuthed) {
      readyToggle.disabled = true;
    }
    readyToggle.addEventListener("click", toggleReady);
  }

  if (generateButton && isOwner) {
    generateButton.addEventListener("click", () => {
      if (generationLocked) return;
      openGenerateModal();
    });
  }

  if (generateConfirm && isOwner) {
    generateConfirm.addEventListener("click", async () => {
      closeGenerateModal();
      generationState = "in_progress";
      generationLocked = true;
      setSettingsLocked(true);
      generateButton.disabled = true;
      if (generateStatus) generateStatus.textContent = "Generating…";
      if (generateBoxState) generateBoxState.textContent = "In progress";
      try {
        const response = await fetch(`/api/lobbies/${lobbyId}/generate`, { method: "POST" });
        const data = await response.json().catch(() => ({}));
        if (!response.ok) {
          generationState = "error";
          generationLocked = false;
          setSettingsLocked(false);
          if (generateStatus) generateStatus.textContent = data.error || "Generation failed.";
          if (generateBoxState) generateBoxState.textContent = "Error";
          generateButton.disabled = false;
          return;
        }
      } catch {
        generationState = "error";
        generationLocked = false;
        setSettingsLocked(false);
        if (generateStatus) generateStatus.textContent = "Generation failed.";
        if (generateBoxState) generateBoxState.textContent = "Error";
        generateButton.disabled = false;
      }
    });
  }

  if (generateCancel) {
    generateCancel.addEventListener("click", () => closeGenerateModal());
  }

  if (modalCloseButtons && modalCloseButtons.length) {
    modalCloseButtons.forEach((btn) => {
      btn.addEventListener("click", () => closeGenerateModal());
    });
  }

  if (closeLobbyButton) {
    closeLobbyButton.addEventListener("click", () => openCloseModal());
  }

  if (closeCancel) {
    closeCancel.addEventListener("click", () => closeCloseModal());
  }

  if (closeModalButtons && closeModalButtons.length) {
    closeModalButtons.forEach((btn) => {
      btn.addEventListener("click", () => closeCloseModal());
    });
  }

  if (settingsOpen) {
    settingsOpen.addEventListener("click", () => openPanel(settingsModal));
  }

  if (settingsCloseButtons && settingsCloseButtons.length) {
    settingsCloseButtons.forEach((btn) => {
      btn.addEventListener("click", () => closePanel(settingsModal));
    });
  }

  if (yamlCloseButtons && yamlCloseButtons.length) {
    yamlCloseButtons.forEach((btn) => {
      btn.addEventListener("click", () => closePanel(yamlModal));
    });
  }

  if (playerCloseButtons && playerCloseButtons.length) {
    playerCloseButtons.forEach((btn) => {
      btn.addEventListener("click", () => closePanel(playerModal));
    });
  }

  if (yamlOpen) {
    yamlOpen.addEventListener("click", () => openPanel(yamlModal));
  }

  if (terminalOpen) {
    terminalOpen.addEventListener("click", () => openPanel(terminalDrawer));
  }

  if (terminalCloseButtons && terminalCloseButtons.length) {
    terminalCloseButtons.forEach((btn) => {
      btn.addEventListener("click", () => closePanel(terminalDrawer));
    });
  }

  if (closeConfirm) {
    closeConfirm.addEventListener("click", async () => {
      closeCloseModal();
      try {
        const response = await fetch(`/api/lobbies/${lobbyId}/close`, { method: "POST" });
        if (!response.ok) {
          const data = await response.json().catch(() => ({}));
          if (settingsStatus) {
            settingsStatus.textContent = data.error || "Failed to close lobby.";
            settingsStatus.style.color = "#ffb3b3";
          }
          return;
        }
        window.location.href = "/";
      } catch {
        if (settingsStatus) {
          settingsStatus.textContent = "Failed to close lobby.";
          settingsStatus.style.color = "#ffb3b3";
        }
      }
    });
  }

  if (hostVoteButton) {
    hostVoteButton.addEventListener("click", async () => {
      setHostVoteStatus("");
      try {
        const response = await fetch(`/api/lobbies/${lobbyId}/vote-host`, { method: "POST" });
        const data = await response.json().catch(() => ({}));
        if (!response.ok) {
          setHostVoteStatus(data.error || "Unable to vote.", true);
          return;
        }
        if (data.status === "closed") {
          window.location.href = "/";
          return;
        }
        setHostVoteStatus(`${data.vote_count || 0}/${data.vote_needed || 0} votes`);
      } catch {
        setHostVoteStatus("Unable to vote.", true);
      }
    });
  }

  if (terminalForm && terminalInput && isOwner) {
    terminalForm.addEventListener("submit", async (event) => {
      event.preventDefault();
      if (!roomId) {
        if (terminalStatus) terminalStatus.textContent = "Room not ready yet.";
        return;
      }
      const cmd = terminalInput.value.trim();
      if (!cmd) return;
      if (socket) {
        socket.emit("terminal_command", { lobby_id: lobbyId, room_id: roomId, cmd });
        terminalInput.value = "";
        if (terminalStatus) terminalStatus.textContent = "Command sent.";
      }
    });
  }

  updateReadyUI();
  if (yamlAddButton) {
    yamlAddButton.addEventListener("click", () => {
      const yamlId = yamlSelect ? yamlSelect.value : "";
      if (!yamlId) {
        if (yamlStatus) yamlStatus.textContent = "Select a YAML first.";
        return;
      }
      setActiveYaml(yamlId, "add");
    });
  }

  if (!isPrivate || isMember) {
    joinLobby();
  } else if (passwordStatus && isAuthed) {
    setPasswordStatus("Enter the lobby password to join.", false);
  }
  loadMessages(true);
  loadMembers();
  updateHostStatus();

  if (socket) {
    socket.on("connect", () => {
      socketConnected = true;
      stopPolling();
      if (!isPrivate || isMember) {
        socket.emit("join_lobby", { lobby_id: lobbyId });
      }
    });
    socket.on("disconnect", () => {
      socketConnected = false;
      startPolling();
    });
    socket.on("connect_error", () => {
      socketConnected = false;
      startPolling();
    });
    socket.on("lobby_message", (data) => {
      renderMessage(data);
    });
    socket.on("lobby_kicked", (data) => {
      const message = data && data.message ? data.message : "You were removed from this lobby.";
      handleKickNotice(message);
      playSfx("error", 0.4);
    });
    socket.on("lobby_auth_required", (data) => {
      const message = data && data.message ? data.message : "Sign in to join this lobby.";
      const redirectUrl = data && data.redirect_url ? data.redirect_url : "/auth";
      const notice = document.createElement("div");
      notice.className = "skl-kick-banner";
      notice.textContent = message;
      if (lobbyRoot && !document.querySelector(".skl-kick-banner")) {
        lobbyRoot.prepend(notice);
      }
      setTimeout(() => {
        window.location.href = redirectUrl;
      }, 1400);
    });
    socket.on("lobby_password_required", (data) => {
      const message = data && data.message ? data.message : "Lobby password required.";
      setPasswordStatus(message, true);
    });
    socket.on("lobby_closed", (data) => {
      const message = data && data.message ? data.message : "Lobby closed by the host.";
      handleKickNotice(message);
      playSfx("error", 0.4);
    });
    socket.on("lobby_host_changed", (data) => {
      if (ownerName && data && data.host_name) {
        ownerName.textContent = data.host_name;
      }
      updateHostStatus();
    });
    socket.on("lobby_settings", (data) => {
      applySettings(data);
    });
    socket.on("members_update", (data) => {
      renderMembers(data.members || []);
    });
    socket.on("generation_update", (data) => {
      if (!data) return;
      if (data.status === "error") {
        generationState = "error";
        generationLocked = false;
        setSettingsLocked(false);
        setSeedCompletedAt(null);
        playSfx("error", 0.4);
      } else if (data.status === "done") {
        generationState = "done";
        generationLocked = true;
        setSettingsLocked(true);
        setSeedCompletedAt(data.completed_at);
        playSfx("success", 0.35);
      } else if (data.status === "queued") {
        generationState = "queued";
        generationLocked = true;
        setSettingsLocked(true);
      } else if (data.status === "none") {
        generationState = "idle";
        generationLocked = false;
        setSettingsLocked(false);
        setSeedCompletedAt(null);
      } else {
        generationState = "in_progress";
        generationLocked = true;
        setSettingsLocked(true);
      }
      if (data.room_url) {
        fetchRoomStatus(data.room_url, data.seed_url || "");
      }
      updateGenerateStatus(isAllReady());
    });
    socket.on("terminal_output", (data) => {
      if (data && data.text) appendTerminalText(data.text);
    });
    socket.on("room_stats", (data) => {
      trackerStats = data;
      if (roomInfo && data && data.players_total) {
        roomInfo.players_total = data.players_total;
      }
      updateRoomInfoBox();
    });
    socket.on("room_info", (data) => {
      if (!roomInfo) roomInfo = {};
      if (data && data.room_url) {
        roomInfo.room_url = data.room_url;
      }
      if (data && data.last_port) {
        roomInfo.last_port = data.last_port;
      }
      updateRoomInfoBox();
    });
  }

  let leaveSent = false;
  const emitLeave = () => {
    if (leaveSent) return;
    leaveSent = true;
    if (socket && lobbyId) {
      socket.emit("leave_lobby", { lobby_id: lobbyId });
    }
  };

  window.addEventListener("beforeunload", emitLeave);
  window.addEventListener("pagehide", emitLeave);

  if (profileLink) {
    profileLink.addEventListener("contextmenu", (e) => {
      e.preventDefault();
    });
  }

  // Initial generation check + start polling as fallback
  pollGeneration();
  startPolling();

  const setupCardToggles = () => {
    document.querySelectorAll(".skl-card-toggle").forEach((btn) => {
      btn.addEventListener("click", () => {
        const card = btn.closest(".skl-card");
        if (!card) return;
        const collapsed = card.classList.toggle("collapsed");
        btn.setAttribute("aria-expanded", (!collapsed).toString());
      });
    });
  };

  setupCardToggles();
}
