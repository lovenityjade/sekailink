/**
 * SekaiLink landing v2 — étoiles, reveal, nav, paliers, carrousel boxart, chips
 */

/** Jaquettes : fichiers dans assets/boxart/ (copiés depuis sekailink/sekailink-website) */
const BOXTART_GAMES = [
  { name: "A Link Between Worlds", file: "a_link_between_worlds.png" },
  { name: "A Link to the Past", file: "a_link_to_the_past.png" },
  { name: "Donkey Kong Country", file: "donkey_kong_country.png" },
  { name: "Donkey Kong Country 2", file: "donkey_kong_country_2.png" },
  { name: "Donkey Kong Country 3", file: "donkey_kong_country_3.png" },
  { name: "EarthBound", file: "earthbound.png" },
  { name: "Final Fantasy IV Free Enterprise", file: "final_fantasy_iv_free_enterprise.png" },
  { name: "Final Fantasy Tactics Advance", file: "final_fantasy_tactics_advance.png" },
  { name: "Kirby's Dream Land 3", file: "kirbys_dream_land_3.png" },
  { name: "Lufia II: Ancient Cave", file: "lufia_ii_ancient_cave.png" },
  { name: "Mega Man 2", file: "mega_man_2.png" },
  { name: "Mega Man 3", file: "mega_man_3.png" },
  { name: "Metroid Fusion", file: "metroid_fusion.png" },
  { name: "Metroid Zero Mission", file: "metroid_zero_mission.png" },
  { name: "Ocarina of Time", file: "ocarina_of_time.webp" },
  { name: "Pokémon Crystal", file: "pokemon_crystal.png" },
  { name: "Pokémon Emerald", file: "pokemon_emerald.png" },
  { name: "Pokémon FireRed / LeafGreen", file: "pokemon_firered_and_leafgreen.png" },
  { name: "Pokémon Red / Blue", file: "pokemon_red_and_blue.png" },
  { name: "Ship of Harkinian", file: "ship_of_harkinian.png" },
  { name: "SMZ3", file: "smz3.png" },
  { name: "Super Mario 64", file: "super_mario_64.png" },
  { name: "Super Mario Land 2", file: "super_mario_land_2.png" },
  { name: "Super Mario World", file: "super_mario_world.png" },
  { name: "Super Metroid", file: "super_metroid.jpg" },
  { name: "The Legend of Zelda", file: "the_legend_of_zelda.png" },
  { name: "The Legend of Zelda: Oracle of Seasons", file: "the_legend_of_zelda_oracle_of_seasons.png" },
  { name: "The Minish Cap", file: "the_minish_cap.png" },
  { name: "Wario Land", file: "wario_land.png" },
  { name: "Wario Land 4", file: "wario_land_4.png" },
  { name: "Yoshi's Island", file: "yoshis_island.png" },
  { name: "Zelda II: The Adventure of Link", file: "zelda_ii_the_adventure_of_link.jpg" },
];

/** Si locales.js ne charge pas, chips en français */
const FEATURES_FALLBACK = [
  "Client moderne",
  "Gestion des jeux",
  "Rooms & sessions",
  "Social / amis",
  "Chat",
  "Multi-layout",
  "Tracker intégré",
  "Services connectés",
  "Companion mobile (à venir)",
  "Mode solo hors ligne",
];

function sortLocaleTag(lang) {
  if (lang === "zh-Hans") return "zh-Hans-CN";
  if (lang === "zh-Hant") return "zh-Hant-TW";
  return lang;
}

function initStars() {
  const canvas = document.getElementById("stars-canvas");
  if (!canvas || !canvas.getContext) return;

  const reduced =
    window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  if (reduced) return;

  const ctx = canvas.getContext("2d");
  let raf = 0;
  let stars = [];
  let w = 0;
  let h = 0;

  function resize() {
    w = canvas.width = window.innerWidth;
    h = canvas.height = window.innerHeight;
    const n = Math.min(120, Math.floor((w * h) / 12000));
    stars = Array.from({ length: n }, () => ({
      x: Math.random() * w,
      y: Math.random() * h,
      r: Math.random() * 1.15 + 0.15,
      tw: Math.random() * Math.PI * 2,
      sp: 0.018 + Math.random() * 0.055,
      /** Points plus « digitaux » (teal/cyan) avec un pic de brillance */
      tech: Math.random() < 0.28,
      ph: Math.random() * Math.PI * 2,
    }));
  }

  function frame() {
    ctx.clearRect(0, 0, w, h);
    for (const s of stars) {
      s.tw += s.sp;
      let a = 0.055 + Math.sin(s.tw) * 0.052;
      if (s.tech) {
        a += Math.max(0, Math.sin(s.tw * 2.4 + s.ph)) * 0.14;
      }
      const [cr, cg, cb] = s.tech ? [90, 220, 235] : [140, 195, 210];
      ctx.fillStyle = `rgba(${cr}, ${cg}, ${cb}, ${Math.min(0.95, a)})`;
      ctx.beginPath();
      ctx.arc(s.x, s.y, s.r, 0, Math.PI * 2);
      ctx.fill();
    }
    raf = requestAnimationFrame(frame);
  }

  resize();
  frame();
  window.addEventListener(
    "resize",
    () => {
      cancelAnimationFrame(raf);
      resize();
      raf = requestAnimationFrame(frame);
    },
    { passive: true }
  );
}

