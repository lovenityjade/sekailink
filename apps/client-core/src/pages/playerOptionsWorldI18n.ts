import { LocaleCode } from "../i18n/types";

type ChoiceDict = Record<string, string>;

type WorldLocaleMap = {
  groups?: Record<string, string>;
  options?: Record<string, string>;
  docs?: Record<string, string>;
  choices?: Record<string, ChoiceDict>;
};

type WorldMap = Partial<Record<LocaleCode, WorldLocaleMap>>;

const normalizeGame = (value: string) =>
  String(value || "")
    .trim()
    .toLowerCase()
    .replace(/[^a-z0-9]+/g, "");

const WORLD_TRANSLATIONS: Record<string, WorldMap> = {
  alinktothepast: {
    fr: {
      groups: {
        "Game Options": "Options de jeu",
        "Item & Location Options": "Objets et emplacements",
        "Dungeon & Boss Options": "Donjons et boss",
        "Enemy & Difficulty Options": "Ennemis et difficulté",
        "Generation Options": "Options de génération",
        "Aesthetics & Accessibility": "Accessibilité et visuel"
      },
      options: {
        Accessibility: "Accessibilité",
        Goal: "Objectif",
        Mode: "Mode",
        "Glitches Required": "Glitches requis",
        "Dark Room Logic": "Logique des salles sombres",
        "Open Pyramid Hole": "Pyramide ouverte",
        "Crystals for GT": "Cristaux pour GT",
        "Crystals for Ganon": "Cristaux pour Ganon",
        "Triforce Pieces Mode": "Mode pièces de Triforce",
        "Triforce Pieces Percentage": "Pourcentage de pièces de Triforce",
        "Triforce Pieces Required": "Pièces requises",
        "Triforce Pieces Available": "Pièces disponibles",
        "Triforce Pieces Extra": "Pièces bonus",
        "Entrance Shuffle": "Mélange des entrées",
        "Entrance Shuffle Seed": "Graine du mélange des entrées",
        "Boss Shuffle": "Mélange des boss",
        "Enemy Shuffle": "Mélange des ennemis",
        "Enemy Damage": "Dégâts ennemis",
        "Enemy Health": "Vie ennemie",
        "Menu Speed": "Vitesse des menus",
        "Play music": "Musique",
        "Reduce Screen Flashes": "Réduire les flashs écran",
        "Display Method for Triforce Hunt": "Affichage chasse Triforce",
        Sprite: "Sprite",
        "Sprite Pool": "Pool de sprites",
        "Random Sprite on Hit": "Sprite aléatoire à l'impact",
        "Random Sprite on Enter": "Sprite aléatoire en entrant",
        "Random Sprite on Exit": "Sprite aléatoire en sortant",
        "Random Sprite on Slash": "Sprite aléatoire à l'épée",
        "Random Sprite on Item": "Sprite aléatoire sur objet",
        "Random Sprite on Bonk": "Sprite aléatoire au choc",
        "Random Sprite on Everything": "Sprite aléatoire partout"
      },
      docs: {
        Goal: "Définit la condition de victoire de la partie.",
        Mode: "Choisit la logique d'accès du monde (ouvert, standard...).",
        "Glitches Required": "Niveau de techniques/glitches attendu pour la logique.",
        "Crystals for Ganon": "Nombre de cristaux nécessaires pour affronter Ganon.",
        "Crystals for GT": "Nombre de cristaux nécessaires pour ouvrir la Tour de Ganon."
      },
      choices: {
        glitches_required: {
          no_glitches: "Aucun glitch",
          minor_glitches: "Glitches mineurs",
          overworld_glitches: "Glitches overworld",
          hybrid_major_glitches: "Glitches majeurs hybrides",
          no_logic: "Sans logique",
          none: "Aucun glitch",
          noglitches: "Aucun glitch",
          nologic: "Sans logique"
        }
      }
    }
  }
};

const SIMPLE_WORLD_OPTION_NAMES: Record<string, string[]> = {
  alinktothepast: [
    "accessibility",
    "goal",
    "mode",
    "glitches_required",
    "dark_room_logic",
    "crystals_needed_for_gt",
    "crystals_needed_for_ganon",
    "triforce_pieces_mode",
    "triforce_pieces_required",
    "triforce_pieces_available",
    "entrance_shuffle",
    "boss_shuffle",
    "enemy_shuffle",
    "enemy_damage",
    "enemy_health",
    "menu_speed",
    "music",
    "reduceflashing"
  ]
};

