import React, { useEffect, useState } from "react";
import { adminApi } from "@/services/adminApi";

type LogEntry = {
  name: string;
  size: string;
  updated_at: string;
};

const LogsPage: React.FC = () => {
  const [logs, setLogs] = useState<LogEntry[]>([]);
  const [content, setContent] = useState("");
  const [error, setError] = useState("");

  const load = async () => {
    try {
      const data = await adminApi.getLogs();
      setLogs((data.logs || []) as LogEntry[]);
      setError("");
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err || "logs_failed"));
    }
  };

  useEffect(() => {
    void load();
  }, []);

  return (
    <section className="page-section" aria-label="Server logs">
      <header className="panel-header">
        <h2>Logs</h2>
        <div className="actions-row">
          <button className="btn ghost" type="button" onClick={() => void load()}>Refresh</button>
        </div>
      </header>
      {error && <p className="error-text">{error}</p>}
      <div className="table-wrap">
        <table className="table">
          <thead>
            <tr>
              <th>Name</th>
              <th>Size</th>
              <th>Updated</th>
              <th>Action</th>
            </tr>
          </thead>
          <tbody>
            {logs.map((log) => (
              <tr key={log.name}>
                <td>{log.name}</td>
                <td>{log.size}</td>
                <td>{new Date(log.updated_at).toLocaleString()}</td>
                <td>
                  <button className="btn ghost" type="button" onClick={async () => {
                    const data = await adminApi.getLogContent(log.name, 350);
                    setContent(data.content || "");
                  }}>View</button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
      <div className="panel-block">
        <h3>Log content</h3>
        <pre className="log-viewer">{content || "Select a log to inspect."}</pre>
      </div>
    </section>
  );
};

export default LogsPage;
