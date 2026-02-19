const tabButtons = document.querySelectorAll("[data-admin-tab]");
const panels = document.querySelectorAll("[data-admin-panel]");
const quickOpenButtons = document.querySelectorAll("[data-admin-open]");

const setActivePanel = (name) => {
  tabButtons.forEach((btn) => btn.classList.toggle("active", btn.dataset.adminTab === name));
  panels.forEach((panel) => panel.classList.toggle("active", panel.dataset.adminPanel === name));
};

if (tabButtons.length) {
tabButtons.forEach((btn) => {
  btn.addEventListener("click", () => setActivePanel(btn.dataset.adminTab));
});

quickOpenButtons.forEach((btn) => {
  btn.addEventListener("click", () => setActivePanel(btn.dataset.adminOpen));
});
}

const usersList = document.getElementById("admin-users-list");
const userSearch = document.getElementById("admin-user-search");
const userRefresh = document.getElementById("admin-user-refresh");

const renderUsers = (users) => {
  if (!usersList) return;
  usersList.innerHTML = "";
  users.forEach((user) => {
    const row = document.createElement("div");
    row.className = "skl-admin-row";

    const name = document.createElement("span");
    name.textContent = user.display_name || user.discord_id;

    const discordId = document.createElement("span");
    discordId.textContent = user.discord_id;

    const roleCell = document.createElement("span");
    const roleSelect = document.createElement("select");
    ["admin", "moderator", "player"].forEach((role) => {
      const opt = document.createElement("option");
      opt.value = role;
      opt.textContent = role;
      if (role === user.role) opt.selected = true;
      roleSelect.appendChild(opt);
    });
    roleCell.appendChild(roleSelect);

    const status = document.createElement("span");
    const statusPill = document.createElement("span");
    statusPill.className = `skl-status-pill ${user.banned ? "danger" : ""}`;
    statusPill.textContent = user.banned ? "Banned" : "Active";
    status.appendChild(statusPill);

    const reasonCell = document.createElement("span");
    const reasonInput = document.createElement("input");
    reasonInput.type = "text";
    reasonInput.maxLength = 255;
    reasonInput.placeholder = "Ban reason";
    reasonInput.value = user.ban_reason || "";
    reasonCell.appendChild(reasonInput);

    const actionCell = document.createElement("span");
    const saveBtn = document.createElement("button");
    saveBtn.className = "skl-btn ghost";
    saveBtn.textContent = "Save";
    const banBtn = document.createElement("button");
    banBtn.className = "skl-btn ghost";
    banBtn.textContent = user.banned ? "Unban" : "Ban";

    saveBtn.addEventListener("click", async () => {
      await fetch(`/api/admin/users/${encodeURIComponent(user.discord_id)}/role`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ role: roleSelect.value }),
      });
    });

    banBtn.addEventListener("click", async () => {
      const banned = !user.banned;
      await fetch(`/api/admin/users/${encodeURIComponent(user.discord_id)}/ban`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ banned, reason: reasonInput.value }),
      });
      loadUsers();
    });

    actionCell.appendChild(saveBtn);
    actionCell.appendChild(banBtn);

    row.appendChild(name);
    row.appendChild(discordId);
    row.appendChild(roleCell);
    row.appendChild(status);
    row.appendChild(reasonCell);
    row.appendChild(actionCell);
    usersList.appendChild(row);
  });
};

const loadUsers = async () => {
  if (!usersList) return;
  const query = userSearch ? userSearch.value.trim() : "";
  const res = await fetch(`/api/admin/users${query ? `?search=${encodeURIComponent(query)}` : ""}`);
  if (!res.ok) return;
  const data = await res.json();
  renderUsers(data.users || []);
};

if (userRefresh) userRefresh.addEventListener("click", loadUsers);
if (userSearch) userSearch.addEventListener("input", () => {
  if (userSearch.value.length === 0 || userSearch.value.length > 2) loadUsers();
});

const lobbiesList = document.getElementById("admin-lobbies-list");
const lobbyDetail = document.getElementById("admin-lobby-detail");
const lobbiesRefresh = document.getElementById("admin-lobbies-refresh");

