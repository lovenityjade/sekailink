(() => {
  const sfxFiles = {
    // Core UI aliases used in code
    appopen: "./assets/sfx/sfx_boot_app_start.wav",
    confirm: "./assets/sfx/sfx_ui_confirm.wav",
    error: "./assets/sfx/sfx_ui_error.wav",
    friendrequest: "./assets/sfx/sfx_friend_request_notification.wav",
    global: "./assets/sfx/sfx_global_notification.wav",
    join: "./assets/sfx/sfx_lobby_join.wav",
    leave: "./assets/sfx/sfx_lobby_leave.wav",
    ready: "./assets/sfx/sfx_ui_selection.wav",
    unready: "./assets/sfx/sfx_ui_hover.wav",
    success: "./assets/sfx/sfx_generation_completed.wav",

    // Extended keys (exposed in Audio tab)
    hover: "./assets/sfx/sfx_ui_hover.wav",
    select: "./assets/sfx/sfx_ui_selection.wav",
    info: "./assets/sfx/sfx_information.wav",
    chat: "./assets/sfx/sfx_chat_notification.wav",
    update: "./assets/sfx/sfx_update_notification.wav",
    friendonline: "./assets/sfx/sfx_friend_online.wav",
    friendaccepted: "./assets/sfx/sfx_friend_request_accepted.wav",
    generationcomplete: "./assets/sfx/sfx_generation_completed.wav",
    generationerror: "./assets/sfx/sfx_generation_error.wav",
  };

  const bgmTracks = {
    menu_teal_01: { label: "Menu Teal 01", src: "./assets/sfx/bgm_menu_teal_01.wav" },
    menu_teal_02: { label: "Menu Teal 02", src: "./assets/sfx/bgm_menu_teal_02.wav" },
    menu_teal_03: { label: "Menu Teal 03", src: "./assets/sfx/bgm_menu_teal_03.wav" },
  };

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
  let bgmAudio = null;
  let lastBgmStartAt = 0;

  const STORAGE = {
    globalMuted: "skl_audio_global_muted",
    globalVolume: "skl_audio_global_volume",
    sfxMutedPrefix: "skl_audio_sfx_muted_",
    sfxVolumePrefix: "skl_audio_sfx_volume_",
    bgmEnabled: "skl_audio_bgm_enabled",
    bgmTrack: "skl_audio_bgm_track",
    bgmVolume: "skl_audio_bgm_volume",
  };

  const clamp = (v, lo, hi) => Math.min(Math.max(v, lo), hi);
  const boolFromStorage = (key, defaultValue = false) => {
    const raw = localStorage.getItem(key);
    if (raw === null) return defaultValue;
    return raw === "1";
  };
  const numFromStorage = (key, defaultValue) => {
    const raw = localStorage.getItem(key);
    if (raw === null) return defaultValue;
    const n = Number(raw);
    return Number.isFinite(n) ? n : defaultValue;
  };

  const getGlobalMuted = () => boolFromStorage(STORAGE.globalMuted, false);
  const setGlobalMuted = (value) => localStorage.setItem(STORAGE.globalMuted, value ? "1" : "0");
  const toggleGlobalMuted = () => {
    const next = !getGlobalMuted();
    setGlobalMuted(next);
    if (next) stopBgm();
    else startBgm();
    return next;
  };
  const getGlobalVolume = () => clamp(numFromStorage(STORAGE.globalVolume, 0.3), 0, 1);
  const setGlobalVolume = (value) => {
    localStorage.setItem(STORAGE.globalVolume, String(clamp(value, 0, 1)));
    applyBgmVolume();
  };

  const getSoundMuted = (name) => boolFromStorage(`${STORAGE.sfxMutedPrefix}${name}`, false);
  const setSoundMuted = (name, value) => localStorage.setItem(`${STORAGE.sfxMutedPrefix}${name}`, value ? "1" : "0");
  const getSoundVolume = (name) => clamp(numFromStorage(`${STORAGE.sfxVolumePrefix}${name}`, 1), 0, 1);
  const setSoundVolume = (name, value) => localStorage.setItem(`${STORAGE.sfxVolumePrefix}${name}`, String(clamp(value, 0, 1)));

  const getBgmEnabled = () => boolFromStorage(STORAGE.bgmEnabled, true);
  const setBgmEnabled = (value) => {
    localStorage.setItem(STORAGE.bgmEnabled, value ? "1" : "0");
    if (!value) stopBgm();
    else startBgm();
  };
  const getBgmTrack = () => {
    const raw = String(localStorage.getItem(STORAGE.bgmTrack) || "menu_teal_01");
    if (bgmTracks[raw]) return raw;
    return "menu_teal_01";
  };
  const setBgmTrack = (trackId) => {
    if (!bgmTracks[trackId]) return;
    localStorage.setItem(STORAGE.bgmTrack, trackId);
    stopBgm();
    startBgm();
  };
  const getBgmVolume = () => clamp(numFromStorage(STORAGE.bgmVolume, 0.25), 0, 1);
  const setBgmVolume = (value) => {
    localStorage.setItem(STORAGE.bgmVolume, String(clamp(value, 0, 1)));
    applyBgmVolume();
  };

  const prefersReduced = () =>
    window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches;

  const loadAudio = (name) => {
    if (cache.has(name)) return cache.get(name);
    const src = sfxFiles[name];
    if (!src) return null;
    const audio = new Audio(src);
    audio.preload = "auto";
    cache.set(name, audio);
    return audio;
  };

  const play = (name, volume) => {
    if (!name || getGlobalMuted()) return false;
    if (getSoundMuted(name)) return false;
    if (prefersReduced() && !["confirm", "error"].includes(name)) return false;

    const now = performance.now();
    const cd = cooldowns[name] || 180;
    const last = lastPlayed.get(name) || 0;
    if (now - last < cd) return false;
    lastPlayed.set(name, now);

    const audio = loadAudio(name);
    if (!audio) return false;

    const globalVol = getGlobalVolume();
    const soundVol = getSoundVolume(name);
    const vol = typeof volume === "number" ? clamp(volume, 0, 1) : 1;
    audio.volume = clamp(globalVol * soundVol * vol, 0, 1);

    try {
      audio.currentTime = 0;
      const result = audio.play();
      if (result && typeof result.catch === "function") result.catch(() => {});
      return true;
    } catch {
      return false;
    }
  };

  const preload = () => {
    Object.keys(sfxFiles).forEach((name) => loadAudio(name));
  };

  const applyBgmVolume = () => {
    if (!bgmAudio) return;
    bgmAudio.volume = clamp(getGlobalVolume() * getBgmVolume(), 0, 1);
  };

  const stopBgm = () => {
    if (!bgmAudio) return;
    try {
      bgmAudio.pause();
      bgmAudio.currentTime = 0;
    } catch {}
    bgmAudio = null;
  };

  const startBgm = () => {
    if (getGlobalMuted() || !getBgmEnabled()) return false;

    const now = Date.now();
    if (now - lastBgmStartAt < 300) return false;
    lastBgmStartAt = now;

    const trackId = getBgmTrack();
    const track = bgmTracks[trackId];
    if (!track) return false;

    if (bgmAudio && bgmAudio.dataset && bgmAudio.dataset.trackId === trackId) {
      applyBgmVolume();
      const resumed = bgmAudio.play();
      if (resumed && typeof resumed.catch === "function") resumed.catch(() => {});
      return true;
    }

    stopBgm();
    const audio = new Audio(track.src);
    audio.preload = "auto";
    audio.loop = true;
    audio.dataset.trackId = trackId;
    bgmAudio = audio;
    applyBgmVolume();
    try {
      const result = audio.play();
      if (result && typeof result.catch === "function") result.catch(() => {});
      return true;
    } catch {
      return false;
    }
  };

  const unlock = () => {
    if (!unlocked) {
      unlocked = true;
      preload();
    }
    startBgm();
  };

  window.SKL_SFX = {
    play,
    preload,
    unlock,

    // backward-compatible
    setMuted: setGlobalMuted,
    getMuted: getGlobalMuted,
    toggleMuted: toggleGlobalMuted,
    getBaseVolume: getGlobalVolume,
    setBaseVolume: setGlobalVolume,

    // audio controls
    getGlobalMuted,
    setGlobalMuted,
    toggleGlobalMuted,
    getGlobalVolume,
    setGlobalVolume,
    getSoundMuted,
    setSoundMuted,
    getSoundVolume,
    setSoundVolume,
    getSoundKeys: () => Object.keys(sfxFiles),
    getBgmEnabled,
    setBgmEnabled,
    getBgmTrack,
    setBgmTrack,
    getBgmVolume,
    setBgmVolume,
    getBgmTracks: () => Object.entries(bgmTracks).map(([id, meta]) => ({ id, label: meta.label })),
    startBgm,
    stopBgm,
  };

  window.addEventListener("pointerdown", unlock, { once: true, passive: true });
  window.addEventListener("click", unlock, { once: true, passive: true });
  window.addEventListener("touchstart", unlock, { once: true, passive: true });
  window.addEventListener("keydown", unlock, { once: true });
  document.addEventListener("DOMContentLoaded", () => {
    preload();
    setTimeout(() => startBgm(), 1800);
  }, { once: true });
})();
