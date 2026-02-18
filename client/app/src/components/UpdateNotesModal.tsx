import React, { useMemo } from "react";
import { useI18n } from "../i18n";

interface UpdateNotesModalProps {
  open: boolean;
  title?: string;
  markdown: string;
  onClose: () => void;
}

const escapeHtml = (value: string) => {
  return String(value || "")
    .replace(/&/g, "&amp;")
    .replace(/</g, "&lt;")
    .replace(/>/g, "&gt;")
    .replace(/"/g, "&quot;")
    .replace(/'/g, "&#39;");
};

const isSafeUrl = (value: string) => {
  const raw = String(value || "").trim();
  if (!raw) return false;
  if (raw.startsWith("/") || raw.startsWith("#")) return true;
  return /^https?:\/\//i.test(raw) || /^mailto:/i.test(raw);
};

const renderInline = (input: string) => {
  let text = escapeHtml(input);
  text = text.replace(/\[([^\]]+)\]\(([^)]+)\)/g, (_m, label, href) => {
    const safeHref = String(href || "").trim();
    if (!isSafeUrl(safeHref)) return escapeHtml(label);
    return `<a href="${escapeHtml(safeHref)}" target="_blank" rel="noopener noreferrer">${escapeHtml(label)}</a>`;
  });
  text = text.replace(/`([^`]+)`/g, "<code>$1</code>");
  text = text.replace(/\*\*([^*]+)\*\*/g, "<strong>$1</strong>");
  text = text.replace(/\*([^*]+)\*/g, "<em>$1</em>");
  text = text.replace(/~~([^~]+)~~/g, "<del>$1</del>");
  return text;
};

const renderMarkdown = (source: string) => {
  const lines = String(source || "").replace(/\r\n/g, "\n").split("\n");
  const html: string[] = [];
  let i = 0;

  while (i < lines.length) {
    const line = lines[i];

    const fenceMatch = line.match(/^```/);
    if (fenceMatch) {
      i += 1;
      const codeLines: string[] = [];
      while (i < lines.length && !/^```/.test(lines[i])) {
        codeLines.push(lines[i]);
        i += 1;
      }
      if (i < lines.length && /^```/.test(lines[i])) i += 1;
      html.push(`<pre><code>${escapeHtml(codeLines.join("\n"))}</code></pre>`);
      continue;
    }

    const headingMatch = line.match(/^(#{1,6})\s+(.+)$/);
    if (headingMatch) {
      const level = Math.min(6, headingMatch[1].length);
      html.push(`<h${level}>${renderInline(headingMatch[2])}</h${level}>`);
      i += 1;
      continue;
    }

    if (/^\s*[-*+]\s+/.test(line)) {
      const items: string[] = [];
      while (i < lines.length && /^\s*[-*+]\s+/.test(lines[i])) {
        items.push(lines[i].replace(/^\s*[-*+]\s+/, ""));
        i += 1;
      }
      html.push(`<ul>${items.map((item) => `<li>${renderInline(item)}</li>`).join("")}</ul>`);
      continue;
    }

    if (/^\s*\d+\.\s+/.test(line)) {
      const items: string[] = [];
      while (i < lines.length && /^\s*\d+\.\s+/.test(lines[i])) {
        items.push(lines[i].replace(/^\s*\d+\.\s+/, ""));
        i += 1;
      }
      html.push(`<ol>${items.map((item) => `<li>${renderInline(item)}</li>`).join("")}</ol>`);
      continue;
    }

    if (/^\s*>\s?/.test(line)) {
      const quoteLines: string[] = [];
      while (i < lines.length && /^\s*>\s?/.test(lines[i])) {
        quoteLines.push(lines[i].replace(/^\s*>\s?/, ""));
        i += 1;
      }
      html.push(`<blockquote><p>${renderInline(quoteLines.join(" "))}</p></blockquote>`);
      continue;
    }

    if (!line.trim()) {
      i += 1;
      continue;
    }

    const paragraphLines: string[] = [];
    while (i < lines.length) {
      const cur = lines[i];
      if (!cur.trim()) break;
      if (/^```/.test(cur) || /^(#{1,6})\s+/.test(cur) || /^\s*[-*+]\s+/.test(cur) || /^\s*\d+\.\s+/.test(cur) || /^\s*>\s?/.test(cur)) break;
      paragraphLines.push(cur);
      i += 1;
    }
    html.push(`<p>${renderInline(paragraphLines.join(" "))}</p>`);
  }

  return html.join("\n");
};

const UpdateNotesModal: React.FC<UpdateNotesModalProps> = ({ open, title, markdown, onClose }) => {
  const { t } = useI18n();
  const rendered = useMemo(() => renderMarkdown(markdown), [markdown]);

  if (!open) return null;

  return (
    <div className="skl-update-notes-overlay" role="dialog" aria-modal="true" aria-label={title || t("auth.update_notes")}>
      <div className="skl-update-notes-card">
        <div className="skl-update-notes-header">
          <h2>{title || t("auth.what_changed")}</h2>
          <button className="skl-btn ghost" type="button" onClick={onClose}>
            {t("common.close")}
          </button>
        </div>
        <div className="skl-update-notes-body markdown" dangerouslySetInnerHTML={{ __html: rendered }} />
      </div>
    </div>
  );
};

export default UpdateNotesModal;