const renderLobbies = (lobbies) => {
  if (!lobbiesList) return;
  lobbiesList.innerHTML = "";
  lobbies.forEach((lobby) => {
    const row = document.createElement("div");
    row.className = "skl-admin-row";

    const name = document.createElement("span");
    name.textContent = lobby.name;

    const owner = document.createElement("span");
    owner.textContent = lobby.owner;

    const members = document.createElement("span");
    members.textContent = lobby.member_count;

    const lastActive = document.createElement("span");
    lastActive.textContent = new Date(lobby.last_activity).toLocaleString();

    const room = document.createElement("span");
    room.textContent = lobby.room_id || "-";

    const actions = document.createElement("span");
    const inspectBtn = document.createElement("button");
    inspectBtn.className = "skl-btn ghost";
    inspectBtn.textContent = "Inspect";
    inspectBtn.addEventListener("click", () => loadLobbyDetail(lobby.id));
    const closeBtn = document.createElement("button");
    closeBtn.className = "skl-btn ghost";
    closeBtn.textContent = "Close";
    closeBtn.addEventListener("click", async () => {
      await fetch(`/api/admin/lobbies/${lobby.id}/close`, { method: "POST" });
      loadLobbies();
    });
    actions.appendChild(inspectBtn);
    actions.appendChild(closeBtn);

    row.appendChild(name);
    row.appendChild(owner);
    row.appendChild(members);
    row.appendChild(lastActive);
    row.appendChild(room);
    row.appendChild(actions);
    lobbiesList.appendChild(row);
  });
};

const loadLobbyDetail = async (lobbyId) => {
  if (!lobbyDetail) return;
  const res = await fetch(`/api/admin/lobbies/${lobbyId}`);
  if (!res.ok) return;
  const data = await res.json();
  lobbyDetail.innerHTML = `
    <h3>${data.name}</h3>
    <p>${data.description || "No description"}</p>
    <p><strong>Owner:</strong> ${data.owner}</p>
    <p><strong>Room:</strong> ${data.room_id || "-"}</p>
    <p><strong>Settings:</strong> ${data.settings_summary}</p>
    <p><strong>Members:</strong> ${data.members.map((m) => m.name).join(", ") || "-"}</p>
    <p><strong>Recent messages:</strong> ${data.recent_messages.map((m) => `${m.author}: ${m.content}`).join(" | ") || "-"}</p>
  `;
};

const loadLobbies = async () => {
  if (!lobbiesList) return;
  const res = await fetch("/api/admin/lobbies");
  if (!res.ok) return;
  const data = await res.json();
  renderLobbies(data.lobbies || []);
};

if (lobbiesRefresh) lobbiesRefresh.addEventListener("click", loadLobbies);

const roomsList = document.getElementById("admin-rooms-list");
const roomLog = document.getElementById("admin-room-log");
const roomsRefresh = document.getElementById("admin-rooms-refresh");
const roomsPurge = document.getElementById("admin-rooms-purge");

const renderRooms = (rooms) => {
  if (!roomsList) return;
  roomsList.innerHTML = "";
  rooms.forEach((room) => {
    const row = document.createElement("div");
    row.className = "skl-admin-row";

    const id = document.createElement("span");
    id.textContent = room.id;

    const owner = document.createElement("span");
    owner.textContent = room.owner;

    const port = document.createElement("span");
    port.textContent = room.last_port || "-";

    const lastActive = document.createElement("span");
    lastActive.textContent = new Date(room.last_activity).toLocaleString();

    const seed = document.createElement("span");
    seed.textContent = room.seed_id;

    const status = document.createElement("span");
    const statusPill = document.createElement("span");
    statusPill.className = "skl-status-pill";
    const statusValue = room.status || "Unknown";
    if (statusValue === "Error") statusPill.classList.add("danger");
    if (statusValue === "Active") statusPill.classList.add("success");
    statusPill.textContent = statusValue;
    status.appendChild(statusPill);

    const actions = document.createElement("span");
    const logBtn = document.createElement("button");
    logBtn.className = "skl-btn ghost";
    logBtn.textContent = "View log";
    logBtn.addEventListener("click", () => loadRoomLog(room.log_file));
    actions.appendChild(logBtn);
    const closeBtn = document.createElement("button");
    closeBtn.className = "skl-btn ghost";
    closeBtn.textContent = "Close";
    closeBtn.addEventListener("click", async () => {
      await fetch(`/api/admin/rooms/${room.id}/close`, { method: "POST" });
      loadRooms();
    });
    actions.appendChild(closeBtn);

    row.appendChild(id);
    row.appendChild(owner);
    row.appendChild(port);
    row.appendChild(lastActive);
    row.appendChild(seed);
    row.appendChild(status);
    row.appendChild(actions);
    roomsList.appendChild(row);
  });
};

const loadRoomLog = async (filename) => {
  if (!roomLog) return;
  if (!filename) {
    roomLog.textContent = "No log file found.";
    return;
  }
  const res = await fetch(`/api/admin/logs/${encodeURIComponent(filename)}?lines=200`);
  if (!res.ok) {
    roomLog.textContent = "Unable to load log.";
    return;
  }
  const data = await res.json();
  roomLog.textContent = data.content || "";
};