const normalizeChoiceValue = (value: string | number | boolean) =>
  String(value).trim().toLowerCase();

const GENERIC_GROUP_TRANSLATIONS: Partial<Record<LocaleCode, Record<string, string>>> = {
  fr: {
    "Game Options": "Options de jeu",
    "Item & Location Options": "Objets et emplacements",
    "Generation Options": "Options de generation",
    "Aesthetics & Accessibility": "Accessibilite et visuel",
    "Enemy & Difficulty Options": "Ennemis et difficulte",
    "Dungeon & Boss Options": "Donjons et boss",
  },
  es: {
    "Game Options": "Opciones de juego",
    "Item & Location Options": "Objetos y ubicaciones",
    "Generation Options": "Opciones de generacion",
    "Aesthetics & Accessibility": "Accesibilidad y estetica",
    "Enemy & Difficulty Options": "Enemigos y dificultad",
    "Dungeon & Boss Options": "Mazmorras y jefes",
  },
  ja: {
    "Game Options": "ゲームオプション",
    "Item & Location Options": "アイテムとロケーション",
    "Generation Options": "生成オプション",
    "Aesthetics & Accessibility": "見た目とアクセシビリティ",
    "Enemy & Difficulty Options": "敵と難易度",
    "Dungeon & Boss Options": "ダンジョンとボス",
  },
  "zh-CN": {
    "Game Options": "游戏选项",
    "Item & Location Options": "物品与地点",
    "Generation Options": "生成选项",
    "Aesthetics & Accessibility": "外观与辅助功能",
    "Enemy & Difficulty Options": "敌人与难度",
    "Dungeon & Boss Options": "地牢与Boss",
  },
  "zh-TW": {
    "Game Options": "遊戲選項",
    "Item & Location Options": "物品與地點",
    "Generation Options": "生成選項",
    "Aesthetics & Accessibility": "外觀與輔助功能",
    "Enemy & Difficulty Options": "敵人與難度",
    "Dungeon & Boss Options": "地城與Boss",
  },
};

const GENERIC_OPTION_NAME_TRANSLATIONS: Partial<Record<LocaleCode, Record<string, string>>> = {
  fr: {
    accessibility: "Accessibilite",
    goal: "Objectif",
    mode: "Mode",
    logic: "Logique",
    glitches_required: "Glitches requis",
    difficulty: "Difficulte",
    hints: "Indices",
    hint_cost: "Cout des indices",
    progression_balancing: "Equilibrage de progression",
    item_pool: "Pool d'objets",
    entrance_shuffle: "Melange des entrees",
    boss_shuffle: "Melange des boss",
    enemy_shuffle: "Melange des ennemis",
    enemy_damage: "Degats ennemis",
    enemy_health: "Vie ennemie",
    music: "Musique",
    menu_speed: "Vitesse des menus",
    death_link: "Lien de mort",
  },
  es: {
    accessibility: "Accesibilidad",
    goal: "Objetivo",
    mode: "Modo",
    logic: "Logica",
    glitches_required: "Glitches requeridos",
    difficulty: "Dificultad",
    hints: "Pistas",
    hint_cost: "Costo de pistas",
    progression_balancing: "Balance de progresion",
    item_pool: "Pool de objetos",
    entrance_shuffle: "Mezcla de entradas",
    boss_shuffle: "Mezcla de jefes",
    enemy_shuffle: "Mezcla de enemigos",
    enemy_damage: "Danio enemigo",
    enemy_health: "Salud enemiga",
    music: "Musica",
    menu_speed: "Velocidad del menu",
    death_link: "Enlace de muerte",
  },
  ja: {
    accessibility: "アクセシビリティ",
    goal: "目標",
    mode: "モード",
    logic: "ロジック",
    glitches_required: "必要グリッチ",
    difficulty: "難易度",
    hints: "ヒント",
    hint_cost: "ヒントコスト",
    progression_balancing: "進行バランス",
    item_pool: "アイテムプール",
    entrance_shuffle: "入口シャッフル",
    boss_shuffle: "ボスシャッフル",
    enemy_shuffle: "敵シャッフル",
    enemy_damage: "敵ダメージ",
    enemy_health: "敵体力",
    music: "音楽",
    menu_speed: "メニュー速度",
    death_link: "デスリンク",
  },
  "zh-CN": {
    accessibility: "辅助功能",
    goal: "目标",
    mode: "模式",
    logic: "逻辑",
    glitches_required: "所需漏洞技巧",
    difficulty: "难度",
    hints: "提示",
    hint_cost: "提示消耗",
    progression_balancing: "进度平衡",
    item_pool: "物品池",
    entrance_shuffle: "入口洗牌",
    boss_shuffle: "Boss洗牌",
    enemy_shuffle: "敌人洗牌",
    enemy_damage: "敌人伤害",
    enemy_health: "敌人生命",
    music: "音乐",
    menu_speed: "菜单速度",
    death_link: "死亡链接",
  },
  "zh-TW": {
    accessibility: "輔助功能",
    goal: "目標",
    mode: "模式",
    logic: "邏輯",
    glitches_required: "所需漏洞技巧",
    difficulty: "難度",
    hints: "提示",
    hint_cost: "提示成本",
    progression_balancing: "進度平衡",
    item_pool: "物品池",
    entrance_shuffle: "入口洗牌",
    boss_shuffle: "Boss洗牌",
    enemy_shuffle: "敵人洗牌",
    enemy_damage: "敵人傷害",
    enemy_health: "敵人生命",
    music: "音樂",
    menu_speed: "選單速度",
    death_link: "死亡連結",
  },
};

