import React, { useEffect, useMemo, useState } from "react";
import { adminApi, type SystemHealth } from "@/services/adminApi";

const formatBytes = (value: number) => {
  if (!Number.isFinite(value) || value <= 0) return "0 B";
  const units = ["B", "KB", "MB", "GB", "TB"];
  let n = value;
  let i = 0;
  while (n >= 1024 && i < units.length - 1) {
    n /= 1024;
    i += 1;
  }
  return `${n.toFixed(i === 0 ? 0 : 1)} ${units[i]}`;
};

const DashboardPage: React.FC = () => {
  const [health, setHealth] = useState<SystemHealth | null>(null);
  const [journal, setJournal] = useState("");
  const [journalService, setJournalService] = useState<"webhost" | "workers">("webhost");
  const [statusText, setStatusText] = useState("");
  const [errorText, setErrorText] = useState("");

  const loadHealth = async () => {
    try {
      const data = await adminApi.getSystemHealth();
      setHealth(data);
      setErrorText("");
    } catch (err) {
      setErrorText(err instanceof Error ? err.message : String(err || "health_failed"));
    }
  };

  const loadJournal = async (service: "webhost" | "workers") => {
    try {
      setJournalService(service);
      const data = await adminApi.getJournal(service, 220);
      setJournal(data.content || "");
      setErrorText("");
    } catch (err) {
      setErrorText(err instanceof Error ? err.message : String(err || "journal_failed"));
    }
  };

  const restartService = async (service: "webhost" | "workers") => {
    try {
      setStatusText(`Restarting ${service}...`);
      await adminApi.restartService(service);
      setStatusText(`Service ${service} restarted.`);
      await loadHealth();
      await loadJournal(service);
    } catch (err) {
      setErrorText(err instanceof Error ? err.message : String(err || "restart_failed"));
      setStatusText("");
    }
  };

  const requestReboot = async () => {
    const reason = window.prompt("Reboot reason (required for audit log):", "planned maintenance") || "";
    if (!reason.trim()) return;
    const ok = window.confirm("Confirm VPS reboot now? This disconnects all active sessions.");
    if (!ok) return;
    try {
      setStatusText("Scheduling reboot...");
      await adminApi.rebootHost(reason.trim(), "REBOOT");
      setStatusText("Reboot command accepted.");
    } catch (err) {
      setErrorText(err instanceof Error ? err.message : String(err || "reboot_failed"));
    }
  };

  useEffect(() => {
    void loadHealth();
    void loadJournal("webhost");
    const timer = window.setInterval(() => {
      void loadHealth();
    }, 12000);
    return () => window.clearInterval(timer);
  }, []);

  const servicesJson = useMemo(() => JSON.stringify(health?.services || {}), [health?.services]);

  return (
    <section className="page-section" aria-label="Operations dashboard">
      <header className="panel-header">
        <h2>Operations Dashboard</h2>
        <div className="actions-row">
          <button className="btn ghost" type="button" onClick={() => void loadHealth()}>Refresh</button>
          <button className="btn ghost" type="button" onClick={() => void loadJournal(journalService)}>Reload journal</button>
        </div>
      </header>

      {statusText && <p className="status-text">{statusText}</p>}
      {errorText && <p className="error-text">{errorText}</p>}

      <div className="metric-grid">
        <article className="metric-card">
          <h3>Host</h3>
          <p>{health?.host || "-"}</p>
          <small>App {health?.app_version || "-"}</small>
        </article>
        <article className="metric-card">
          <h3>CPU</h3>
          <p>{health?.metrics?.cpu_count ?? "-"} cores</p>
          <small>load: {(health?.metrics?.loadavg || []).join(" / ") || "-"}</small>
        </article>
        <article className="metric-card">
          <h3>Memory</h3>
          <p>{formatBytes(health?.metrics?.mem_available_bytes || 0)} free</p>
          <small>total {formatBytes(health?.metrics?.mem_total_bytes || 0)}</small>
        </article>
        <article className="metric-card">
          <h3>Disk</h3>
          <p>{formatBytes(health?.metrics?.disk_free_bytes || 0)} free</p>
          <small>total {formatBytes(health?.metrics?.disk_total_bytes || 0)}</small>
        </article>
      </div>

      <div className="panel-block">
        <h3>Service Health (Vue widget)</h3>
        <service-health-widget snapshot={servicesJson}></service-health-widget>
        <div className="actions-row">
          <button className="btn" type="button" onClick={() => void restartService("webhost")}>Restart WebHost</button>
          <button className="btn" type="button" onClick={() => void restartService("workers")}>Restart Workers</button>
          <button className="btn danger" type="button" onClick={() => void requestReboot()}>Reboot VPS</button>
        </div>
      </div>

      <div className="panel-block">
        <h3>System Journal ({journalService})</h3>
        <div className="actions-row">
          <button className="btn ghost" type="button" onClick={() => void loadJournal("webhost")}>WebHost logs</button>
          <button className="btn ghost" type="button" onClick={() => void loadJournal("workers")}>Worker logs</button>
        </div>
        <pre className="log-viewer" aria-live="polite">{journal || "No journal output."}</pre>
      </div>
    </section>
  );
};

export default DashboardPage;
