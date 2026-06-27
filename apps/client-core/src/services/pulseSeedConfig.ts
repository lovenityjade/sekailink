import { apiFetch } from "./api";
import { canonicalSeedGameKey, type SeedEntry, type SeedGameEntry } from "./seedConfig";
import { trace, traceError } from "./trace";

export interface PulseAnswer {
  id: string;
  label: string;
  description?: string;
}

export interface PulseQuestion {
  id: string;
  text: string;
  answers: PulseAnswer[];
}

export interface PulseSeedSession {
  session_id: string;
  game_key: string;
  game_name: string;
  title?: string;
  question?: PulseQuestion;
  complete?: boolean;
  summary?: string;
  seed?: SeedEntry;
  local_answers?: Array<{ question_id: string; answer_id: string; answer_label: string }>;
}

const LOCAL_PULSE_QUESTIONS: PulseQuestion[] = [
  {
    id: "experience",
    text: "What kind of run do you want?",
    answers: [
      { id: "casual", label: "Casual and guided" },
      { id: "balanced", label: "Balanced showcase" },
      { id: "challenge", label: "Extra challenge" },
      { id: "expert", label: "Expert routing" },
      { id: "surprise", label: "Surprise me" },
    ],
  },
  {
    id: "pace",
    text: "How long should the seed feel?",
    answers: [
      { id: "short", label: "Short" },
      { id: "standard", label: "Standard" },
      { id: "long", label: "Long" },
      { id: "hunt", label: "Treasure hunt" },
      { id: "marathon", label: "Marathon" },
    ],
  },
  {
    id: "shuffle",
    text: "How much item shuffle do you want?",
    answers: [
      { id: "simple", label: "Simple" },
      { id: "dungeon_items", label: "Dungeon items" },
      { id: "keysanity", label: "Keysanity" },
      { id: "mixed", label: "Mixed" },
      { id: "maximum", label: "Maximum" },
    ],
  },
  {
    id: "goal",
    text: "What should the win condition be?",
    answers: [
      { id: "final_boss", label: "Final boss" },
      { id: "crystals", label: "Crystal route" },
      { id: "triforce_hunt", label: "Triforce hunt" },
      { id: "bosses", label: "Boss route" },
      { id: "pedestal", label: "Pedestal" },
    ],
  },
  {
    id: "comfort",
    text: "Pick one comfort option.",
    answers: [
      { id: "open_start", label: "Open start" },
      { id: "boots_start", label: "Early mobility" },
      { id: "helpful_hints", label: "Helpful hints" },
      { id: "classic_start", label: "Classic start" },
      { id: "no_comfort", label: "No extra help" },
    ],
  },
];

const localPulseValues = (answers: Array<{ answer_id: string }>): Record<string, unknown> => {
  const ids = new Set(answers.map((answer) => answer.answer_id));
  const values: Record<string, unknown> = {
    glitches_required: "no_glitches",
    accessibility: ids.has("expert") ? "minimal" : "items",
    progression_balancing: ids.has("casual") ? 75 : ids.has("challenge") ? 35 : ids.has("expert") ? 15 : 50,
    mode: ids.has("classic_start") ? "standard" : "open",
  };
  if (ids.has("short")) {
    values.crystals_needed_for_gt = 5;
    values.crystals_needed_for_ganon = 5;
  } else if (ids.has("long") || ids.has("marathon")) {
    values.crystals_needed_for_gt = 7;
    values.crystals_needed_for_ganon = 7;
  }
  if (ids.has("triforce_hunt") || ids.has("hunt")) {
    values.goal = "triforce_hunt";
    values.triforce_pieces_required = ids.has("marathon") ? 30 : 20;
    values.triforce_pieces_available = ids.has("marathon") ? 40 : 30;
  } else if (ids.has("pedestal")) {
    values.goal = "pedestal";
  } else {
    values.goal = "ganon";
  }
  if (ids.has("keysanity") || ids.has("maximum")) {
    values.big_key_shuffle = "own_world";
    values.small_key_shuffle = "own_world";
    values.map_shuffle = "own_world";
    values.compass_shuffle = "own_world";
    values.key_drop_shuffle = true;
  } else if (ids.has("dungeon_items") || ids.has("mixed")) {
    values.big_key_shuffle = "original_dungeon";
    values.small_key_shuffle = "original_dungeon";
    values.map_shuffle = "original_dungeon";
    values.compass_shuffle = "original_dungeon";
  }
  if (ids.has("helpful_hints")) values.hints = "on";
  return values;
};