const GENERIC_CHOICE_TRANSLATIONS: Partial<Record<LocaleCode, Record<string, string>>> = {
  fr: {
    true: "Oui",
    false: "Non",
    yes: "Oui",
    no: "Non",
    random: "Aleatoire",
    default: "Defaut",
    normal: "Normal",
    hard: "Difficile",
    easy: "Facile",
    standard: "Standard",
    open: "Ouvert",
  },
  es: {
    true: "Si",
    false: "No",
    yes: "Si",
    no: "No",
    random: "Aleatorio",
    default: "Predeterminado",
    normal: "Normal",
    hard: "Dificil",
    easy: "Facil",
    standard: "Estandar",
    open: "Abierto",
  },
  ja: {
    true: "はい",
    false: "いいえ",
    yes: "はい",
    no: "いいえ",
    random: "ランダム",
    default: "デフォルト",
    normal: "通常",
    hard: "ハード",
    easy: "イージー",
    standard: "標準",
    open: "オープン",
  },
  "zh-CN": {
    true: "是",
    false: "否",
    yes: "是",
    no: "否",
    random: "随机",
    default: "默认",
    normal: "普通",
    hard: "困难",
    easy: "简单",
    standard: "标准",
    open: "开放",
  },
  "zh-TW": {
    true: "是",
    false: "否",
    yes: "是",
    no: "否",
    random: "隨機",
    default: "預設",
    normal: "普通",
    hard: "困難",
    easy: "簡單",
    standard: "標準",
    open: "開放",
  },
};

