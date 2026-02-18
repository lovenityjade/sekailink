import React, { useEffect, useState } from "react";
import { adminApi } from "@/services/adminApi";

type Ticket = {
  id: number;
  user: string;
  user_id: string;
  status: string;
  category: string;
  subject: string;
  message: string;
  created_at: string;
};

const SupportPage: React.FC = () => {
  const [tickets, setTickets] = useState<Ticket[]>([]);
  const [category, setCategory] = useState("");
  const [error, setError] = useState("");

  const load = async (cat = "") => {
    try {
      const data = await adminApi.getSupport(cat);
      setTickets((data.tickets || []) as Ticket[]);
      setError("");
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err || "support_failed"));
    }
  };

  useEffect(() => {
    void load();
  }, []);

  return (
    <section className="page-section" aria-label="Support tickets">
      <header className="panel-header">
        <h2>Support</h2>
        <div className="actions-row">
          <select className="input" value={category} onChange={(e) => setCategory(e.target.value)} aria-label="Support category">
            <option value="">all</option>
            <option value="ban_appeal">ban_appeal</option>
          </select>
          <button className="btn ghost" type="button" onClick={() => void load(category)}>Refresh</button>
        </div>
      </header>
      {error && <p className="error-text">{error}</p>}
      <div className="table-wrap">
        <table className="table">
          <thead>
            <tr>
              <th>ID</th>
              <th>User</th>
              <th>Category</th>
              <th>Status</th>
              <th>Message</th>
              <th>Action</th>
            </tr>
          </thead>
          <tbody>
            {tickets.map((t) => (
              <tr key={t.id}>
                <td>{t.id}</td>
                <td>{t.user}</td>
                <td>{t.category}</td>
                <td>{t.status}</td>
                <td>{t.message}</td>
                <td>
                  <div className="actions-row">
                    <button className="btn ghost" type="button" onClick={async () => {
                      const response = window.prompt("Reply message:", "") || "";
                      await adminApi.updateSupport(t.id, "open", response);
                      await load(category);
                    }}>Reply</button>
                    <button className="btn ghost" type="button" onClick={async () => {
                      const response = window.prompt("Optional close message:", "") || "";
                      await adminApi.updateSupport(t.id, "closed", response);
                      await load(category);
                    }}>Close</button>
                  </div>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </section>
  );
};

export default SupportPage;
