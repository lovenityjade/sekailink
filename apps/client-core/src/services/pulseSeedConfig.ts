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
}

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
    throw error;
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

export const confirmPulseSeedSession = async (session: PulseSeedSession): Promise<SeedEntry> => {
  trace("pulse-seed", "confirm_start", { sessionId: session.session_id });
  try {
    const response = await apiFetch(`/api/pulse/seed-config/sessions/${encodeURIComponent(session.session_id)}/confirm`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({}),
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
      updated_at: typeof seed.updated_at === "string" ? seed.updated_at : "",
      source: "pulse",
      values: seed.values && typeof seed.values === "object" ? seed.values : undefined,
    };
    trace("pulse-seed", "confirm_success", { sessionId: session.session_id, seedId: parsed.id, title: parsed.title });
    return parsed;
  } catch (error) {
    traceError("pulse-seed", "confirm_failed", error, { sessionId: session.session_id });
    throw error;
  }
};
