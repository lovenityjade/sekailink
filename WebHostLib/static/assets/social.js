const socialRoot = document.getElementById("skl-social-root");
if (socialRoot) {
  const friendsDrawer = document.getElementById("skl-friends-drawer");
  const pmDrawer = document.getElementById("skl-pm-drawer");
  const friendsButton = document.getElementById("skl-friends-button");
  const friendsButtonMobile = document.getElementById("skl-friends-button-mobile");
  const friendsTriggers = document.querySelectorAll("[data-friends-open]");
  const friendsList = document.getElementById("skl-friends-list");
  const requestsList = document.getElementById("skl-friend-requests");
  const unreadBadge = document.getElementById("skl-unread-badge") || document.getElementById("skl-friends-badge");
  const unreadBadgeMobile = document.getElementById("skl-unread-badge-mobile") || document.getElementById("skl-friends-badge-mobile");
  const friendsCount = document.getElementById("skl-friends-count");
  const toastStack = document.getElementById("skl-toast-stack");
  const statusSelect = document.getElementById("skl-social-status");
  const dmPolicySelect = document.getElementById("skl-social-dm-policy");
  const pmTitle = document.getElementById("skl-pm-title");
  const pmMessages = document.getElementById("skl-pm-messages");
  const pmForm = document.getElementById("skl-pm-form");
  const pmInput = document.getElementById("skl-pm-input");
  const typingIndicator = document.getElementById("skl-pm-typing");
  const profileName = document.getElementById("skl-profile-name");
  const profileHandle = document.getElementById("skl-profile-handle");
  const profileAvatar = document.getElementById("skl-profile-avatar");
  const accountStatus = document.getElementById("social-status-select");
  const accountDmPolicy = document.getElementById("social-dm-policy-select");
  const accountStatusText = document.getElementById("social-settings-status");

  const socialSocket = window.io ? window.io({ transports: ["websocket"] }) : null;
  let activeConversationId = null;
  let activeConversationName = "";
  const friendAvatarMap = new Map();
  const presenceCache = new Map();
  let presenceReady = false;
  const presenceBootTime = Date.now();

  const playSfx = (name, volume) => window.SKL_SFX && window.SKL_SFX.play ? window.SKL_SFX.play(name, volume) : false;
  const showToast = (message, sound = "global") => {
    if (!toastStack) return;
    const toast = document.createElement("div");
    toast.className = "skl-toast";
    toast.textContent = message;
    toastStack.appendChild(toast);
    if (sound) playSfx(sound, 0.3);
    setTimeout(() => toast.remove(), 3800);
  };

  const setUnreadBadge = (count) => {
    const text = count > 99 ? "99+" : `${count}`;
    [unreadBadge, unreadBadgeMobile].forEach((badge) => {
      if (!badge) return;
      if (count > 0) {
        badge.classList.add("active");
        badge.textContent = text;
      } else {
        badge.classList.remove("active");
        badge.textContent = "";
      }
    });
  };

  const openDrawer = (drawer) => {
    if (!drawer) return;
    drawer.classList.add("open");
    drawer.setAttribute("aria-hidden", "false");
  };

  const closeDrawer = (drawer) => {
    if (!drawer) return;
    drawer.classList.remove("open");
    drawer.setAttribute("aria-hidden", "true");
  };

  const renderFriends = (friends) => {
    if (!friendsList) return;
    friendsList.innerHTML = "";
    if (friendsCount) {
      const online = friends.filter((friend) => friend.status === "online").length;
      friendsCount.textContent = `${online}/${friends.length}`;
    }
    const supportRow = document.createElement("div");
    supportRow.className = "skl-friend-row";
    supportRow.addEventListener("click", () => openConversation("support", "SekaiLink Support"));
    const supportAvatar = document.createElement("div");
    supportAvatar.className = "skl-friend-avatar";
    supportAvatar.style.background = "rgba(92, 231, 181, 0.2)";
    supportAvatar.style.display = "flex";
    supportAvatar.style.alignItems = "center";
    supportAvatar.style.justifyContent = "center";
    supportAvatar.style.color = "#9ef3ce";
    supportAvatar.textContent = "S";
    const supportInfo = document.createElement("div");
    supportInfo.className = "skl-friend-info";
    const supportName = document.createElement("div");
    supportName.className = "skl-friend-name";
    supportName.textContent = "SekaiLink Support";
    const supportStatus = document.createElement("div");
    supportStatus.className = "skl-friend-status";
    supportStatus.textContent = "Support messages";
    supportInfo.appendChild(supportName);
    supportInfo.appendChild(supportStatus);
    supportRow.appendChild(supportAvatar);
    supportRow.appendChild(supportInfo);
    friendsList.appendChild(supportRow);
    if (!friends.length) {
      const empty = document.createElement("div");
      empty.className = "skl-friend-row";
      empty.textContent = "No friends yet.";
      friendsList.appendChild(empty);
      return;
    }
    friends.forEach((friend) => {
      friendAvatarMap.set(friend.user_id, friend.avatar_url || "");
      const row = document.createElement("div");
      row.className = "skl-friend-row";
      row.addEventListener("click", () => openConversation(friend.user_id, friend.display_name));

      const avatar = document.createElement("img");
      avatar.className = "skl-friend-avatar";
      avatar.src = friend.avatar_url || "";
      avatar.alt = friend.display_name;

      const info = document.createElement("div");
      info.className = "skl-friend-info";
      const name = document.createElement("div");
      name.className = "skl-friend-name";
      name.textContent = friend.display_name;
      const status = document.createElement("div");
      status.className = "skl-friend-status";
      const dot = document.createElement("span");
      dot.className = `skl-status-dot ${friend.status}`;
      status.appendChild(dot);
      status.appendChild(document.createTextNode(friend.status === "online" ? "Online" : friend.status === "dnd" ? "Non-available" : "Offline"));
      info.appendChild(name);
      info.appendChild(status);

      const actions = document.createElement("div");
      actions.className = "skl-friend-actions";
      const messageBtn = document.createElement("button");
      messageBtn.className = "skl-btn ghost";
      messageBtn.type = "button";
      messageBtn.textContent = "Message";
      messageBtn.addEventListener("click", (event) => {
        event.stopPropagation();
        openConversation(friend.user_id, friend.display_name);
      });
      const removeBtn = document.createElement("button");
      removeBtn.className = "skl-btn ghost";
      removeBtn.type = "button";
      removeBtn.textContent = "Remove";
      removeBtn.addEventListener("click", async (event) => {
        event.stopPropagation();
        await fetch("/api/social/friends/remove", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ user_id: friend.user_id }),
        });
        loadFriends();
      });
      actions.appendChild(messageBtn);
      actions.appendChild(removeBtn);

      row.appendChild(avatar);
      row.appendChild(info);
      row.appendChild(actions);
      friendsList.appendChild(row);
    });
  };

  const renderRequests = (incoming, outgoing) => {
    if (!requestsList) return;
    requestsList.innerHTML = "";
    if (!incoming.length && !outgoing.length) {
      const empty = document.createElement("div");
      empty.className = "skl-friend-row";
      empty.textContent = "No pending requests.";
      requestsList.appendChild(empty);
      return;
    }
    incoming.forEach((req) => {
      const row = document.createElement("div");
      row.className = "skl-friend-row";
      const name = document.createElement("div");
      name.className = "skl-friend-name";
      name.textContent = `${req.from_name} sent a request`;
      const actions = document.createElement("div");
      actions.className = "skl-friend-actions";
      const accept = document.createElement("button");
      accept.className = "skl-btn ghost";
      accept.textContent = "Accept";
      accept.addEventListener("click", async () => {
        await fetch("/api/social/friends/respond", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ request_id: req.id, action: "accept" }),
        });
        loadRequests();
        loadFriends();
      });
      const decline = document.createElement("button");
      decline.className = "skl-btn ghost";
      decline.textContent = "Decline";
      decline.addEventListener("click", async () => {
        await fetch("/api/social/friends/respond", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ request_id: req.id, action: "decline" }),
        });
        loadRequests();
      });
      actions.appendChild(accept);
      actions.appendChild(decline);
      row.appendChild(name);
      row.appendChild(actions);
      requestsList.appendChild(row);
    });
    outgoing.forEach((req) => {
      const row = document.createElement("div");
      row.className = "skl-friend-row";
      row.textContent = `Request sent to ${req.to_name}`;
      requestsList.appendChild(row);
    });
  };

  const loadFriends = async () => {
    const res = await fetch("/api/social/friends");
    if (!res.ok) return;
    const data = await res.json();
    const friends = data.friends || [];
    friends.forEach((friend) => {
      if (friend && friend.user_id) {
        presenceCache.set(friend.user_id, friend.status || "offline");
      }
    });
    presenceReady = true;
    renderFriends(friends);
  };

  const loadRequests = async () => {
    const res = await fetch("/api/social/requests");
    if (!res.ok) return;
    const data = await res.json();
    renderRequests(data.incoming || [], data.outgoing || []);
  };

  const loadUnreadCount = async () => {
    const res = await fetch("/api/social/unread-count");
    if (!res.ok) return;
    const data = await res.json();
    setUnreadBadge(data.unread || 0);
  };

  const loadSettings = async () => {
    const res = await fetch("/api/social/settings");
    if (!res.ok) return;
    const data = await res.json();
    if (statusSelect) statusSelect.value = data.presence_status || "online";
    if (dmPolicySelect) dmPolicySelect.value = data.dm_policy || "friends";
    if (accountStatus) accountStatus.value = data.presence_status || "online";
    if (accountDmPolicy) accountDmPolicy.value = data.dm_policy || "friends";
  };

  const saveSettings = async (presence, policy) => {
    const res = await fetch("/api/social/settings", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ presence_status: presence, dm_policy: policy }),
    });
    if (accountStatusText) {
      accountStatusText.textContent = res.ok ? "Settings saved." : "Unable to save settings.";
    }
    if (!res.ok) {
      showToast("Unable to update your social settings.", "error");
    }
  };

  const openConversation = async (userId, name) => {
    activeConversationId = userId;
    activeConversationName = name || "Conversation";
    if (pmTitle) pmTitle.textContent = activeConversationName;
    if (profileName) profileName.textContent = activeConversationName;
    if (profileHandle) profileHandle.textContent = userId ? `@${userId}` : "Ready to chat";
    if (profileAvatar) {
      const avatarUrl = friendAvatarMap.get(userId);
      profileAvatar.style.backgroundImage = avatarUrl ? `url('${avatarUrl}')` : "";
      profileAvatar.style.backgroundSize = avatarUrl ? "cover" : "auto";
    }
    if (pmMessages) pmMessages.innerHTML = "";
    closeDrawer(friendsDrawer);
    openDrawer(pmDrawer);
    const res = await fetch(`/api/social/messages?user_id=${encodeURIComponent(userId)}`);
    if (!res.ok) return;
    const data = await res.json();
    (data.messages || []).forEach((msg) => {
      appendMessage(msg.direction, msg.content, msg.created_at);
    });
    await fetch("/api/social/messages/read", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ user_id: userId }),
    });
    loadUnreadCount();
  };

  const appendMessage = (direction, content, createdAt) => {
    if (!pmMessages) return;
    const bubble = document.createElement("div");
    bubble.className = `skl-pm-bubble ${direction}`;
    bubble.textContent = content;
    if (createdAt) {
      const time = document.createElement("div");
      time.className = "skl-pm-bubble-time";
      const date = new Date(createdAt);
      time.textContent = Number.isNaN(date.getTime())
        ? ""
        : date.toLocaleString(undefined, { month: "short", day: "numeric", hour: "2-digit", minute: "2-digit" });
      bubble.appendChild(time);
    }
    pmMessages.appendChild(bubble);
    pmMessages.scrollTop = pmMessages.scrollHeight;
  };

  if (friendsButton) friendsButton.addEventListener("click", (event) => {
    event.preventDefault();
    openDrawer(friendsDrawer);
  });
  if (friendsButtonMobile) friendsButtonMobile.addEventListener("click", (event) => {
    event.preventDefault();
    openDrawer(friendsDrawer);
  });
  if (friendsTriggers.length) {
    friendsTriggers.forEach((trigger) => {
      trigger.addEventListener("click", (event) => {
        event.preventDefault();
        openDrawer(friendsDrawer);
      });
    });
  }

  if (friendsDrawer) {
    friendsDrawer.querySelectorAll("[data-social-close]").forEach((btn) => {
      btn.addEventListener("click", () => closeDrawer(friendsDrawer));
    });
  }
  if (pmDrawer) {
    pmDrawer.querySelectorAll("[data-pm-close]").forEach((btn) => {
      btn.addEventListener("click", () => closeDrawer(pmDrawer));
    });
  }

  if (statusSelect && dmPolicySelect) {
    statusSelect.addEventListener("change", () => saveSettings(statusSelect.value, dmPolicySelect.value));
    dmPolicySelect.addEventListener("change", () => saveSettings(statusSelect.value, dmPolicySelect.value));
  }
  if (accountStatus && accountDmPolicy) {
    accountStatus.addEventListener("change", () => saveSettings(accountStatus.value, accountDmPolicy.value));
    accountDmPolicy.addEventListener("change", () => saveSettings(accountStatus.value, accountDmPolicy.value));
  }

  if (pmForm) {
    pmForm.addEventListener("submit", async (event) => {
      event.preventDefault();
      if (!activeConversationId || !pmInput) return;
      const content = pmInput.value.trim();
      if (!content) return;
      pmInput.value = "";
      appendMessage("out", content, new Date().toISOString());
      const res = await fetch("/api/social/messages", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ user_id: activeConversationId, content }),
      });
      if (!res.ok) {
        showToast("Message failed to send.", "error");
      }
    });
  }

  let typingTimeout = null;
  const showTyping = (label) => {
    if (!typingIndicator) return;
    typingIndicator.textContent = label || "Typing...";
    if (typingTimeout) clearTimeout(typingTimeout);
    typingTimeout = setTimeout(() => {
      typingIndicator.textContent = "";
    }, 1800);
  };

  if (pmInput) {
    pmInput.addEventListener("input", () => {
      if (socialSocket && activeConversationId) {
        socialSocket.emit("dm_typing", { user_id: activeConversationId });
      }
    });
  }

  if (socialSocket) {
    socialSocket.on("friend_presence", (data) => {
      if (!data) return;
      loadFriends();
      const userId = data.user_id;
      const nextStatus = data.status || "offline";
      const lastStatus = userId ? presenceCache.get(userId) : null;
      if (userId) presenceCache.set(userId, nextStatus);
      const bootGrace = Date.now() - presenceBootTime < 3000;
      if (!presenceReady || bootGrace || !lastStatus || lastStatus === nextStatus) {
        return;
      }
      const message = `${data.display_name} is now ${nextStatus === "online" ? "online" : nextStatus === "dnd" ? "non-available" : "offline"}.`;
      showToast(message, "global");
    });
    socialSocket.on("friend_activity", (data) => {
      if (!data) return;
      if (data.type === "lobby_join") {
        showToast(`${data.display_name} joined a lobby.`, "global");
      } else if (data.type === "lobby_leave") {
        showToast(`${data.display_name} left a lobby.`, "global");
      } else if (data.type === "room_join") {
        showToast(`${data.display_name} joined a room.`, "global");
      } else if (data.type === "room_leave") {
        showToast(`${data.display_name} left a room.`, "global");
      }
    });
    socialSocket.on("dm_new", (data) => {
      if (!data) return;
      const fromId = data.from_id;
      if (activeConversationId === fromId) {
        appendMessage("in", data.content, data.created_at);
        fetch("/api/social/messages/read", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ user_id: fromId }),
        });
      } else {
        showToast(`New message from ${data.from_name}.`, "global");
        loadUnreadCount();
      }
    });
    socialSocket.on("dm_typing", (data) => {
      if (!data || !typingIndicator) return;
      if (data.from_id !== activeConversationId) return;
      showTyping(`${data.from_name || "Friend"} is typing…`);
    });
    socialSocket.on("friend_request", () => {
      showToast("You received a friend request.", "friendrequest");
      loadRequests();
    });
    socialSocket.on("friend_added", (data) => {
      if (!data) return;
      showToast(`${data.display_name} accepted your friend request.`, "global");
      loadFriends();
    });
    socialSocket.on("support_ticket_update", (data) => {
      if (!data) return;
      showToast(`Support ticket update: ${data.subject} (${data.status}).`, "global");
    });
  }

  loadSettings();
  loadFriends();
  loadRequests();
  loadUnreadCount();

  window.SKL_SOCIAL = {
    openConversation: (userId, name) => openConversation(userId, name),
    sendFriendRequest: async (userId) => {
      const res = await fetch("/api/social/friends/request", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ user_id: userId }),
      });
      if (!res.ok) {
        const data = await res.json().catch(() => ({}));
        showToast(data.error || "Unable to send friend request.", "error");
        return false;
      }
      showToast("Friend request sent.", "confirm");
      loadRequests();
      return true;
    },
  };
}