const GENERIC_OPTION_TOKEN_TRANSLATIONS: Partial<Record<LocaleCode, Record<string, string>>> = {
  fr: {
    accessibility: "accessibilite",
    allow: "autoriser",
    balancing: "equilibrage",
    boss: "boss",
    cost: "cout",
    crystal: "cristal",
    crystals: "cristaux",
    death: "mort",
    difficulty: "difficulte",
    display: "affichage",
    enemy: "ennemi",
    enter: "entrer",
    entrance: "entree",
    extra: "bonus",
    for: "pour",
    game: "jeu",
    ganon: "ganon",
    glitches: "glitches",
    goal: "objectif",
    gt: "gt",
    health: "vie",
    hint: "indice",
    hints: "indices",
    item: "objet",
    items: "objets",
    link: "lien",
    location: "emplacement",
    logic: "logique",
    menu: "menu",
    mode: "mode",
    music: "musique",
    needed: "necessaires",
    open: "ouvert",
    options: "options",
    pieces: "pieces",
    pool: "pool",
    progression: "progression",
    pyramid: "pyramide",
    random: "aleatoire",
    reduce: "reduire",
    required: "requis",
    room: "salle",
    screen: "ecran",
    seed: "graine",
    shuffle: "melange",
    speed: "vitesse",
    sprite: "sprite",
    triforce: "triforce",
  },
  es: {
    accessibility: "accesibilidad",
    allow: "permitir",
    balancing: "balance",
    boss: "jefe",
    cost: "costo",
    crystal: "cristal",
    crystals: "cristales",
    death: "muerte",
    difficulty: "dificultad",
    display: "visualizacion",
    enemy: "enemigo",
    enter: "entrar",
    entrance: "entrada",
    extra: "extra",
    for: "para",
    game: "juego",
    ganon: "ganon",
    glitches: "glitches",
    goal: "objetivo",
    gt: "gt",
    health: "salud",
    hint: "pista",
    hints: "pistas",
    item: "objeto",
    items: "objetos",
    link: "enlace",
    location: "ubicacion",
    logic: "logica",
    menu: "menu",
    mode: "modo",
    music: "musica",
    needed: "necesarios",
    open: "abierto",
    options: "opciones",
    pieces: "piezas",
    pool: "pool",
    progression: "progresion",
    pyramid: "piramide",
    random: "aleatorio",
    reduce: "reducir",
    required: "requeridos",
    room: "sala",
    screen: "pantalla",
    seed: "semilla",
    shuffle: "mezcla",
    speed: "velocidad",
    sprite: "sprite",
    triforce: "trifuerza",
  },
  ja: {
    accessibility: "アクセシビリティ",
    allow: "許可",
    balancing: "バランス",
    boss: "ボス",
    cost: "コスト",
    crystal: "クリスタル",
    crystals: "クリスタル",
    death: "死亡",
    difficulty: "難易度",
    display: "表示",
    enemy: "敵",
    enter: "入る",
    entrance: "入口",
    extra: "追加",
    for: "用",
    game: "ゲーム",
    ganon: "ガノン",
    glitches: "グリッチ",
    goal: "目標",
    gt: "GT",
    health: "体力",
    hint: "ヒント",
    hints: "ヒント",
    item: "アイテム",
    items: "アイテム",
    link: "リンク",
    location: "ロケーション",
    logic: "ロジック",
    menu: "メニュー",
    mode: "モード",
    music: "音楽",
    needed: "必要",
    open: "オープン",
    options: "オプション",
    pieces: "欠片",
    pool: "プール",
    progression: "進行",
    pyramid: "ピラミッド",
    random: "ランダム",
    reduce: "軽減",
    required: "必須",
    room: "部屋",
    screen: "画面",
    seed: "シード",
    shuffle: "シャッフル",
    speed: "速度",
    sprite: "スプライト",
    triforce: "トライフォース",
  },
  "zh-CN": {
    accessibility: "辅助功能",
    allow: "允许",
    balancing: "平衡",
    boss: "Boss",
    cost: "消耗",
    crystal: "水晶",
    crystals: "水晶",
    death: "死亡",
    difficulty: "难度",
    display: "显示",
    enemy: "敌人",
    enter: "进入",
    entrance: "入口",
    extra: "额外",
    for: "用于",
    game: "游戏",
    ganon: "甘侬",
    glitches: "漏洞技巧",
    goal: "目标",
    gt: "GT",
    health: "生命",
    hint: "提示",
    hints: "提示",
    item: "物品",
    items: "物品",
    link: "链接",
    location: "地点",
    logic: "逻辑",
    menu: "菜单",
    mode: "模式",
    music: "音乐",
    needed: "所需",
    open: "开放",
    options: "选项",
    pieces: "碎片",
    pool: "池",
    progression: "进度",
    pyramid: "金字塔",
    random: "随机",
    reduce: "减少",
    required: "需要",
    room: "房间",
    screen: "屏幕",
    seed: "种子",
    shuffle: "洗牌",
    speed: "速度",
    sprite: "精灵",
    triforce: "三角力量",
  },
  "zh-TW": {
    accessibility: "輔助功能",
    allow: "允許",
    balancing: "平衡",
    boss: "Boss",
    cost: "成本",
    crystal: "水晶",
    crystals: "水晶",
    death: "死亡",
    difficulty: "難度",
    display: "顯示",
    enemy: "敵人",
    enter: "進入",
    entrance: "入口",
    extra: "額外",
    for: "用於",
    game: "遊戲",
    ganon: "加農",
    glitches: "漏洞技巧",
    goal: "目標",
    gt: "GT",
    health: "生命",
    hint: "提示",
    hints: "提示",
    item: "物品",
    items: "物品",
    link: "連結",
    location: "地點",
    logic: "邏輯",
    menu: "選單",
    mode: "模式",
    music: "音樂",
    needed: "所需",
    open: "開放",
    options: "選項",
    pieces: "碎片",
    pool: "池",
    progression: "進度",
    pyramid: "金字塔",
    random: "隨機",
    reduce: "減少",
    required: "需要",
    room: "房間",
    screen: "畫面",
    seed: "種子",
    shuffle: "洗牌",
    speed: "速度",
    sprite: "精靈",
    triforce: "三角神力",
  },
};

