/* ============================================================
   SekaiLink Website — Main JavaScript
   Circuit Forge canvas, scroll reveals, game carousel, i18n
   ============================================================ */

(function () {
  'use strict';

  /* ── Circuit Board Canvas Background ── */
  const canvas = document.getElementById('skl-bg-canvas');
  if (canvas) {
    const ctx = canvas.getContext('2d');
    let w, h, particles, traces, grid;
    const PARTICLE_COUNT = 60;
    const TRACE_COUNT = 18;
    const ACCENT = { r: 0, g: 255, b: 200 };

    function resize() {
      w = canvas.width = window.innerWidth;
      h = canvas.height = window.innerHeight;
    }

    function initParticles() {
      particles = [];
      for (let i = 0; i < PARTICLE_COUNT; i++) {
        particles.push({
          x: Math.random() * w,
          y: Math.random() * h,
          vx: (Math.random() - 0.5) * 0.3,
          vy: (Math.random() - 0.5) * 0.3,
          r: Math.random() * 2 + 0.5,
          a: Math.random() * 0.4 + 0.1,
        });
      }
    }

    function initTraces() {
      traces = [];
      for (let i = 0; i < TRACE_COUNT; i++) {
        const horizontal = Math.random() > 0.5;
        const startX = Math.random() * w;
        const startY = Math.random() * h;
        const len = Math.random() * 200 + 80;
        traces.push({
          x1: startX,
          y1: startY,
          x2: horizontal ? startX + len : startX,
          y2: horizontal ? startY : startY + len,
          phase: Math.random() * Math.PI * 2,
          speed: Math.random() * 0.008 + 0.003,
        });
      }
    }

    function initGrid() {
      grid = [];
      const spacing = 80;
      for (let x = 0; x < w + spacing; x += spacing) {
        for (let y = 0; y < h + spacing; y += spacing) {
          if (Math.random() > 0.7) {
            grid.push({ x, y, a: Math.random() * 0.08 + 0.02 });
          }
        }
      }
    }

    function drawGrid(t) {
      ctx.fillStyle = `rgba(${ACCENT.r},${ACCENT.g},${ACCENT.b},0.02)`;
      for (const g of grid) {
        const pulse = Math.sin(t * 0.001 + g.x * 0.01) * 0.02;
        ctx.globalAlpha = g.a + pulse;
        ctx.fillRect(g.x - 1, g.y - 1, 2, 2);
      }
      ctx.globalAlpha = 1;
    }

    function drawTraces(t) {
      for (const tr of traces) {
        const a = (Math.sin(t * tr.speed + tr.phase) + 1) * 0.5 * 0.12 + 0.02;
        ctx.beginPath();
        ctx.moveTo(tr.x1, tr.y1);
        ctx.lineTo(tr.x2, tr.y2);
        ctx.strokeStyle = `rgba(${ACCENT.r},${ACCENT.g},${ACCENT.b},${a})`;
        ctx.lineWidth = 1;
        ctx.setLineDash([4, 6]);
        ctx.stroke();
        ctx.setLineDash([]);
      }
    }

    function drawParticles() {
      for (const p of particles) {
        p.x += p.vx;
        p.y += p.vy;
        if (p.x < 0) p.x = w;
        if (p.x > w) p.x = 0;
        if (p.y < 0) p.y = h;
        if (p.y > h) p.y = 0;

        ctx.beginPath();
        ctx.arc(p.x, p.y, p.r, 0, Math.PI * 2);
        ctx.fillStyle = `rgba(${ACCENT.r},${ACCENT.g},${ACCENT.b},${p.a})`;
        ctx.fill();
      }
    }

    function drawConnections() {
      const maxDist = 120;
      for (let i = 0; i < particles.length; i++) {
        for (let j = i + 1; j < particles.length; j++) {
          const dx = particles[i].x - particles[j].x;
          const dy = particles[i].y - particles[j].y;
          const dist = Math.sqrt(dx * dx + dy * dy);
          if (dist < maxDist) {
            const a = (1 - dist / maxDist) * 0.08;
            ctx.beginPath();
            ctx.moveTo(particles[i].x, particles[i].y);
            ctx.lineTo(particles[j].x, particles[j].y);
            ctx.strokeStyle = `rgba(${ACCENT.r},${ACCENT.g},${ACCENT.b},${a})`;
            ctx.lineWidth = 0.5;
            ctx.stroke();
          }
        }
      }
    }

    let animFrame;
    function animate(t) {
      ctx.clearRect(0, 0, w, h);
      drawGrid(t);
      drawTraces(t);
      drawConnections();
      drawParticles();
      animFrame = requestAnimationFrame(animate);
    }

    function init() {
      resize();
      initParticles();
      initTraces();
      initGrid();
      animate(0);
    }

    const prefersReduced = window.matchMedia('(prefers-reduced-motion: reduce)');
    if (!prefersReduced.matches) {
      init();
      window.addEventListener('resize', () => {
        resize();
        initGrid();
      });
    }
  }

  /* ── Mobile Navigation ── */
  const burger = document.getElementById('skl-mobile-nav');
  if (burger) {
    burger.addEventListener('click', () => {
      burger.closest('.skl-topbar').classList.toggle('open');
    });

    document.querySelectorAll('.skl-topbar-links a').forEach((a) => {
      a.addEventListener('click', () => {
        burger.closest('.skl-topbar').classList.remove('open');
      });
    });
  }

  /* ── Navbar scroll shrink ── */
  const topbar = document.getElementById('skl-topbar');
  if (topbar) {
    let ticking = false;
    window.addEventListener('scroll', () => {
      if (!ticking) {
        requestAnimationFrame(() => {
          if (window.scrollY > 60) {
            topbar.classList.add('scrolled');
          } else {
            topbar.classList.remove('scrolled');
          }
          ticking = false;
        });
        ticking = true;
      }
    });
  }

  /* ── Scroll Reveal (IntersectionObserver) ── */
  const revealEls = document.querySelectorAll('.skl-reveal, .skl-reveal-left, .skl-reveal-right');
  if (revealEls.length && 'IntersectionObserver' in window) {
    const observer = new IntersectionObserver(
      (entries) => {
        entries.forEach((entry) => {
          if (entry.isIntersecting) {
            entry.target.classList.add('visible');
            observer.unobserve(entry.target);
          }
        });
      },
      { threshold: 0.12, rootMargin: '0px 0px -40px 0px' }
    );
    revealEls.forEach((el) => observer.observe(el));
  } else {
    revealEls.forEach((el) => el.classList.add('visible'));
  }

  /* ── Game Carousel (auto-scrolling boxart) ── */
  const track = document.getElementById('skl-games-track');
  if (track) {
    const games = [
      { name: 'A Link to the Past', file: 'a_link_to_the_past.png' },
      { name: 'Pokemon Emerald', file: 'pokemon_emerald.png' },
      { name: 'Ocarina of Time', file: 'ocarina_of_time.webp' },
      { name: 'Super Metroid', file: 'super_metroid.jpg' },
      { name: 'Super Mario 64', file: 'super_mario_64.png' },
      { name: 'Super Mario World', file: 'super_mario_world.png' },
      { name: 'Donkey Kong Country', file: 'donkey_kong_country.png' },
      { name: 'Donkey Kong Country 2', file: 'donkey_kong_country_2.png' },
      { name: 'Donkey Kong Country 3', file: 'donkey_kong_country_3.png' },
      { name: 'EarthBound', file: 'earthbound.png' },
      { name: 'Metroid Fusion', file: 'metroid_fusion.png' },
      { name: 'Metroid Zero Mission', file: 'metroid_zero_mission.png' },
      { name: 'A Link Between Worlds', file: 'a_link_between_worlds.png' },
      { name: 'Pokemon Crystal', file: 'pokemon_crystal.png' },
      { name: 'Ship of Harkinian', file: 'ship_of_harkinian.png' },
      { name: 'The Legend of Zelda', file: 'the_legend_of_zelda.png' },
      { name: 'SMZ3', file: 'smz3.png' },
      { name: 'Mega Man 2', file: 'mega_man_2.png' },
      { name: 'Mega Man 3', file: 'mega_man_3.png' },
      { name: 'Wario Land', file: 'wario_land.png' },
      { name: 'Wario Land 4', file: 'wario_land_4.png' },
      { name: "Yoshi's Island", file: 'yoshis_island.png' },
      { name: 'The Minish Cap', file: 'the_minish_cap.png' },
      { name: 'Pokemon FireRed', file: 'pokemon_firered_and_leafgreen.png' },
      { name: 'FF4 Free Enterprise', file: 'final_fantasy_iv_free_enterprise.png' },
      { name: 'FF Tactics Advance', file: 'final_fantasy_tactics_advance.png' },
      { name: 'Super Mario Land 2', file: 'super_mario_land_2.png' },
      { name: "Kirby's Dream Land 3", file: 'kirbys_dream_land_3.png' },
      { name: 'Lufia II', file: 'lufia_ii_ancient_cave.png' },
      { name: 'Pokemon Red/Blue', file: 'pokemon_red_and_blue.png' },
    ];

    function buildCards(list) {
      return list
        .map(
          (g) => `
        <div class="skl-game-card">
          <img src="assets/boxart/${g.file}" alt="${g.name}" loading="lazy" width="160" height="213">
          <div class="skl-game-card-name">${g.name}</div>
        </div>`
        )
        .join('');
    }

    track.innerHTML = buildCards(games) + buildCards(games);
  }

  /* ── Active nav link highlight ── */
  const sections = document.querySelectorAll('.skl-landing section[id]');
  const navLinks = document.querySelectorAll('.skl-topbar-links a[href^="#"]');

  if (sections.length && navLinks.length) {
    const navObserver = new IntersectionObserver(
      (entries) => {
        entries.forEach((entry) => {
          if (entry.isIntersecting) {
            const id = entry.target.getAttribute('id');
            navLinks.forEach((link) => {
              link.classList.toggle('active', link.getAttribute('href') === `#${id}`);
            });
          }
        });
      },
      { threshold: 0.3, rootMargin: '-72px 0px -50% 0px' }
    );
    sections.forEach((s) => navObserver.observe(s));
  }

  /* ── i18n ── */
  const i18n = {
    en: {
      lang_label: 'Lang',
      nav_features: 'Features',
      nav_steps: 'How It Works',
      nav_verified: 'Link Verified',
      nav_streamers: 'Streamers',
      nav_games: 'Games',
      nav_mori: 'MORI AI',
      nav_download: 'Download',
      badge_version: 'beta-0.02.0 — LIVE',
      hero_title: 'Start your multiworld in <span class="skl-highlight">minutes</span>, not hours.',
      hero_sub:
        'SekaiLink unifies launch, room sync, tracking, and support for faster setup, cleaner sync, and easier recovery.',
      hero_micro:
        'Built for new players, veterans, and streamers who want less setup pain and more gameplay.',
      cta_discord: 'JOIN DISCORD',
      cta_download: 'Download SekaiLink',
      mock_label: 'READY TO LAUNCH',
      badge_linux: 'Linux + Windows',
      badge_games: '30+ Games',
      badge_open: 'Open Source',
      feat_eyebrow: 'Why SekaiLink',
      feat_title: 'A full feature stack designed for real sessions.',
      f1_t: 'One-click Launch Flow',
      f1_b: 'Patch, emulator, tracker and room connection in one smooth launch path. Select your game, click play.',
      f2_t: 'Universal Lua Connector Protocol',
      f2_b: 'ULCP standardizes runtime communication to make connector behavior consistent and easier to debug.',
      f3_t: 'In-app Bug Report',
      f3_b: 'Send title + description + screenshot + logs in a few clicks for fast issue triage. No more Discord-only reports.',
      f4_t: 'Social + Live Presence',
      f4_b: 'Friends, requests, block list, online status, and lobby-aware interactions — all built in.',
      f5_t: 'Streaming-Friendly UI',
      f5_b: 'Fast pre-stream setup, clearer controls, and cleaner room operations designed for live audiences.',
      f6_t: 'Bootstrapper Updater',
      f6_b: 'Unified installer/updater model with repair-ready workflow and channel-aware releases (debug → test → stable).',
      steps_eyebrow: 'How It Works',
      steps_title: 'From install to gameplay in four steps.',
      s1_t: 'Install SekaiLink',
      s1_b: 'Download the bootstrapper. It handles installation, updates, and repairs automatically.',
      s2_t: 'Import Your ROMs',
      s2_b: 'Point SekaiLink to your ROM files once. The Game Manager handles the rest.',
      s3_t: 'Join or Create a Room',
      s3_b: 'Browse the room list, join friends, or create your own multiworld session with custom settings.',
      s4_t: 'Click Launch',
      s4_b: 'SekaiLink patches, launches the emulator, starts tracking, and connects — all in one click.',
      lv_eyebrow: 'Link Verified',
      lv_title: 'Popular games, verified for smoother SekaiLink flow.',
      lv_desc:
        'Link Verified means the game path is validated by SekaiLink for reliable runtime behavior, expected launcher flow, and known support quality.',
      lv1: 'Stable setup path and tested integration',
      lv2: 'Consistent runtime/connector behavior',
      lv3: 'Faster support diagnostics',
      lv4: 'Priority quality updates',
      lv_popular: 'Popular Link Verified picks',
      str_eyebrow: 'For Streamers',
      str_title: 'Go live faster with less prep friction.',
      str_desc:
        'SekaiLink focuses on quick room setup, cleaner launch flow, and easier recovery when things go wrong on stream.',
      str1: 'Rapid room-to-launch workflow',
      str2: 'Integrated bug report pipeline',
      str3: 'Cleaner in-session controls',
      str4: 'Twitch bot integration (coming soon)',
      games_eyebrow: 'Integrated Games',
      games_title: 'SekaiLink integrated roster',
      games_sub: 'Integrated, verified, and maintained for the SekaiLink platform.',
      mori_eyebrow: 'MORI AI',
      mori_title: 'Configure your multiworld with AI assistance.',
      mori_desc:
        'MORI (Multiworld Options Randomizer Intelligence) helps you configure game options through natural language. Describe what you want and MORI generates the YAML.',
      mori_sub:
        'No more guessing slider values or hunting through documentation. MORI knows every option for every supported game.',
      mori_cta: 'Try MORI →',
      mori_q: 'I want a casual ALTTP rando with easy mode and keys on dungeons',
      mori_a:
        "Got it! I've set ALTTP to Easy difficulty with keysanity set to own_dungeons. Here's your YAML — ready to export or tweak further.",
      dl_eyebrow: 'Download',
      dl_title: 'Get SekaiLink',
      dl_sub: 'Choose your platform and get started in under a minute.',
      dl_linux: 'Linux',
      dl_linux_desc: 'AppImage — works on Steam Deck, ROG Ally, and any modern distro.',
      dl_windows: 'Windows',
      dl_windows_desc: 'Bootstrapper installer with auto-update and repair workflow.',
      dl_web: 'Web Client',
      dl_web_desc: 'Browser-based access for configuration, rooms, and MORI AI.',
      dl_btn: 'Download',
      dl_launch: 'Launch Web',
      final_title: 'Ready to play smarter?',
      final_sub: 'Join the community, grab the beta, and launch your next multiworld with less friction.',
      powered_by: 'Powered by the SekaiLink platform and native service stack.',
    },
    fr: {
      lang_label: 'Langue',
      nav_features: 'Fonctionnalités',
      nav_steps: 'Comment ça marche',
      nav_verified: 'Link Verified',
      nav_streamers: 'Streamers',
      nav_games: 'Jeux',
      nav_mori: 'MORI IA',
      nav_download: 'Télécharger',
      badge_version: 'beta-0.02.0 — EN LIGNE',
      hero_title:
        'Lance ton multiworld en <span class="skl-highlight">quelques minutes</span>, pas en plusieurs heures.',
      hero_sub:
        'SekaiLink regroupe le lancement, la synchro de room, le tracking et le support pour un setup plus rapide et une récupération plus simple.',
      hero_micro:
        'Pensé pour les nouveaux joueurs, les vétérans et les streamers qui veulent moins de friction et plus de jeu.',
      cta_discord: 'REJOINDRE DISCORD',
      cta_download: 'Télécharger SekaiLink',
      mock_label: 'PRÊT À LANCER',
      badge_linux: 'Linux + Windows',
      badge_games: '30+ Jeux',
      badge_open: 'Open Source',
      feat_eyebrow: 'Pourquoi SekaiLink',
      feat_title: 'Une stack complète pensée pour les vraies sessions.',
      f1_t: 'Flux de lancement en un clic',
      f1_b: 'Patch, émulateur, tracker et connexion room dans un seul flux fluide.',
      f2_t: 'Universal Lua Connector Protocol',
      f2_b: 'ULCP standardise la communication runtime pour un comportement plus stable et plus simple à déboguer.',
      f3_t: 'Bug Report intégré',
      f3_b: 'Envoie titre + description + capture + logs en quelques clics.',
      f4_t: 'Social + présence live',
      f4_b: 'Amis, requêtes, blocage, statuts en ligne et interactions lobby — tout intégré.',
      f5_t: 'UI orientée streaming',
      f5_b: 'Setup pré-stream rapide, contrôles clairs, opérations room propres.',
      f6_t: 'Bootstrapper Updater',
      f6_b: 'Installateur/mise à jour unifiés avec workflow de réparation et releases par canal.',
      steps_eyebrow: 'Comment ça marche',
      steps_title: "De l'installation au jeu en quatre étapes.",
      s1_t: 'Installe SekaiLink',
      s1_b: "Télécharge le bootstrapper. Il gère l'installation, les mises à jour et les réparations automatiquement.",
      s2_t: 'Importe tes ROMs',
      s2_b: 'Pointe SekaiLink vers tes ROMs une fois. Le Game Manager fait le reste.',
      s3_t: 'Rejoins ou crée une room',
      s3_b: 'Parcours la liste des rooms, rejoins des amis, ou crée ta propre session multiworld.',
      s4_t: 'Clique sur Lancer',
      s4_b: "SekaiLink patche, lance l'émulateur, démarre le tracking et connecte — en un clic.",
      lv_eyebrow: 'Link Verified',
      lv_title: 'Jeux populaires vérifiés pour un flow SekaiLink plus fluide.',
      lv_desc:
        'Link Verified signifie que le parcours du jeu est validé par SekaiLink pour une fiabilité runtime et un support prévisible.',
      lv1: 'Parcours de setup stable et testé',
      lv2: 'Comportement runtime/connecteur cohérent',
      lv3: 'Diagnostic support plus rapide',
      lv4: 'Priorité sur la qualité des mises à jour',
      lv_popular: 'Choix Link Verified populaires',
      str_eyebrow: 'Pour les Streamers',
      str_title: 'Passe en live plus vite, avec moins de friction.',
      str_desc:
        'SekaiLink accélère la préparation room, simplifie le launch et facilite la récupération en cas de problème.',
      str1: 'Workflow room vers launch ultra rapide',
      str2: 'Pipeline bug report intégré',
      str3: 'Contrôles de session plus propres',
      str4: 'Intégration bot Twitch (bientôt)',
      games_eyebrow: 'Jeux intégrés',
      games_title: 'Roster intégré SekaiLink',
      games_sub: 'Intégré, vérifié et maintenu pour la plateforme SekaiLink.',
      mori_eyebrow: 'MORI IA',
      mori_title: "Configure ton multiworld avec l'aide de l'IA.",
      mori_desc:
        "MORI (Multiworld Options Randomizer Intelligence) t'aide à configurer les options de jeu en langage naturel. Décris ce que tu veux et MORI génère le YAML.",
      mori_sub:
        'Plus besoin de deviner les valeurs ou de chercher dans la documentation. MORI connaît chaque option de chaque jeu supporté.',
      mori_cta: 'Essayer MORI →',
      mori_q: 'Je veux un ALTTP rando casual avec mode facile et clés dans les donjons',
      mori_a:
        "C'est noté ! J'ai configuré ALTTP en difficulté Easy avec keysanity sur own_dungeons. Voici ton YAML — prêt à exporter ou ajuster.",
      dl_eyebrow: 'Télécharger',
      dl_title: 'Obtenir SekaiLink',
      dl_sub: 'Choisis ta plateforme et commence en moins d\'une minute.',
      dl_linux: 'Linux',
      dl_linux_desc: 'AppImage — fonctionne sur Steam Deck, ROG Ally et toute distro moderne.',
      dl_windows: 'Windows',
      dl_windows_desc: 'Installateur bootstrapper avec mise à jour auto et workflow de réparation.',
      dl_web: 'Client Web',
      dl_web_desc: 'Accès navigateur pour la configuration, les rooms et MORI IA.',
      dl_btn: 'Télécharger',
      dl_launch: 'Lancer le Web',
      final_title: 'Prêt à jouer plus efficacement ?',
      final_sub:
        'Rejoins la communauté, télécharge la beta, et lance ton prochain multiworld plus vite.',
      powered_by:
        'Propulsé par la plateforme SekaiLink et sa stack de services native.',
    },
    es: {
      lang_label: 'Idioma',
      nav_features: 'Funciones',
      nav_steps: 'Cómo funciona',
      nav_verified: 'Link Verified',
      nav_streamers: 'Streamers',
      nav_games: 'Juegos',
      nav_mori: 'MORI IA',
      nav_download: 'Descargar',
      badge_version: 'beta-0.02.0 — ACTIVA',
      hero_title:
        'Inicia tu multiworld en <span class="skl-highlight">minutos</span>, no en horas.',
      hero_sub:
        'SekaiLink unifica el lanzamiento, la sincronización de salas, el tracking y el soporte para un setup más rápido y una recuperación más clara.',
      hero_micro:
        'Hecho para nuevos jugadores, veteranos y streamers que quieren menos fricción y más juego.',
      cta_discord: 'UNIRSE A DISCORD',
      cta_download: 'Descargar SekaiLink',
      mock_label: 'LISTO PARA LANZAR',
      badge_linux: 'Linux + Windows',
      badge_games: '30+ Juegos',
      badge_open: 'Open Source',
      feat_eyebrow: 'Por qué SekaiLink',
      feat_title: 'Un stack completo para sesiones reales.',
      f1_t: 'Flujo de lanzamiento en un clic',
      f1_b: 'Patch, emulador, tracker y conexión de sala en un solo flujo.',
      f2_t: 'Universal Lua Connector Protocol',
      f2_b: 'ULCP estandariza la comunicación runtime para un comportamiento consistente y fácil de depurar.',
      f3_t: 'Bug report integrado',
      f3_b: 'Envía título + descripción + captura + logs en pocos clics.',
      f4_t: 'Social + presencia en vivo',
      f4_b: 'Amigos, solicitudes, bloqueos, estado online e interacciones de lobby.',
      f5_t: 'UI para streaming',
      f5_b: 'Preparación pre-stream rápida, controles claros y operaciones limpias.',
      f6_t: 'Bootstrapper Updater',
      f6_b: 'Instalador/actualizador unificado con flujo de reparación.',
      steps_eyebrow: 'Cómo funciona',
      steps_title: 'De la instalación al juego en cuatro pasos.',
      s1_t: 'Instala SekaiLink',
      s1_b: 'Descarga el bootstrapper. Maneja instalación, actualizaciones y reparaciones automáticamente.',
      s2_t: 'Importa tus ROMs',
      s2_b: 'Apunta SekaiLink a tus ROMs una vez. El Game Manager hace el resto.',
      s3_t: 'Únete o crea una sala',
      s3_b: 'Navega la lista de salas, únete a amigos, o crea tu propia sesión multiworld.',
      s4_t: 'Haz clic en Lanzar',
      s4_b: 'SekaiLink parchea, lanza el emulador, inicia el tracking y conecta — todo en un clic.',
      lv_eyebrow: 'Link Verified',
      lv_title: 'Juegos populares verificados para un flujo SekaiLink más estable.',
      lv_desc:
        'Link Verified significa que esa ruta de juego fue validada por SekaiLink para confiabilidad runtime y soporte predecible.',
      lv1: 'Ruta de setup estable y probada',
      lv2: 'Comportamiento runtime/conector consistente',
      lv3: 'Diagnóstico de soporte más rápido',
      lv4: 'Actualizaciones de calidad prioritarias',
      lv_popular: 'Picks Link Verified populares',
      str_eyebrow: 'Para Streamers',
      str_title: 'Sal en vivo más rápido y con menos fricción.',
      str_desc:
        'SekaiLink acelera la preparación de sala, simplifica el launch y ayuda a recuperar sesiones.',
      str1: 'Flujo rápido de sala a launch',
      str2: 'Pipeline integrado de bug report',
      str3: 'Controles de sesión más limpios',
      str4: 'Integración con bot de Twitch (próximamente)',
      games_eyebrow: 'Juegos integrados',
      games_title: 'Roster integrado SekaiLink',
      games_sub: 'Integrado, verificado y mantenido para la plataforma SekaiLink.',
      mori_eyebrow: 'MORI IA',
      mori_title: 'Configura tu multiworld con asistencia de IA.',
      mori_desc:
        'MORI te ayuda a configurar opciones de juego con lenguaje natural. Describe lo que quieres y MORI genera el YAML.',
      mori_sub:
        'No más adivinar valores o buscar en la documentación. MORI conoce cada opción de cada juego.',
      mori_cta: 'Probar MORI →',
      mori_q: 'Quiero un ALTTP rando casual con modo fácil y llaves en mazmorras',
      mori_a:
        '¡Listo! Configuré ALTTP en dificultad Easy con keysanity en own_dungeons. Aquí está tu YAML — listo para exportar o ajustar.',
      dl_eyebrow: 'Descargar',
      dl_title: 'Obtener SekaiLink',
      dl_sub: 'Elige tu plataforma y empieza en menos de un minuto.',
      dl_linux: 'Linux',
      dl_linux_desc: 'AppImage — funciona en Steam Deck, ROG Ally y cualquier distro moderna.',
      dl_windows: 'Windows',
      dl_windows_desc: 'Instalador bootstrapper con auto-actualización y flujo de reparación.',
      dl_web: 'Cliente Web',
      dl_web_desc: 'Acceso por navegador para configuración, salas y MORI IA.',
      dl_btn: 'Descargar',
      dl_launch: 'Iniciar Web',
      final_title: '¿Listo para jugar mejor?',
      final_sub:
        'Únete a la comunidad, descarga la beta y lanza tu próximo multiworld con menos fricción.',
      powered_by:
        'Impulsado por la plataforma SekaiLink y su stack nativa de servicios.',
    },
    ja: {
      lang_label: '言語',
      nav_features: '機能',
      nav_steps: '使い方',
      nav_verified: 'Link Verified',
      nav_streamers: '配信者向け',
      nav_games: 'ゲーム',
      nav_mori: 'MORI AI',
      nav_download: 'ダウンロード',
      badge_version: 'beta-0.02.0 — 稼働中',
      hero_title:
        'マルチワールドを<span class="skl-highlight">数分</span>で開始。数時間ではなく。',
      hero_sub:
        'SekaiLink は起動・ルーム同期・トラッキング・サポートを一体化し、セットアップと復旧をより速くします。',
      hero_micro:
        '初心者から上級者、配信者まで。準備の手間を減らしてプレイ時間を増やします。',
      cta_discord: 'DISCORDに参加',
      cta_download: 'SekaiLinkをダウンロード',
      mock_label: '起動準備完了',
      badge_linux: 'Linux + Windows',
      badge_games: '30+ ゲーム',
      badge_open: 'オープンソース',
      feat_eyebrow: 'SekaiLinkの強み',
      feat_title: '実運用セッション向けのフル機能スタック。',
      f1_t: 'ワンクリック起動フロー',
      f1_b: 'パッチ、エミュ、トラッカー、ルーム接続を1つの流れで実行。',
      f2_t: 'Universal Lua Connector Protocol',
      f2_b: 'ULCP により runtime 通信を標準化し、安定性とデバッグ性を向上。',
      f3_t: 'アプリ内バグレポート',
      f3_b: 'タイトル・説明・スクリーンショット・ログを数クリックで送信。',
      f4_t: 'ソーシャル + ライブプレゼンス',
      f4_b: 'フレンド、申請、ブロック、オンライン状態、ロビー連携。',
      f5_t: '配信向けUI',
      f5_b: '配信前準備を短縮し、操作性を改善。',
      f6_t: 'Bootstrapper Updater',
      f6_b: 'インストーラーとアップデーターを統合し、修復フローにも対応。',
      steps_eyebrow: '使い方',
      steps_title: 'インストールからプレイまで4ステップ。',
      s1_t: 'SekaiLinkをインストール',
      s1_b: 'ブートストラッパーをダウンロード。インストール、更新、修復を自動処理。',
      s2_t: 'ROMをインポート',
      s2_b: 'ROMファイルを一度指定。Game Managerが残りを処理。',
      s3_t: 'ルームに参加または作成',
      s3_b: 'ルームリストを閲覧、フレンドに参加、またはカスタムセッションを作成。',
      s4_t: '起動をクリック',
      s4_b: 'SekaiLinkがパッチ、エミュ起動、トラッキング開始、接続 — すべてワンクリック。',
      lv_eyebrow: 'Link Verified',
      lv_title: '人気タイトルを検証済みフローで安定運用。',
      lv_desc:
        'Link Verified は SekaiLink がそのゲーム導線を検証し、安定した runtime とサポート品質を確保していることを示します。',
      lv1: '安定した検証済みセットアップ',
      lv2: '一貫した runtime / コネクタ挙動',
      lv3: '迅速なサポート診断',
      lv4: '品質更新の優先対応',
      lv_popular: '人気 Link Verified タイトル',
      str_eyebrow: '配信者向け',
      str_title: '配信開始までを最短に。',
      str_desc:
        'SekaiLink はルーム準備と起動を高速化し、トラブル時の復帰も簡単にします。',
      str1: 'ルームから起動まで高速',
      str2: '統合バグレポート',
      str3: 'セッション操作を整理',
      str4: 'Twitch ボット連携（近日公開）',
      games_eyebrow: '統合ゲーム',
      games_title: 'SekaiLink 統合ロスター',
      games_sub: 'SekaiLink 向けに統合・検証・保守されたゲーム群です。',
      mori_eyebrow: 'MORI AI',
      mori_title: 'AIアシスタントでマルチワールドを設定。',
      mori_desc:
        'MORI は自然言語でゲームオプションを設定するAIアシスタントです。必要な内容を伝えればYAMLを生成します。',
      mori_sub:
        'スライダーの値を推測したりドキュメントを探す必要はありません。MORIは全ゲームの全オプションを把握しています。',
      mori_cta: 'MORIを試す →',
      mori_q: 'ALTTPのカジュアルランダマイザーをイージーモードでダンジョン鍵あり',
      mori_a:
        '了解！ALTTPをEasy難易度、keysanityをown_dungeonsに設定しました。YAMLは以下 — エクスポートまたは調整できます。',
      dl_eyebrow: 'ダウンロード',
      dl_title: 'SekaiLinkを入手',
      dl_sub: 'プラットフォームを選んで1分以内に開始。',
      dl_linux: 'Linux',
      dl_linux_desc: 'AppImage — Steam Deck、ROG Ally、その他モダンなディストロに対応。',
      dl_windows: 'Windows',
      dl_windows_desc: 'ブートストラッパーインストーラー、自動更新と修復ワークフロー付き。',
      dl_web: 'Webクライアント',
      dl_web_desc: 'ブラウザベースの設定、ルーム、MORI AIアクセス。',
      dl_btn: 'ダウンロード',
      dl_launch: 'Webを開く',
      final_title: 'もっとスマートに遊ぶ準備はできましたか？',
      final_sub:
        'コミュニティに参加し、ベータを入れて、次のマルチワールドをよりスムーズに。',
      powered_by:
        'SekaiLink プラットフォームとネイティブサービス基盤で動作。',
    },
  };

  function applyLang(lang) {
    const dict = i18n[lang] || i18n.en;
    document.querySelectorAll('[data-i18n]').forEach((node) => {
      const key = node.getAttribute('data-i18n');
      if (dict[key]) {
        if (key === 'hero_title') {
          node.innerHTML = dict[key];
        } else {
          node.textContent = dict[key];
        }
      }
    });
    document.documentElement.lang = lang;
    try {
      localStorage.setItem('skl_lang', lang);
    } catch (_) {}
  }

  const langSelect = document.getElementById('skl-lang-select');
  let saved = null;
  try {
    saved = localStorage.getItem('skl_lang');
  } catch (_) {}
  const browserLang = (navigator.language || 'en').toLowerCase();
  const defaultLang =
    saved ||
    (browserLang.startsWith('fr')
      ? 'fr'
      : browserLang.startsWith('es')
        ? 'es'
        : browserLang.startsWith('ja')
          ? 'ja'
          : 'en');

  if (langSelect) {
    langSelect.value = defaultLang;
    langSelect.addEventListener('change', () => applyLang(langSelect.value));
  }
  applyLang(defaultLang);

  /* ── Smooth scroll for anchor links ── */
  document.querySelectorAll('a[href^="#"]').forEach((link) => {
    link.addEventListener('click', (e) => {
      const target = document.querySelector(link.getAttribute('href'));
      if (target) {
        e.preventDefault();
        target.scrollIntoView({ behavior: 'smooth' });
      }
    });
  });
})();