const localPulseResponse = (
  game: SeedGameEntry,
  answers: Array<{ question_id: string; answer_id: string; answer_label: string }> = []
): PulseSeedSession => {
  const gameKey = canonicalSeedGameKey(game.game_key || game.display_name);
  if (answers.length >= LOCAL_PULSE_QUESTIONS.length) {
    const values = localPulseValues(answers);
    return {
      session_id: `local:${gameKey}`,
      game_key: gameKey,
      game_name: game.display_name,
      title: `Pulse ${game.display_name} Seed`,
      complete: true,
      local_answers: answers,
      summary: `Pulse prepared a guided ${game.display_name} setup from your answers. Review the generated settings before saving.`,
      seed: {
        id: "local-pulse-preview",
        title: `Pulse ${game.display_name} Seed`,
        game: game.display_name,
        game_key: gameKey,
        custom: true,
        source: "pulse",
        description: "Created by Pulse from SekaiLink Easy Config.",
        values,
      },
    };
  }
  return {
    session_id: `local:${gameKey}`,
    game_key: gameKey,
    game_name: game.display_name,
    title: `Pulse ${game.display_name} Seed`,
    complete: false,
    local_answers: answers,
    question: LOCAL_PULSE_QUESTIONS[answers.length],
  };
};

const parsePulseSession = (payload: any, game: SeedGameEntry): PulseSeedSession => {
  const raw = payload?.session || payload;
  const question = raw?.question || payload?.question;
  return {
    session_id: String(raw?.session_id || raw?.id || payload?.session_id || ""),
    game_key: canonicalSeedGameKey(raw?.game_key || game.game_key || game.display_name),
    game_name: String(raw?.game_name || raw?.display_name || game.display_name),
    title: typeof raw?.title === "string" ? raw.title : "",
    question: question
      ? {
          id: String(question.id || question.question_id || ""),
          text: String(question.text || question.prompt || ""),
          answers: Array.isArray(question.answers)
            ? question.answers.slice(0, 5).map((answer: any, index: number) => ({
                id: String(answer?.id || answer?.answer_id || answer?.value || `answer-${index + 1}`),
                label: String(answer?.label || answer?.text || answer?.title || ""),
                description: typeof answer?.description === "string" ? answer.description : "",
              }))
            : [],
        }
      : undefined,
    complete: Boolean(raw?.complete || payload?.complete),
    summary: typeof raw?.summary === "string" ? raw.summary : typeof payload?.summary === "string" ? payload.summary : "",
    seed: raw?.seed || payload?.seed,
  };
};

const requirePulseQuestionShape = (session: PulseSeedSession) => {
  if (session.complete) return session;
  const answerCount = session.question?.answers?.length || 0;
  if (!session.session_id || !session.question?.id || !session.question.text || answerCount !== 5) {
    throw new Error("Pulse did not return a valid 5-answer seed question.");
  }
  return session;
};

export const startPulseSeedSession = async (game: SeedGameEntry): Promise<PulseSeedSession> => {
  const gameKey = canonicalSeedGameKey(game.game_key || game.display_name);
  trace("pulse-seed", "session_start", { gameKey, gameName: game.display_name });
  try {
    const response = await apiFetch("/api/pulse/seed-config/sessions", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        game_key: gameKey,
        game_name: game.display_name,
      }),
    });
    const data = await response.json().catch(() => ({}));
    if (!response.ok) throw new Error(String((data as any)?.error || "Pulse is not available yet."));
    const session = requirePulseQuestionShape(parsePulseSession(data, game));
    trace("pulse-seed", "session_started", {
      sessionId: session.session_id,
      gameKey: session.game_key,
      complete: session.complete === true,
      answerCount: session.question?.answers?.length || 0,
    });
    return session;
  } catch (error) {
    traceError("pulse-seed", "session_start_failed", error, { gameKey });
    trace("pulse-seed", "session_start_local_fallback", { gameKey, gameName: game.display_name }, "warn");
    return requirePulseQuestionShape(localPulseResponse(game));
  }
};

