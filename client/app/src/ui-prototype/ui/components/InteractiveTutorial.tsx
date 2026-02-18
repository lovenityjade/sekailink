import React, { useEffect, useMemo, useState } from "react";
import { useNavigate } from "react-router-dom";
import { useI18n } from "@/i18n";

type TutorialAction = {
  label: string;
  to: string;
};

type TutorialStep = {
  title: string;
  body: string;
  selector?: string;
  strict?: boolean;
  action?: TutorialAction;
};

interface InteractiveTutorialProps {
  open: boolean;
  onSkip: () => void;
  onComplete: () => void;
  busy?: boolean;
}

const InteractiveTutorial: React.FC<InteractiveTutorialProps> = ({ open, onSkip, onComplete, busy = false }) => {
  const { t } = useI18n();
  const navigate = useNavigate();
  const [stepIndex, setStepIndex] = useState(0);
  const [targetRect, setTargetRect] = useState<DOMRect | null>(null);

  useEffect(() => {
    if (open) setStepIndex(0);
  }, [open]);

  const steps = useMemo<TutorialStep[]>(
    () => [
      {
        title: t("tutorial.step1.title"),
        body: t("tutorial.step1.body"),
        selector: '[data-tutorial="home-button"]',
        strict: true,
      },
      {
        title: t("tutorial.step2.title"),
        body: t("tutorial.step2.body"),
        selector: '[data-tutorial="room-list-panel"]',
        strict: true,
        action: { label: t("tutorial.step2.action"), to: "/" },
      },
      {
        title: t("tutorial.step3.title"),
        body: t("tutorial.step3.body"),
        selector: '[data-tutorial="gm-tabs"]',
        strict: true,
        action: { label: t("tutorial.step3.action"), to: "/dashboard/yaml/new?tab=add" },
      },
      {
        title: t("tutorial.step4.title"),
        body: t("tutorial.step4.body"),
        selector: '[data-tutorial="settings-tabs"]',
        strict: true,
        action: { label: t("tutorial.step4.action"), to: "/settings" },
      },
      {
        title: t("tutorial.step5.title"),
        body: t("tutorial.step5.body"),
        selector: '[data-tutorial="room-join"]',
        strict: true,
      },
    ],
    [t]
  );

  const current = steps[Math.min(stepIndex, steps.length - 1)];
  const last = stepIndex >= steps.length - 1;
  const strictActive = Boolean(current.strict && targetRect);

  useEffect(() => {
    if (!open) return;
    const selector = current.selector;
    if (!selector) {
      setTargetRect(null);
      return;
    }

    let raf = 0;

    const updateRect = () => {
      const el = document.querySelector(selector) as HTMLElement | null;
      if (!el) {
        setTargetRect(null);
        return;
      }
      el.scrollIntoView({ block: "center", inline: "nearest", behavior: "smooth" });
      setTargetRect(el.getBoundingClientRect());
    };

    const schedule = () => {
      window.cancelAnimationFrame(raf);
      raf = window.requestAnimationFrame(updateRect);
    };

    updateRect();
    window.addEventListener("resize", schedule);
    window.addEventListener("scroll", schedule, true);
    const interval = window.setInterval(updateRect, 600);

    return () => {
      window.cancelAnimationFrame(raf);
      window.removeEventListener("resize", schedule);
      window.removeEventListener("scroll", schedule, true);
      window.clearInterval(interval);
    };
  }, [open, current.selector, stepIndex]);

  if (!open) return null;

  const goPrev = () => setStepIndex((prev) => Math.max(0, prev - 1));
  const goNext = () => {
    if (last) {
      onComplete();
      return;
    }
    setStepIndex((prev) => Math.min(steps.length - 1, prev + 1));
  };

  return (
    <div className="sklp-tutorial-overlay" role="dialog" aria-modal="true" aria-labelledby="sklp-tutorial-title">
      {targetRect ? (
        <>
          <div className={`sklp-tutorial-mask top${strictActive ? " strict" : ""}`} style={{ height: Math.max(0, targetRect.top - 8) }} />
          <div
            className={`sklp-tutorial-mask left${strictActive ? " strict" : ""}`}
            style={{ top: Math.max(0, targetRect.top - 8), height: targetRect.height + 16, width: Math.max(0, targetRect.left - 8) }}
          />
          <div
            className={`sklp-tutorial-mask right${strictActive ? " strict" : ""}`}
            style={{ top: Math.max(0, targetRect.top - 8), height: targetRect.height + 16, left: targetRect.right + 8 }}
          />
          <div className={`sklp-tutorial-mask bottom${strictActive ? " strict" : ""}`} style={{ top: targetRect.bottom + 8 }} />
          <div
            className="sklp-tutorial-focus"
            style={{
              top: Math.max(0, targetRect.top - 8),
              left: Math.max(0, targetRect.left - 8),
              width: targetRect.width + 16,
              height: targetRect.height + 16,
            }}
          />
        </>
      ) : null}

      <div className="sklp-tutorial-modal">
        <header className="sklp-tutorial-header">
          <h3 id="sklp-tutorial-title">{t("tutorial.title")}</h3>
          <div className="sklp-tutorial-step">{t("tutorial.step_counter", { current: stepIndex + 1, total: steps.length })}</div>
        </header>

        <div className="sklp-tutorial-body">
          <h4>{current.title}</h4>
          <p>{current.body}</p>
          {strictActive ? <p className="sklp-tutorial-strict-note">{t("tutorial.strict_hint")}</p> : null}
          {current.action ? (
            <button type="button" className="skl-btn ghost" onClick={() => navigate(current.action!.to)} disabled={busy}>
              {current.action.label}
            </button>
          ) : null}
        </div>

        <footer className="sklp-tutorial-actions">
          <button type="button" className="skl-btn" onClick={onSkip} disabled={busy}>
            {t("tutorial.skip")}
          </button>
          <div className="sklp-tutorial-actions-right">
            <button type="button" className="skl-btn ghost" onClick={goPrev} disabled={busy || stepIndex <= 0}>
              {t("tutorial.back")}
            </button>
            <button type="button" className="skl-btn primary" onClick={goNext} disabled={busy}>
              {last ? t("tutorial.finish") : t("tutorial.next")}
            </button>
          </div>
        </footer>
      </div>
    </div>
  );
};

export default InteractiveTutorial;
