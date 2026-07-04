import type React from 'react';

export type LobbyTextLink = {
  id: string;
  name: string;
};

export const STANDARD_EMOTES = [
  { code: ':)', emoji: '🙂', label: 'Smile' },
  { code: ':-)', emoji: '🙂', label: 'Smile' },
  { code: ':(', emoji: '☹️', label: 'Sad' },
  { code: ':-(', emoji: '☹️', label: 'Sad' },
  { code: ':D', emoji: '😄', label: 'Big smile' },
  { code: ':-D', emoji: '😄', label: 'Big smile' },
  { code: ';)', emoji: '😉', label: 'Wink' },
  { code: ';-)', emoji: '😉', label: 'Wink' },
  { code: ':P', emoji: '😛', label: 'Tongue' },
  { code: ':-P', emoji: '😛', label: 'Tongue' },
  { code: ':O', emoji: '😮', label: 'Surprised' },
  { code: ':-O', emoji: '😮', label: 'Surprised' },
  { code: ':|', emoji: '😐', label: 'Neutral' },
  { code: '<3', emoji: '❤️', label: 'Heart' },
  { code: ':fire:', emoji: '🔥', label: 'Fire' },
  { code: ':star:', emoji: '⭐', label: 'Star' },
  { code: ':check:', emoji: '✅', label: 'Check' },
  { code: ':x:', emoji: '❌', label: 'Cross' },
];

const EMOTE_BY_CODE = new Map(STANDARD_EMOTES.map((entry) => [entry.code, entry]));
const EMOTE_PATTERN = /(:-\)|:\)|:-\(|:\(|:-D|:D|;-\)|;\)|:-P|:P|:-O|:O|:\||<3|:fire:|:star:|:check:|:x:)/g;
const TOKEN_PATTERN = /(\[[^\]\n]{1,120}\]|:-\)|:\)|:-\(|:\(|:-D|:D|;-\)|;\)|:-P|:P|:-O|:O|:\||<3|:fire:|:star:|:check:|:x:)/g;

export const appendEmote = (value: string, code: string) => {
  const separator = value && !/\s$/.test(value) ? ' ' : '';
  return `${value}${separator}${code} `;
};

const findLobbyLink = (token: string, lobbyLinks: LobbyTextLink[]) => {
  if (!token.startsWith('[') || !token.endsWith(']')) return null;
  const label = token.slice(1, -1).trim().toLowerCase();
  if (!label) return null;
  return lobbyLinks.find((lobby) => lobby.name.toLowerCase() === label || lobby.id.toLowerCase() === label) || null;
};

export function EmoteText({
  text,
  className = '',
  lobbyLinks = [],
  onLobbyLinkClick,
}: {
  text: string;
  className?: string;
  lobbyLinks?: LobbyTextLink[];
  onLobbyLinkClick?: (lobby: LobbyTextLink) => void;
}) {
  const source = String(text || '');
  const parts: React.ReactNode[] = [];
  let lastIndex = 0;
  source.replace(TOKEN_PATTERN, (match, _token, offset) => {
    if (offset > lastIndex) parts.push(source.slice(lastIndex, offset));
    const emote = EMOTE_BY_CODE.get(match);
    const lobbyLink = findLobbyLink(match, lobbyLinks);
    if (lobbyLink && onLobbyLinkClick) {
      parts.push(
        <button
          key={`${match}-${offset}`}
          type="button"
          onClick={() => onLobbyLinkClick(lobbyLink)}
          className="inline font-semibold text-[#e9fffb] underline decoration-[#4ecdc4]/70 decoration-1 underline-offset-2 drop-shadow-[0_0_5px_rgba(78,205,196,0.75)] transition-colors hover:text-white hover:decoration-white"
          title={`Open lobby ${lobbyLink.name}`}
        >
          [{lobbyLink.name}]
        </button>
      );
    } else if (emote) {
      parts.push(
        <span key={`${match}-${offset}`} className="inline-block translate-y-[1px] text-base leading-none" title={emote.label} aria-label={emote.label}>
          {emote.emoji}
        </span>
      );
    } else {
      parts.push(match);
    }
    lastIndex = offset + match.length;
    return match;
  });
  if (lastIndex < source.length) parts.push(source.slice(lastIndex));
  return <span className={className}>{parts}</span>;
}
