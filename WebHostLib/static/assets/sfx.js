(() => {
  const files = {
    appopen: "/assets/sfx/ui_appopen.mp3",
    confirm: "/assets/sfx/ui_confirm.mp3",
    error: "/assets/sfx/ui_error.mp3",
    friendrequest: "/assets/sfx/ui_friendrequest_notification.mp3",
    global: "/assets/sfx/ui_global_notification.mp3",
    join: "/assets/sfx/ui_join.mp3",
    leave: "/assets/sfx/ui_leave.mp3",
    ready: "/assets/sfx/ui_ready.mp3",
    unready: "/assets/sfx/ui_unready.mp3",
    success: "/assets/sfx/ui_success.mp3",
  };

  /* per-sound cooldown in ms */
  const cooldowns = {
    appopen: 250,
    confirm: 180,
    error: 200,
    friendrequest: 250,
    global: 200,
    join: 200,
    leave: 200,
    ready: 150,
    unready: 150,
    success: 200,
  };

  const cache = new Map();
  const lastPlayed = new Map();
  let unlocked = false;

  const clamp = (v, lo, hi) => Math.min(Math.max(v, lo), hi);
  const getMuted = () => localStorage.getItem("skl_sfx_muted") === "1";
  const setMuted = (v) => localStorage.setItem("skl_sfx_muted", v ? "1" : "0");
  const toggleMuted = () => { const m = !getMuted(); setMuted(m); return m; };
  const getBaseVolume = () => {
    const raw = localStorage.getItem("skl_sfx_volume");
    const v = raw ? Number(raw) : 0.3;
    return Number.isFinite(v) ? clamp(v, 0, 1) : 0.3;
  };
  const setBaseVolume = (v) => localStorage.setItem("skl_sfx_volume", String(clamp(v, 0, 1)));

  /* prefers-reduced-motion — suppress non-essential audio */
  const prefersReduced = () =>
    window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches;

  const loadAudio = (name) => {
    if (cache.has(name)) return cache.get(name);
    const src = files[name];
    if (!src) return null;
    const audio = new Audio(src);
    audio.preload = "auto";
    cache.set(name, audio);
    return audio;
  };

  const play = (name, volume) => {
    if (!name || getMuted()) return false;

    /* reduced-motion: only allow explicit UI confirmations */
    if (prefersReduced() && !["confirm", "error"].includes(name)) return false;

    /* anti-spam cooldown */
    const now = performance.now();
    const cd = cooldowns[name] || 180;
    const last = lastPlayed.get(name) || 0;
    if (now - last < cd) return false;
    lastPlayed.set(name, now);

    const audio = loadAudio(name);
    if (!audio) return false;

    const base = getBaseVolume();
    const vol = typeof volume === "number" ? clamp(volume, 0, 1) : base;
    audio.volume = vol;

    try {
      audio.currentTime = 0;
      const result = audio.play();
      if (result && typeof result.catch === "function") {
        result.catch(() => {});
      }
      return true;
    } catch {
      return false;
    }
  };

  const preload = () => {
    Object.keys(files).forEach((name) => loadAudio(name));
  };

  const unlock = () => {
    if (unlocked) return;
    unlocked = true;
    preload();
    const sample = loadAudio("confirm");
    if (!sample) return;
    sample.muted = true;
    try {
      const result = sample.play();
      if (result && typeof result.then === "function") {
        result.then(() => {
          sample.pause();
          sample.currentTime = 0;
          sample.muted = false;
        }).catch(() => {});
      }
    } catch {
      sample.muted = false;
    }
  };

  window.SKL_SFX = {
    play,
    preload,
    unlock,
    setMuted,
    getMuted,
    toggleMuted,
    getBaseVolume,
    setBaseVolume,
  };

  window.addEventListener("pointerdown", unlock, { once: true, passive: true });
  window.addEventListener("click", unlock, { once: true, passive: true });
  window.addEventListener("touchstart", unlock, { once: true, passive: true });
  window.addEventListener("keydown", unlock, { once: true });
  document.addEventListener("DOMContentLoaded", preload, { once: true });
})();
