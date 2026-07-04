import { AlertTriangle, Bug, Send, Sparkles, ThumbsDown, ThumbsUp } from 'lucide-react';
import { useState } from 'react';
import { apiFetch } from '../../services/api';
import { trace, traceError } from '../../services/trace';
import pulseImage from '../../imports/pulse.png';
import { ImageWithFallback } from './figma/ImageWithFallback';

type PulseMessage = {
  id: string;
  role: 'pulse' | 'user';
  content: string;
};

const firstPulseMessage =
  "Je parle plusieurs langues et je suis encore en entrainement alors je pourrais faire des erreurs.";

const PULSE_PUBLIC_ASK_URL = 'https://pulse.sekailink.com/api/public-ask';

export default function PulsePage() {
  const [messages, setMessages] = useState<PulseMessage[]>([
    { id: 'welcome', role: 'pulse', content: firstPulseMessage },
  ]);
  const [draft, setDraft] = useState('');
  const [sending, setSending] = useState(false);
  const [lastError, setLastError] = useState('');

  const submit = async () => {
    const prompt = draft.trim();
    if (!prompt || sending) return;
    const userMessage: PulseMessage = { id: `user-${Date.now()}`, role: 'user', content: prompt };
    setMessages((prev) => [...prev, userMessage]);
    setDraft('');
    setSending(true);
    setLastError('');
    trace('pulse-page', 'prompt_send_start', { length: prompt.length });
    try {
      const response = await fetch(PULSE_PUBLIC_ASK_URL, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          message: prompt,
          scope: 'apworld',
          mode: 'normal',
          surface: 'client-core-pulse-page',
        }),
      });
      const data = await response.json().catch(() => ({}));
      if (!response.ok) throw new Error(String((data as any)?.error || 'Pulse is not available right now.'));
      const answer = String((data as any)?.answer || (data as any)?.message || '').trim();
      setMessages((prev) => [
        ...prev,
        {
          id: `pulse-${Date.now()}`,
          role: 'pulse',
          content: answer || "Je n'ai pas encore de réponse fiable pour ça.",
        },
      ]);
      trace('pulse-page', 'prompt_send_success');
    } catch (error) {
      traceError('pulse-page', 'prompt_send_failed', error);
      const message = error instanceof Error ? error.message : 'Pulse is not available right now.';
      setLastError(message);
      setMessages((prev) => [
        ...prev,
        {
          id: `pulse-error-${Date.now()}`,
          role: 'pulse',
          content: "Je n'arrive pas à joindre mon module de réponse. Réessaie dans un instant.",
        },
      ]);
    } finally {
      setSending(false);
    }
  };

  const sendFeedback = async (message: PulseMessage, rating: 'positive' | 'negative' | 'bug') => {
    trace('pulse-page', 'feedback_start', { rating, messageId: message.id });
    try {
      await apiFetch('/api/pulse/feedback', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({
          message_id: message.id,
          rating,
          content: message.content,
          surface: 'client-core-pulse-page',
        }),
      });
    } catch (error) {
      traceError('pulse-page', 'feedback_failed', error, { rating, messageId: message.id });
    }
  };

  return (
    <div className="h-full p-8 flex flex-col gap-6">
      <div className="flex items-center justify-between">
        <div>
          <h1 className="text-3xl font-bold mb-1">Pulse</h1>
          <p className="text-[#8e8f94]">Questions sur Archipelago, les randomizers, les settings et SekaiLink.</p>
        </div>
        <div className="w-12 h-12 rounded-xl bg-gradient-to-br from-[#aa96da] to-[#ff6b35] flex items-center justify-center shadow-lg">
          <Sparkles className="w-6 h-6" />
        </div>
      </div>

      <div className="flex items-start gap-3 rounded-lg border border-[#aa96da]/40 bg-[#aa96da]/10 p-4 text-sm text-[#d9d2ff]">
        <AlertTriangle className="w-5 h-5 flex-shrink-0 text-[#aa96da]" />
        <span>Pulse est encore en entrainement. Vérifie les réponses critiques avant une race ou une génération publique.</span>
      </div>

      <div className="flex-1 min-h-0 rounded-xl border border-[#2a2b30] bg-[#0e0f13]/90 overflow-hidden grid grid-cols-[220px_1fr]">
        <div className="relative border-r border-[#2a2b30] bg-gradient-to-b from-[#aa96da]/20 via-[#14151a] to-[#0e0f13] overflow-hidden">
          <ImageWithFallback
            src={pulseImage}
            alt="Pulse"
            className="absolute left-1/2 top-6 h-[620px] max-w-none -translate-x-1/2 object-contain"
          />
        </div>
        <div className="min-w-0 min-h-0 flex flex-col">
        <div className="flex-1 overflow-auto p-5 space-y-4">
          {messages.map((message) => (
            <div key={message.id} className={`flex ${message.role === 'user' ? 'justify-end' : 'justify-start'}`}>
              <div className={`max-w-[76%] ${message.role === 'user' ? 'text-right' : 'text-left'}`}>
                <div
                  className={`rounded-2xl px-4 py-3 text-sm leading-relaxed ${
                    message.role === 'user'
                      ? 'bg-gradient-to-r from-[#ff6b35] to-[#f38181] rounded-br-sm'
                      : 'bg-[#1c1d22] border border-[#2a2b30] rounded-bl-sm'
                  }`}
                >
                  {message.content}
                </div>
                {message.role === 'pulse' && message.id !== 'welcome' && (
                  <div className="mt-2 flex items-center gap-2">
                    <button
                      onClick={() => void sendFeedback(message, 'positive')}
                      className="w-8 h-8 rounded-lg bg-[#2a2b30] hover:bg-[#4ecdc4]/20 hover:text-[#4ecdc4] transition-colors flex items-center justify-center"
                      aria-label="Good Pulse answer"
                    >
                      <ThumbsUp className="w-4 h-4" />
                    </button>
                    <button
                      onClick={() => void sendFeedback(message, 'negative')}
                      className="w-8 h-8 rounded-lg bg-[#2a2b30] hover:bg-[#f69d50]/20 hover:text-[#f69d50] transition-colors flex items-center justify-center"
                      aria-label="Bad Pulse answer"
                    >
                      <ThumbsDown className="w-4 h-4" />
                    </button>
                    <button
                      onClick={() => void sendFeedback(message, 'bug')}
                      className="w-8 h-8 rounded-lg bg-[#2a2b30] hover:bg-[#f85149]/20 hover:text-[#f85149] transition-colors flex items-center justify-center"
                      aria-label="Report Pulse bug"
                    >
                      <Bug className="w-4 h-4" />
                    </button>
                  </div>
                )}
              </div>
            </div>
          ))}
          {sending && <div className="text-sm text-[#8e8f94]">Pulse réfléchit...</div>}
        </div>

        {lastError && (
          <div className="mx-5 mb-3 rounded-lg border border-[#f85149]/40 bg-[#f85149]/10 px-3 py-2 text-xs text-[#ffb4aa]">
            {lastError}
          </div>
        )}

        <div className="border-t border-[#2a2b30] bg-[#14151a] p-4">
          <div className="flex gap-3">
            <textarea
              value={draft}
              onChange={(event) => setDraft(event.target.value)}
              onKeyDown={(event) => {
                if (event.key === 'Enter' && !event.shiftKey) {
                  event.preventDefault();
                  void submit();
                }
              }}
              rows={2}
              placeholder="Pose une question à Pulse..."
              className="flex-1 resize-none rounded-lg border-2 border-[#2a2b30] bg-[#1c1d22] px-4 py-3 text-white outline-none transition-colors placeholder:text-[#8e8f94] focus:border-[#aa96da]"
            />
            <button
              onClick={() => void submit()}
              disabled={sending || !draft.trim()}
              className="w-14 rounded-lg bg-gradient-to-r from-[#aa96da] to-[#f38181] flex items-center justify-center shadow-lg transition-all disabled:opacity-50"
              aria-label="Send Pulse prompt"
            >
              <Send className="w-5 h-5" />
            </button>
          </div>
        </div>
        </div>
      </div>
    </div>
  );
}
