import React, { useEffect, useState } from "react";
import { adminApi } from "@/services/adminApi";

type AdminUser = {
  discord_id: string;
  display_name: string;
  role: string;
  banned: boolean;
  ban_reason: string;
  last_login: string | null;
};

const UsersPage: React.FC = () => {
  const [users, setUsers] = useState<AdminUser[]>([]);
  const [search, setSearch] = useState("");
  const [error, setError] = useState("");

  const loadUsers = async (query = "") => {
    try {
      const data = await adminApi.getUsers(query);
      setUsers((data.users || []) as AdminUser[]);
      setError("");
    } catch (err) {
      setError(err instanceof Error ? err.message : String(err || "users_load_failed"));
    }
  };

  useEffect(() => {
    void loadUsers();
  }, []);

  return (
    <section className="page-section" aria-label="Users moderation">
      <header className="panel-header">
        <h2>Users</h2>
        <div className="actions-row">
          <input
            className="input"
            value={search}
            onChange={(e) => setSearch(e.target.value)}
            placeholder="Search by name, id, email"
            aria-label="Search users"
          />
          <button className="btn ghost" type="button" onClick={() => void loadUsers(search.trim())}>Search</button>
          <button className="btn ghost" type="button" onClick={() => void loadUsers()}>Refresh</button>
        </div>
      </header>

      {error && <p className="error-text">{error}</p>}

      <div className="table-wrap">
        <table className="table">
          <thead>
            <tr>
              <th>User</th>
              <th>ID</th>
              <th>Role</th>
              <th>Status</th>
              <th>Ban reason</th>
              <th>Action</th>
            </tr>
          </thead>
          <tbody>
            {users.map((u) => (
              <tr key={u.discord_id}>
                <td>{u.display_name}</td>
                <td><code>{u.discord_id}</code></td>
                <td>
                  <select
                    className="input"
                    defaultValue={u.role}
                    aria-label={`Role for ${u.display_name}`}
                    onChange={async (e) => {
                      await adminApi.setUserRole(u.discord_id, e.target.value);
                      await loadUsers(search.trim());
                    }}
                  >
                    <option value="admin">admin</option>
                    <option value="moderator">moderator</option>
                    <option value="player">player</option>
                  </select>
                </td>
                <td>{u.banned ? "Banned" : "Active"}</td>
                <td>{u.ban_reason || "-"}</td>
                <td>
                  <button
                    className="btn ghost"
                    type="button"
                    onClick={async () => {
                      const reason = u.banned ? "" : window.prompt("Ban reason:", u.ban_reason || "") || "";
                      await adminApi.setUserBan(u.discord_id, !u.banned, reason);
                      await loadUsers(search.trim());
                    }}
                  >
                    {u.banned ? "Unban" : "Ban"}
                  </button>
                </td>
              </tr>
            ))}
          </tbody>
        </table>
      </div>
    </section>
  );
};

export default UsersPage;