const loadRooms = async () => {
  if (!roomsList) return;
  const res = await fetch("/api/admin/rooms");
  if (!res.ok) return;
  const data = await res.json();
  renderRooms(data.rooms || []);
};

if (roomsRefresh) roomsRefresh.addEventListener("click", loadRooms);
if (roomsPurge) roomsPurge.addEventListener("click", async () => {
  const ok = window.confirm("Close all active room servers? This will send /exit to every running room.");
  if (!ok) return;
  await fetch("/api/admin/rooms/purge", { method: "POST" });
  loadRooms();
});

const logsList = document.getElementById("admin-logs-list");
const logViewer = document.getElementById("admin-log-viewer");
const logsRefresh = document.getElementById("admin-logs-refresh");

const renderLogs = (logs) => {
  if (!logsList) return;
  logsList.innerHTML = "";
  logs.forEach((log) => {
    const row = document.createElement("div");
    row.className = "skl-admin-row";

    const name = document.createElement("span");
    name.textContent = log.name;

    const size = document.createElement("span");
    size.textContent = log.size;

    const updated = document.createElement("span");
    updated.textContent = new Date(log.updated_at).toLocaleString();

    const actions = document.createElement("span");
    const viewBtn = document.createElement("button");
    viewBtn.className = "skl-btn ghost";
    viewBtn.textContent = "View";
    viewBtn.addEventListener("click", () => loadGenericLog(log.name));
    actions.appendChild(viewBtn);

    row.appendChild(name);
    row.appendChild(size);
    row.appendChild(updated);
    row.appendChild(actions);
    logsList.appendChild(row);
  });
};

const loadGenericLog = async (filename) => {
  if (!logViewer) return;
  const res = await fetch(`/api/admin/logs/${encodeURIComponent(filename)}?lines=200`);
  if (!res.ok) {
    logViewer.textContent = "Unable to load log.";
    return;
  }
  const data = await res.json();
  logViewer.textContent = data.content || "";
};

const loadLogs = async () => {
  if (!logsList) return;
  const res = await fetch("/api/admin/logs");
  if (!res.ok) return;
  const data = await res.json();
  renderLogs(data.logs || []);
};

if (logsRefresh) logsRefresh.addEventListener("click", loadLogs);

const appealsList = document.getElementById("admin-appeals-list");
const appealsRefresh = document.getElementById("admin-appeals-refresh");

const renderAppeals = (appeals) => {
  if (!appealsList) return;
  appealsList.innerHTML = "";
  appeals.forEach((appeal) => {
    const row = document.createElement("div");
    row.className = "skl-admin-row";

    const user = document.createElement("span");
    user.textContent = appeal.user;

    const created = document.createElement("span");
    created.textContent = new Date(appeal.created_at).toLocaleString();

    const status = document.createElement("span");
    const pill = document.createElement("span");
    pill.className = `skl-status-pill ${appeal.status !== "open" ? "" : ""}`;
    pill.textContent = appeal.status;
    status.appendChild(pill);

    const message = document.createElement("span");
    message.textContent = `${appeal.subject ? `[${appeal.subject}] ` : ""}${appeal.message}`;

    const actions = document.createElement("span");
    const replyBtn = document.createElement("button");
    replyBtn.className = "skl-btn ghost";
    replyBtn.textContent = "Reply";
    replyBtn.addEventListener("click", async () => {
      const response = window.prompt("Reply to this ticket (will be sent as a DM):");
      if (!response) return;
      await fetch(`/api/admin/support/${appeal.id}/status`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ status: "open", response }),
      });
      loadAppeals();
    });
    const closeBtn = document.createElement("button");
    closeBtn.className = "skl-btn ghost";
    closeBtn.textContent = "Close";
    closeBtn.disabled = appeal.status !== "open";
    closeBtn.addEventListener("click", async () => {
      const response = window.prompt("Optional reply to the user (leave empty for none):") || "";
      await fetch(`/api/admin/support/${appeal.id}/status`, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ status: "closed", response }),
      });
      loadAppeals();
    });
    actions.appendChild(replyBtn);
    actions.appendChild(closeBtn);

    row.appendChild(user);
    row.appendChild(created);
    row.appendChild(status);
    row.appendChild(message);
    row.appendChild(actions);
    appealsList.appendChild(row);
  });
};

const loadAppeals = async () => {
  if (!appealsList) return;
  const res = await fetch("/api/admin/support?category=ban_appeal");
  if (!res.ok) return;
  const data = await res.json();
  renderAppeals(data.tickets || []);
};

if (appealsRefresh) appealsRefresh.addEventListener("click", loadAppeals);

loadUsers();
loadLobbies();
loadRooms();
loadLogs();
loadAppeals();
