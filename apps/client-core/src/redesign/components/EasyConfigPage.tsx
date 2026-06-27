import { useState, useEffect } from 'react';
import { ArrowLeft, Sparkles } from 'lucide-react';
import { ImageWithFallback } from './figma/ImageWithFallback';
import pulseImage from '../../imports/pulse.png';
import { confirmPulseSeedSession, answerPulseSeedQuestion, startPulseSeedSession, type PulseAnswer, type PulseSeedSession } from '../../services/pulseSeedConfig';
import { ALTTP_SHOWCASE_GAME, type SeedEntry, type SeedGameEntry } from '../../services/seedConfig';
import { trace, traceError } from '../../services/trace';
import { ErrorModal, LoadingModal } from './FeedbackModal';

interface EasyConfigPageProps {
  game?: SeedGameEntry;
  onBack: () => void;
  onComplete: (config: SeedEntry) => void;
}

const completeAnswers: PulseAnswer[] = [
  { id: 'confirm', label: 'Create this seed' },
  { id: 'restart-easier', label: 'Start over easier' },
  { id: 'restart-shorter', label: 'Start over shorter' },
  { id: 'restart-harder', label: 'Start over harder' },
  { id: 'restart', label: 'Start over' },
];

const summarizeValue = (value: unknown): string => {
  if (Array.isArray(value)) return value.map(summarizeValue).join(', ');
  if (value && typeof value === 'object') return JSON.stringify(value);
  if (typeof value === 'boolean') return value ? 'On' : 'Off';
  return String(value ?? '');
};

const settingLabel = (key: string): string =>
  key
    .replace(/[_-]+/g, ' ')
    .replace(/\b\w/g, (char) => char.toUpperCase());

const describeChoice = (key: string, value: unknown): string => {
  const text = summarizeValue(value).replace(/[_-]+/g, ' ').trim();
  if (!text) return '';
  const normalized = text.toLowerCase();
  if (key === 'goal') {
    if (normalized.includes('triforce')) return 'un objectif de chasse aux morceaux de Triforce';
    if (normalized.includes('pedestal')) return 'un objectif alternatif autour du pedestal';
    if (normalized.includes('ganon') || normalized.includes('final')) return 'un objectif classique vers le boss final';
    return `un objectif ${text}`;
  }
  if (key === 'mode') {
    if (normalized.includes('open')) return 'un depart ouvert pour reduire les blocages du debut';
    if (normalized.includes('standard')) return 'un depart plus proche de la structure originale';
    return `un depart ${text}`;
  }
  if (key === 'accessibility') {
    if (normalized.includes('minimal')) return 'une logique plus permissive qui peut demander plus de routing';
    if (normalized.includes('items')) return 'une accessibilite centree sur les items importants';
    return `une accessibilite ${text}`;
  }
  if (key.includes('key') || key.includes('map') || key.includes('compass')) {
    if (normalized.includes('own world') || normalized.includes('any world') || normalized.includes('keysanity')) {
      return 'des items de donjon plus melanges, donc un tracker plus important';
    }
    if (normalized.includes('original dungeon') || normalized.includes('vanilla')) {
      return 'des items de donjon gardes plus pres de leur structure normale';
    }
  }
  if (key === 'hints') return normalized === 'on' || normalized === 'true' ? 'des indices actives' : 'des indices desactives';
  if (key === 'progression_balancing') return `un progression balancing regle a ${text}`;
  if (key.includes('crystals_needed')) return `${text} cristaux requis pour une partie de la fin`;
  if (key.includes('triforce_pieces_required')) return `${text} morceaux requis`;
  return `${settingLabel(key)}: ${text}`;
};