const formatLocalizedOptionFallback = (optionName: string, locale: LocaleCode): string | null => {
  if (!optionName || locale === "en") return null;
  const tokenMap = GENERIC_OPTION_TOKEN_TRANSLATIONS[locale];
  if (!tokenMap) return null;
  const parts = optionName
    .toLowerCase()
    .split(/[_\s]+/)
    .filter(Boolean);
  if (!parts.length) return null;
  const translated = parts.map((part) => tokenMap[part] || part);
  const hasLocaleWord = translated.some((part, idx) => part !== parts[idx]);
  if (!hasLocaleWord) return null;
  if (locale === "ja" || locale.startsWith("zh")) return translated.join("");
  return translated.join(" ");
};

const formatLocalizedFreeText = (text: string, locale: LocaleCode): string | null => {
  if (!text || locale === "en") return null;
  const tokenMap = GENERIC_OPTION_TOKEN_TRANSLATIONS[locale];
  if (!tokenMap) return null;
  const chunks = String(text).split(/([^A-Za-z0-9_]+)/);
  let changed = false;
  const translated = chunks.map((chunk) => {
    if (!/^[A-Za-z0-9_]+$/.test(chunk)) return chunk;
    const parts = chunk
      .toLowerCase()
      .split(/[_\s]+/)
      .filter(Boolean);
    if (!parts.length) return chunk;
    const mapped = parts.map((part) => tokenMap[part] || part);
    if (mapped.some((part, idx) => part !== parts[idx])) changed = true;
    if (locale === "ja" || locale.startsWith("zh")) return mapped.join("");
    return mapped.join(" ");
  });
  if (!changed) return null;
  return translated.join("");
};

export const makeWorldOptionTranslator = (locale: LocaleCode, gameId: string) => {
  const gameKey = normalizeGame(gameId);
  const world = WORLD_TRANSLATIONS[gameKey];
  const localized = world?.[locale] || world?.en;

  const groupTranslated = (text: string) =>
    localized?.groups?.[text] ||
    GENERIC_GROUP_TRANSLATIONS[locale]?.[text] ||
    text;

  const option = (optionName: string, fallback: string) => {
    const byDisplay = localized?.options?.[fallback];
    if (byDisplay) return byDisplay;
    const byName = localized?.options?.[optionName];
    const byToken = formatLocalizedOptionFallback(optionName, locale);
    return (
      byName ||
      GENERIC_OPTION_NAME_TRANSLATIONS[locale]?.[optionName.toLowerCase()] ||
      byToken ||
      fallback
    );
  };

  const doc = (optionName: string, fallback: string) => {
    const byDisplay = localized?.docs?.[optionName];
    if (byDisplay) return byDisplay;
    const byToken = formatLocalizedFreeText(fallback, locale);
    if (byToken) return byToken;
    return fallback;
  };

  const choice = (
    optionName: string,
    optionValue: string | number | boolean,
    fallback: string
  ) => {
    const key = optionName.toLowerCase();
    const valueKey = normalizeChoiceValue(optionValue);
    const map = localized?.choices?.[key];
    const fromWorld = map?.[valueKey];
    if (fromWorld) return fromWorld;
    const generic = GENERIC_CHOICE_TRANSLATIONS[locale]?.[valueKey];
    if (generic) return generic;
    const byToken = formatLocalizedOptionFallback(String(optionValue), locale);
    return byToken || fallback;
  };

  return { group: groupTranslated, option, doc, choice };
};

export const getSimpleOptionSet = (gameId: string, optionNames: string[]) => {
  const gameKey = normalizeGame(gameId);
  const mapped = SIMPLE_WORLD_OPTION_NAMES[gameKey];
  if (mapped && mapped.length) return new Set(mapped.map((name) => name.toLowerCase()));

  const fallback = optionNames
    .filter((name) =>
      /(goal|mode|logic|difficulty|enemy|hint|access|music|speed|shuffle|crystal|triforce)/i.test(
        name
      )
    )
    .slice(0, 14)
    .map((name) => name.toLowerCase());
  return new Set(fallback);
};