function initReveal() {
  const els = document.querySelectorAll("[data-reveal]");
  const reduced =
    window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches;

  const applyDelay = (el) => {
    const d = el.getAttribute("data-reveal-delay");
    if (d) el.style.transitionDelay = `${Number(d) * 0.08}s`;
  };

  if (reduced) {
    els.forEach((el) => {
      applyDelay(el);
      el.classList.add("is-in");
    });
    document.body.classList.add("is-ready");
    return;
  }

  const obs = new IntersectionObserver(
    (entries) => {
      entries.forEach((entry) => {
        if (!entry.isIntersecting) return;
        const el = entry.target;
        applyDelay(el);
        el.classList.add("is-in");
        obs.unobserve(el);
      });
    },
    { rootMargin: "0px 0px -6% 0px", threshold: 0.06 }
  );

  const revealNow = (el) => {
    applyDelay(el);
    el.classList.add("is-in");
  };

  els.forEach((el) => {
    const rect = el.getBoundingClientRect();
    const vh = window.innerHeight || 800;
    const inView = rect.top < vh * 0.92 && rect.bottom > -Math.min(rect.height * 0.35, 120);
    if (inView) revealNow(el);
    else obs.observe(el);
  });

  requestAnimationFrame(() => document.body.classList.add("is-ready"));
}

function initNav() {
  const toggle = document.getElementById("nav-toggle");
  const nav = document.getElementById("site-nav");
  if (!toggle || !nav) return;

  toggle.addEventListener("click", () => {
    const open = nav.classList.toggle("is-open");
    toggle.setAttribute("aria-expanded", String(open));
  });

  nav.querySelectorAll("a").forEach((a) => {
    a.addEventListener("click", () => {
      nav.classList.remove("is-open");
      toggle.setAttribute("aria-expanded", "false");
    });
  });
}

function initTierSwitch() {
  const buttons = document.querySelectorAll(".tier-switch__btn");
  const cards = document.querySelectorAll("[data-tier-card]");
  if (!buttons.length || !cards.length) return;

  const mq = window.matchMedia("(max-width: 960px)");

  function syncFocus(index) {
    cards.forEach((card) => {
      const tier = card.getAttribute("data-tier-card");
      const match = tier === String(index);
      card.classList.toggle("is-focused", mq.matches ? match : false);
    });
  }

  function setTier(index) {
    buttons.forEach((btn, i) => {
      const on = i === index;
      btn.classList.toggle("is-active", on);
      btn.setAttribute("aria-selected", String(on));
    });
    syncFocus(index);
  }

  buttons.forEach((btn, i) => {
    btn.addEventListener("click", () => setTier(i));
  });

  const onMq = () => {
    const active = [...buttons].findIndex((b) => b.classList.contains("is-active"));
    syncFocus(active >= 0 ? active : 1);
  };

  mq.addEventListener("change", onMq);
  setTier(1);
  onMq();
}

function makeBoxartCard(game, duplicate) {
  const fig = document.createElement("figure");
  fig.className = "boxart-card";
  if (duplicate) {
    fig.setAttribute("aria-hidden", "true");
  }
  const img = document.createElement("img");
  img.className = "boxart-card__img";
  img.src = `assets/boxart/${game.file}`;
  img.alt = duplicate ? "" : game.name;
  img.loading = "lazy";
  img.decoding = "async";
  img.width = 160;
  img.height = 213;
  if (duplicate) img.setAttribute("tabindex", "-1");
  const cap = document.createElement("figcaption");
  cap.className = "boxart-card__title";
  cap.textContent = game.name;
  fig.appendChild(img);
  fig.appendChild(cap);
  return fig;
}

function initBoxartCarousel() {
  const track = document.getElementById("boxart-track");
  if (!track) return;

  const lang =
    window.SekaiLinkI18n && typeof SekaiLinkI18n.getLang === "function"
      ? SekaiLinkI18n.getLang()
      : "fr";
  const collator = sortLocaleTag(lang);
  const sorted = [...BOXTART_GAMES].sort((a, b) =>
    a.name.localeCompare(b.name, collator, { sensitivity: "base" })
  );
  sorted.forEach((g) => track.appendChild(makeBoxartCard(g, false)));
  sorted.forEach((g) => track.appendChild(makeBoxartCard(g, true)));

  const reduced =
    window.matchMedia && window.matchMedia("(prefers-reduced-motion: reduce)").matches;
  if (reduced) {
    track.classList.add("boxart-carousel__track--static");
  }
}

function refillChips() {
  const root = document.getElementById("chip-features");
  if (!root) return;
  const labels =
    window.SekaiLinkI18n && typeof SekaiLinkI18n.featuresList === "function"
      ? SekaiLinkI18n.featuresList()
      : FEATURES_FALLBACK;
  root.replaceChildren();
  labels.forEach((label) => {
    const s = document.createElement("span");
    s.className = "chip";
    s.textContent = label;
    root.appendChild(s);
  });
}

document.addEventListener("DOMContentLoaded", () => {
  if (window.SekaiLinkI18n && typeof SekaiLinkI18n.init === "function") {
    SekaiLinkI18n.init();
  }
  window.addEventListener("sekailink:languagechange", refillChips);

  initStars();
  initReveal();
  initNav();
  initTierSwitch();
  initBoxartCarousel();
  refillChips();
});
