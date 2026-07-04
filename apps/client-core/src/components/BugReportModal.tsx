import React from "react";

type BugReportPayload = {
  title: string;
  description: string;
};

interface BugReportModalProps {
  open: boolean;
  submitting?: boolean;
  onClose: () => void;
  onSubmit: (payload: BugReportPayload) => Promise<void> | void;
}

const BugReportModal: React.FC<BugReportModalProps> = ({ open, submitting = false, onClose, onSubmit }) => {
  const [title, setTitle] = React.useState("");
  const [description, setDescription] = React.useState("");
  const [error, setError] = React.useState("");

  React.useEffect(() => {
    if (!open) {
      setTitle("");
      setDescription("");
      setError("");
    }
  }, [open]);

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
    setError("");
    await onSubmit({ title: nextTitle, description: nextDescription });
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