const buildSeedParagraph = (session: PulseSeedSession | null): string => {
  if (!session?.complete) return '';
  const gameName = session.game_name || 'ce jeu';
  const values = session.seed?.values && typeof session.seed.values === 'object' ? session.seed.values : null;
  if (!values) {
    return `Pulse a termine les 5 questions pour ${gameName}, mais le module de resume n'a pas renvoye les valeurs detaillees. La seed peut quand meme etre creee avec les reponses enregistrees; si ce n'est pas ce que tu voulais, recommence la demande aupres de Pulse.`;
  }
  const priorityKeys = [
    'goal',
    'mode',
    'accessibility',
    'progression_balancing',
    'big_key_shuffle',
    'small_key_shuffle',
    'map_shuffle',
    'compass_shuffle',
    'key_drop_shuffle',
    'hints',
    'crystals_needed_for_gt',
    'crystals_needed_for_ganon',
    'triforce_pieces_required',
  ];
  const descriptions: string[] = [];
  const seen = new Set<string>();
  for (const key of priorityKeys) {
    if (!Object.prototype.hasOwnProperty.call(values, key)) continue;
    const description = describeChoice(key, (values as Record<string, unknown>)[key]);
    if (!description || seen.has(description)) continue;
    seen.add(description);
    descriptions.push(description);
    if (descriptions.length >= 5) break;
  }
  if (!descriptions.length) {
    return `Pulse a prepare une configuration ${gameName} avec les choix enregistres pendant les 5 questions. Relis le nom et les settings ci-dessous avant de creer la seed; tu peux recommencer si le resultat ne correspond pas a ton intention.`;
  }
  return `Pulse a prepare une configuration ${gameName} avec ${descriptions.join(', ')}. Ce resume est la traduction humaine des settings qui seront sauvegardes; si le style de run ne correspond pas a ce que tu voulais, retourne poser une nouvelle demande a Pulse avant de creer la seed.`;
};

