import React from "react";
import { Camera, X } from "lucide-react";
import { runtime } from "../services/runtime";

type BugReportPayload = {
  title: string;
  description: string;
  screenshotBase64?: string;
};

interface BugReportModalProps {
  open: boolean;
  submitting?: boolean;
  initialTitle?: string;
  initialDescription?: string;
  onClose: () => void;
  onSubmit: (payload: BugReportPayload) => Promise<void> | void;
}

const BugReportModal: React.FC<BugReportModalProps> = ({ open, submitting = false, initialTitle = "", initialDescription = "", onClose, onSubmit }) => {
  const [title, setTitle] = React.useState("");
  const [description, setDescription] = React.useState("");
  const [error, setError] = React.useState("");
  const [captureSources, setCaptureSources] = React.useState<Array<{ id: string; name: string; previewBase64: string }>>([]);
  const [screenshotBase64, setScreenshotBase64] = React.useState("");
  const [consent, setConsent] = React.useState(false);

  React.useEffect(() => {
    if (open) {
      setTitle(initialTitle);
      setDescription(initialDescription);
    } else {
      setTitle("");
      setDescription("");
      setError("");
      setCaptureSources([]);
      setScreenshotBase64("");
      setConsent(false);
    }
  }, [open, initialTitle, initialDescription]);

  const chooseScreenshot = async () => {
    const result = await runtime.listCaptureSources?.();
    if (!result?.ok) {
      setError(result?.error || "Unable to list screens.");
      return;
    }
    setCaptureSources(result.sources || []);
  };

  const capture = async (sourceId: string) => {
    const result = await runtime.captureSource?.(sourceId);
    if (!result?.ok) {
      setError(result?.error || "Unable to capture screen.");
      return;
    }
    setScreenshotBase64(result.screenshotBase64 || "");
    setCaptureSources([]);
  };

  const handleSubmit = async () => {
    const nextTitle = title.trim();
    const nextDescription = description.trim();
    if (!nextTitle || nextTitle.length > 100) {
      setError("Title must be between 1 and 100 characters.");
      return;
    }
    if (!nextDescription || nextDescription.length > 200) {
      setError("Description must be between 1 and 200 characters.");
      return;
    }
    if (!consent) {
      setError("Confirm diagnostic data collection before sending.");
      return;
    }
    setError("");
    await onSubmit({ title: nextTitle, description: nextDescription, screenshotBase64 });
  };

  return (
    <div className={`skl-modal${open ? " open" : ""}`} id="skl-bug-report-modal" aria-hidden={!open}>
      <div className="skl-modal-backdrop" onClick={submitting ? undefined : onClose}></div>
      <div className="skl-modal-panel skl-bug-report-panel" role="dialog" aria-modal="true" aria-labelledby="skl-bug-report-title">
        <div className="skl-modal-header">
          <h3 id="skl-bug-report-title">Report a bug</h3>
          <button className="skl-btn ghost" type="button" onClick={onClose} disabled={submitting}>Close</button>
        </div>
        <div className="skl-modal-body skl-bug-report-body">
          <label className="skl-bug-report-field">
            <span>Title</span>
            <input
              className="input"
              type="text"
              maxLength={100}
              value={title}
              onChange={(e) => setTitle(e.target.value)}
              placeholder="Short summary of the issue"
              disabled={submitting}
            />
            <small>{title.length}/100</small>
          </label>
          <div className="skl-bug-report-field">
            <span>Screenshot (optional)</span>
            {screenshotBase64 ? (
              <div style={{ position: "relative" }}>
                <img src={`data:image/png;base64,${screenshotBase64}`} alt="Selected screenshot" style={{ width: "100%", maxHeight: 220, objectFit: "contain", border: "1px solid #2a2b30" }} />
                <button type="button" className="skl-btn ghost" onClick={() => setScreenshotBase64("")} disabled={submitting}><X size={14} /> Remove</button>
              </div>
            ) : (
              <button className="skl-btn ghost" type="button" onClick={chooseScreenshot} disabled={submitting}><Camera size={16} /> Screenshot</button>
            )}
            {captureSources.length > 0 && (
              <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fit,minmax(180px,1fr))", gap: 10 }}>
                {captureSources.map((source) => (
                  <button key={source.id} type="button" onClick={() => capture(source.id)} className="skl-btn ghost" style={{ display: "block" }}>
                    <img src={`data:image/png;base64,${source.previewBase64}`} alt={source.name} style={{ width: "100%" }} />
                    <span>{source.name}</span>
                  </button>
                ))}
              </div>
            )}
          </div>
          <label className="skl-bug-report-field" style={{ display: "flex", flexDirection: "row", alignItems: "flex-start", gap: 10 }}>
            <input type="checkbox" checked={consent} onChange={(event) => setConsent(event.target.checked)} disabled={submitting} />
            <span>Include redacted session diagnostics and retain the private report for up to 30 days. Screenshot is included only when selected.</span>
          </label>
          <label className="skl-bug-report-field">
            <span>Description</span>
            <textarea
              className="input skl-bug-report-textarea"
              maxLength={200}
              value={description}
              onChange={(e) => setDescription(e.target.value)}
              placeholder="What happened?"
              disabled={submitting}
            />
            <small>{description.length}/200</small>
          </label>
          {error ? <div className="skl-bug-report-error">{error}</div> : null}
          <div className="skl-modal-actions">
            <button className="skl-btn ghost" type="button" onClick={onClose} disabled={submitting}>Cancel</button>
            <button className="skl-btn" type="button" onClick={handleSubmit} disabled={submitting}>
              {submitting ? "Sending..." : "Send bug report"}
            </button>
          </div>
        </div>
      </div>
    </div>
  );
};

export default BugReportModal;
