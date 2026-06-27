const $ = (id) => document.getElementById(id);

const messages = $("messages");
const askForm = $("askForm");
const messageInput = $("messageInput");
const contextInput = $("contextInput");
const modeSelect = $("modeSelect");
const scopeSelect = $("scopeSelect");
const gameKey = $("gameKey");
const sendButton = $("sendButton");
const charHint = $("charHint");
const pulseDot = $("pulseDot");
const pulseStatus = $("pulseStatus");
const pulseMeta = $("pulseMeta");

const session = [];
let lastAnswer = "";

const nowLabel = () => new Date().toLocaleTimeString([], { hour: "2-digit", minute: "2-digit" });

function addMessage(role, text, meta = {}) {
  const article = document.createElement("article");
  article.className = `message ${role}`;
  const avatar = document.createElement("div");
  avatar.className = "avatar";
  avatar.textContent = role === "user" ? "J" : "P";
  const bubble = document.createElement("div");
  bubble.className = "bubble";
  const head = document.createElement("div");
  head.className = "message-head";
  head.innerHTML = `<strong>${role === "user" ? "Jade" : "Pulse"}</strong><span>${nowLabel()}</span>`;
  const body = document.createElement("pre");
  body.textContent = text;
  bubble.append(head, body);
  if (Object.keys(meta).length) {
    const metaWrap = document.createElement("div");
    metaWrap.className = "result-meta";
    for (const [key, value] of Object.entries(meta)) {
      if (value === undefined || value === null || value === "") continue;
      const pill = document.createElement("span");
      pill.className = "pill";
      pill.textContent = `${key}: ${value}`;
      metaWrap.appendChild(pill);
    }
    bubble.appendChild(metaWrap);
  }
  article.append(avatar, bubble);
  messages.appendChild(article);
  messages.scrollTop = messages.scrollHeight;
  session.push({ role, text, meta, time: new Date().toISOString() });
}

function responseText(payload) {
  if (payload.answer) return String(payload.answer);
  if (payload.error) return `Erreur: ${payload.error}${payload.message ? `\n${payload.message}` : ""}`;
  return JSON.stringify(payload, null, 2);
}

async function refreshStatus() {
  try {
    const res = await fetch("/api/status", { cache: "no-store" });
    const data = await res.json();
    const ok = Boolean(data.ok && data.rag && data.rag.ok);
    pulseDot.className = `dot ${ok ? "ok" : "bad"}`;
    pulseStatus.textContent = ok ? "Pulse online" : "Pulse degraded";
    const rows = data.rag?.index_rows ?? "?";
    const examples = data.rag?.example_responses ?? "?";
    const web = data.web_search ? "web assist on" : "web assist off";
    pulseMeta.textContent = `${data.model || "model"} • ${rows} AP rows • ${examples} examples • ${web}`;
  } catch (error) {
    pulseDot.className = "dot bad";
    pulseStatus.textContent = "Pulse unreachable";
    pulseMeta.textContent = String(error);
  }
}

async function askPulse(event) {
  event.preventDefault();
  const message = messageInput.value.trim();
  if (!message) return;
  addMessage("user", message, { mode: modeSelect.value, scope: scopeSelect.value });
  messageInput.value = "";
  updateHint();
  sendButton.disabled = true;
  sendButton.textContent = "Thinking...";
  try {
    const res = await fetch("/api/ask", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        message,
        context: contextInput.value,
        mode: modeSelect.value,
        scope: scopeSelect.value,
        game_key: gameKey.value || "alttp",
      }),
    });
    const data = await res.json();
    const text = responseText(data);
    lastAnswer = text;
    addMessage("pulse", text, {
      source: data.source,
      confidence: data.confidence,
      web: data.web_sources?.length ? `${data.web_sources.length} source(s)` : "",
      cache: data.cache_hit ? "hit" : "",
    });
    if (data.draft_values && Object.keys(data.draft_values).length) {
      addMessage("pulse", JSON.stringify(data.draft_values, null, 2), { type: "draft_values" });
    }
  } catch (error) {
    addMessage("pulse", `Erreur réseau: ${error}`, { source: "browser" });
  } finally {
    sendButton.disabled = false;
    sendButton.textContent = "Send to Pulse";
  }
}

function updateHint() {
  charHint.textContent = `${messageInput.value.length} / 6000`;
}

document.querySelectorAll("[data-prompt]").forEach((button) => {
  button.addEventListener("click", () => {
    messageInput.value = button.getAttribute("data-prompt") || "";
    updateHint();
    messageInput.focus();
  });
});

$("copyLast").addEventListener("click", async () => {
  if (!lastAnswer) return;
  await navigator.clipboard.writeText(lastAnswer);
});

$("exportLog").addEventListener("click", () => {
  const blob = new Blob([JSON.stringify(session, null, 2)], { type: "application/json" });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = `pulse-console-${new Date().toISOString().replace(/[:.]/g, "-")}.json`;
  link.click();
  URL.revokeObjectURL(url);
});

$("clearChat").addEventListener("click", () => {
  messages.innerHTML = "";
  session.length = 0;
  lastAnswer = "";
  addMessage("pulse", "Session locale effacée. Je suis prête.", { status: "ready" });
});

scopeSelect.addEventListener("change", () => {
  const ap = scopeSelect.value === "apworld";
  gameKey.disabled = !ap;
  if (ap && modeSelect.value === "challenge") modeSelect.value = "explain";
});

messageInput.addEventListener("input", updateHint);
askForm.addEventListener("submit", askPulse);
refreshStatus();
setInterval(refreshStatus, 30000);
updateHint();