export default function EasyConfigPage({ game = ALTTP_SHOWCASE_GAME, onBack, onComplete }: EasyConfigPageProps) {
  const [currentQuestionIndex, setCurrentQuestionIndex] = useState(0);
  const [session, setSession] = useState<PulseSeedSession | null>(null);
  const [displayedText, setDisplayedText] = useState('');
  const [isTyping, setIsTyping] = useState(true);
  const [loading, setLoading] = useState(true);
  const [submitting, setSubmitting] = useState(false);
  const [error, setError] = useState('');
  const [seedTitle, setSeedTitle] = useState('');

  const startSession = async () => {
    setLoading(true);
    setSubmitting(false);
    setError('');
    setCurrentQuestionIndex(0);
    setSeedTitle('');
    trace('easy-config-page', 'session_start', { gameKey: game.game_key, gameName: game.display_name });
    try {
      const next = await startPulseSeedSession(game);
      setSession(next);
      trace('easy-config-page', 'session_started', {
        sessionId: next.session_id,
        complete: next.complete === true,
        answerCount: next.question?.answers?.length || 0,
      });
    } catch (err) {
      setSession(null);
      traceError('easy-config-page', 'session_failed', err, { gameKey: game.game_key });
      setError(err instanceof Error ? err.message : 'Pulse is not available yet.');
    } finally {
      setLoading(false);
    }
  };

  useEffect(() => {
    void startSession();
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [game.game_key]);

  const currentAnswers = session?.complete ? completeAnswers : session?.question?.answers || [];
  useEffect(() => {
    if (!session?.complete || seedTitle.trim()) return;
    setSeedTitle(session.title || `Pulse ${session.game_name} Seed`);
  }, [seedTitle, session?.complete, session?.game_name, session?.title]);

  const generatedSettingsSummary = session?.complete && session.seed?.values && typeof session.seed.values === 'object'
    ? Object.entries(session.seed.values)
        .filter(([, value]) => value !== undefined && value !== null && summarizeValue(value).trim())
        .slice(0, 10)
    : [];
  const generatedSeedParagraph = buildSeedParagraph(session);
  const fullText = loading
    ? "Pulse is waking up the live configuration assistant..."
    : error
      ? error
      : session?.complete
        ? session.summary || "I have a complete seed configuration ready. Does this look good?"
        : session?.question?.text || "Pulse is waiting for the next question.";

  // Typewriting effect
  useEffect(() => {
    setDisplayedText('');
    setIsTyping(true);
    let currentIndex = 0;

    const interval = setInterval(() => {
      if (currentIndex <= fullText.length) {
        setDisplayedText(fullText.slice(0, currentIndex));
        currentIndex++;
      } else {
        setIsTyping(false);
        clearInterval(interval);
      }
    }, 30);

    return () => clearInterval(interval);
  }, [fullText]);

  const handleAnswerClick = async (answer: PulseAnswer) => {
    if (isTyping || loading || submitting) return;
    if (error || !session) {
      void startSession();
      return;
    }

    if (session.complete) {
      if (answer.id === 'confirm') {
        setSubmitting(true);
        setError('');
        trace('easy-config-page', 'confirm_start', { sessionId: session.session_id });
        try {
          const seed = await confirmPulseSeedSession(session, seedTitle);
          trace('easy-config-page', 'confirm_success', { sessionId: session.session_id, seedId: seed.id, title: seed.title });
          onComplete(seed);
          onBack();
        } catch (err) {
          traceError('easy-config-page', 'confirm_failed', err, { sessionId: session.session_id });
          setError(err instanceof Error ? err.message : 'Pulse could not create the seed.');
        } finally {
          setSubmitting(false);
        }
      } else {
        void startSession();
      }
      return;
    }

    setSubmitting(true);
    setError('');
    trace('easy-config-page', 'answer_click', {
      sessionId: session.session_id,
      questionId: session.question?.id || '',
      answerId: answer.id,
    });
    try {
      const next = await answerPulseSeedQuestion(session, answer);
      setSession(next);
      setCurrentQuestionIndex((index) => Math.min(index + 1, 4));
      trace('easy-config-page', 'answer_success', { sessionId: next.session_id, complete: next.complete === true });
    } catch (err) {
      traceError('easy-config-page', 'answer_failed', err, { sessionId: session.session_id, answerId: answer.id });
      setError(err instanceof Error ? err.message : 'Pulse could not process that answer.');
    } finally {
      setSubmitting(false);
    }
  };

  return (
    <div className="h-full flex flex-col p-8 overflow-hidden">
      {/* Header */}
      <div className="flex items-center justify-between mb-6">
        <button
          onClick={onBack}
          className="flex items-center gap-2 px-4 py-2 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg transition-colors"
        >
          <ArrowLeft className="w-4 h-4" />
          Back to Library
        </button>
        <div className="flex items-center gap-2 text-[#8e8f94]">
          <Sparkles className="w-4 h-4 text-[#4ecdc4]" />
          <span className="text-sm">Question {Math.min(currentQuestionIndex + 1, 5)} of 5</span>
        </div>
      </div>

      {/* Main Content */}
      <div className="flex-1 min-h-0 relative flex items-center justify-end gap-12 overflow-visible">
        {/* Pulse Character */}
        <div className="absolute left-[-270px] bottom-[-145px] z-0 w-[860px] h-[980px] overflow-hidden pointer-events-none">
          <ImageWithFallback
            src={pulseImage}
            alt="Pulse Character"
            className="w-full h-full object-cover object-top"
          />
        </div>

        {/* Dialogue and Questions */}
        <div className="relative z-10 w-full max-w-2xl space-y-8">
          {/* Speech Bubble */}
          <div className="relative">
            <div className="bg-gradient-card border-2 border-[#4ecdc4] rounded-2xl p-6 shadow-2xl card-float">
              <div className="flex items-start gap-3">
                <Sparkles className="w-6 h-6 text-[#4ecdc4] flex-shrink-0 mt-1" />
                <p className="text-lg text-white leading-relaxed">
                  {displayedText}
                  {isTyping && <span className="inline-block w-1 h-5 bg-[#4ecdc4] ml-1 animate-pulse"></span>}
                </p>
              </div>
            </div>
            {/* Speech bubble pointer */}
            <div className="absolute left-0 top-1/2 -translate-x-4 -translate-y-1/2 w-0 h-0 border-t-8 border-t-transparent border-b-8 border-b-transparent border-r-8 border-r-[#4ecdc4]"></div>
          </div>

          {/* Answer Options */}
          <div className="space-y-3">
            <div className="text-sm font-bold text-[#8e8f94] mb-3">Choose your answer:</div>
            {session?.complete && !error && (
              <div className="rounded-xl border border-[#4ecdc4]/30 bg-[#10151a]/95 p-4 space-y-3">
                <div>
                  <label className="text-xs uppercase text-[#8e8f94] font-bold">Seed name</label>
                  <input
                    value={seedTitle}
                    onChange={(event) => setSeedTitle(event.target.value)}
                    className="mt-2 w-full rounded-lg border-2 border-[#2a2b30] bg-[#0e0f13] px-4 py-3 text-white outline-none focus:border-[#4ecdc4]"
                    placeholder="Name this seed"
                  />
                </div>
                <p className="text-sm text-[#8e8f94]">
                  Read Pulse's summary before creating the seed. If it does not match the run you want, start over and answer Pulse differently.
                </p>
                {generatedSeedParagraph && (
                  <p className="rounded-lg border border-[#2a2b30] bg-[#0f1015] px-4 py-3 text-sm leading-relaxed text-[#d7f7f4]">
                    {generatedSeedParagraph}
                  </p>
                )}
              </div>
            )}
            {(error ? [{ id: 'retry', label: 'Try again' } satisfies PulseAnswer] : currentAnswers).map((answer, index) => (
              <button
                key={answer.id || index}
                onClick={() => handleAnswerClick(answer)}
                disabled={isTyping || loading || submitting}
                className={`w-full p-4 rounded-lg text-left transition-all ${
                  isTyping || loading || submitting
                    ? 'bg-[#1c1d22] border-2 border-[#2a2b30] text-[#8e8f94] cursor-not-allowed opacity-50'
                    : 'bg-gradient-card border-2 border-[#2a2b30] hover:border-[#4ecdc4] text-white hover:shadow-lg hover:shadow-[#4ecdc4]/20 cursor-pointer'
                }`}
              >
                <div className="flex items-center gap-3">
                  <div className={`w-8 h-8 rounded-full flex items-center justify-center font-bold text-sm ${
                    isTyping ? 'bg-[#2a2b30] text-[#8e8f94]' : 'bg-gradient-to-r from-[#ff6b35] to-[#f38181] text-white'
                  }`}>
                    {index + 1}
                  </div>
                  <span className="flex-1">{submitting && index === 0 ? 'Working...' : answer.label}</span>
                </div>
              </button>
            ))}
          </div>

          {generatedSettingsSummary.length > 0 && (
            <div className="bg-[#15161b]/90 border border-[#4ecdc4]/35 rounded-xl p-4 shadow-xl">
              <div className="text-sm font-bold text-[#4ecdc4] mb-3">Generated settings summary</div>
              <div className="grid grid-cols-1 sm:grid-cols-2 gap-2">
                {generatedSettingsSummary.map(([key, value]) => (
                  <div key={key} className="rounded-lg bg-[#0f1015] border border-[#2a2b30] px-3 py-2">
                    <div className="text-[11px] uppercase tracking-wide text-[#8e8f94]">{settingLabel(key)}</div>
                    <div className="text-sm text-white break-words">{summarizeValue(value)}</div>
                  </div>
                ))}
              </div>
            </div>
          )}

          {/* Progress Indicator */}
          <div className="flex gap-2 justify-center mt-6">
            {[0, 1, 2, 3, 4].map((index) => (
              <div
                key={index}
                className={`h-2 rounded-full transition-all ${
                  index === currentQuestionIndex
                    ? 'w-8 bg-gradient-to-r from-[#4ecdc4] to-[#95e1d3]'
                    : index < currentQuestionIndex
                    ? 'w-2 bg-[#4ecdc4]'
                    : 'w-2 bg-[#2a2b30]'
                }`}
              ></div>
            ))}
          </div>
        </div>
      </div>
      {submitting && (
        <LoadingModal
          title="Pulse is thinking..."
          message={session?.complete ? 'Pulse is creating your Sync configuration on Nexus.' : 'Pulse is turning your answer into the next seed decision.'}
        />
      )}
      {error && (
        <ErrorModal
          title="Pulse could not continue"
          message={error}
          code="PULSE_CONFIG_FAILED"
          onClose={() => {
            const shouldRestart = !session;
            setError('');
            if (shouldRestart) void startSession();
          }}
        />
      )}
    </div>
  );
}