export const answerPulseSeedQuestion = async (
  session: PulseSeedSession,
  answer: PulseAnswer
): Promise<PulseSeedSession> => {
  trace("pulse-seed", "answer_start", {
    sessionId: session.session_id,
    questionId: session.question?.id || "",
    answerId: answer.id,
  });
  if (session.session_id.startsWith("local:")) {
    const answers = [
      ...(session.local_answers || []),
      {
        question_id: session.question?.id || "",
        answer_id: answer.id,
        answer_label: answer.label,
      },
    ];
    return requirePulseQuestionShape(localPulseResponse({
      game_key: session.game_key,
      display_name: session.game_name,
    }, answers));
  }
  try {
    const response = await apiFetch(`/api/pulse/seed-config/sessions/${encodeURIComponent(session.session_id)}/answers`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        question_id: session.question?.id,
        answer_id: answer.id,
        answer_label: answer.label,
      }),
    });
    const data = await response.json().catch(() => ({}));
    if (!response.ok) throw new Error(String((data as any)?.error || "Pulse could not process that answer."));
    const next = requirePulseQuestionShape(parsePulseSession(data, {
      game_key: session.game_key,
      display_name: session.game_name,
    }));
    trace("pulse-seed", "answer_success", {
      sessionId: next.session_id,
      complete: next.complete === true,
      answerCount: next.question?.answers?.length || 0,
    });
    return next;
  } catch (error) {
    traceError("pulse-seed", "answer_failed", error, { sessionId: session.session_id, answerId: answer.id });
    throw error;
  }
};

export const confirmPulseSeedSession = async (session: PulseSeedSession, title?: string): Promise<SeedEntry> => {
  trace("pulse-seed", "confirm_start", { sessionId: session.session_id });
  if (session.session_id.startsWith("local:")) {
    const seedTitle = String(title || session.title || "Pulse Seed").trim() || "Pulse Seed";
    const values = session.seed?.values && typeof session.seed.values === "object" ? session.seed.values : {};
    const response = await apiFetch("/api/yamls/new", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        title: seedTitle,
        game: session.game_name,
        game_key: canonicalSeedGameKey(session.game_key || session.game_name),
        values,
        source: "pulse",
        mode: "pulse",
        description: "Created by Pulse from SekaiLink Easy Config.",
      }),
    });
    const data = await response.json().catch(() => ({}));
    if (!response.ok) throw new Error(String((data as any)?.error || "Pulse could not create the seed."));
    const saved = (data as any)?.yaml || data;
    return {
      id: String(saved?.id || data?.id || ""),
      title: String(saved?.title || seedTitle),
      game: String(saved?.game || session.game_name),
      game_key: canonicalSeedGameKey(session.game_key || session.game_name),
      player_name: typeof saved?.player_name === "string" ? saved.player_name : "",
      custom: true,
      created_at: typeof saved?.created_at === "string" ? saved.created_at : "",
      updated_at: typeof saved?.updated_at === "string" ? saved.updated_at : "",
      source: "pulse",
      description: "Created by Pulse from SekaiLink Easy Config.",
      values,
    };
  }
  try {
    const response = await apiFetch(`/api/pulse/seed-config/sessions/${encodeURIComponent(session.session_id)}/confirm`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ title: String(title || "").trim() }),
    });
    const data = await response.json().catch(() => ({}));
    if (!response.ok) throw new Error(String((data as any)?.error || "Pulse could not create the seed."));
    const seed = (data as any)?.seed || (data as any)?.yaml || data;
    if (!seed?.id) throw new Error("Pulse created a response without a seed id.");
    const parsed = {
      id: String(seed.id),
      title: String(seed.title || seed.name || "Pulse Seed"),
      game: String(seed.game || session.game_name),
      game_key: canonicalSeedGameKey(seed.game_key || session.game_key),
      player_name: typeof seed.player_name === "string" ? seed.player_name : "",
      custom: true,
      created_at: typeof seed.created_at === "string" ? seed.created_at : "",
      updated_at: typeof seed.updated_at === "string" ? seed.updated_at : "",
      source: "pulse",
      description: typeof seed.description === "string" ? seed.description : "Created by Pulse from SekaiLink Easy Config.",
      values: seed.values && typeof seed.values === "object" ? seed.values : undefined,
    };
    trace("pulse-seed", "confirm_success", { sessionId: session.session_id, seedId: parsed.id, title: parsed.title });
    return parsed;
  } catch (error) {
    traceError("pulse-seed", "confirm_failed", error, { sessionId: session.session_id });
    throw error;
  }
};
