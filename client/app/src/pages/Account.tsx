import React, { useEffect, useState } from "react";
import { Link, useNavigate } from "react-router-dom";
import { apiFetch, apiJson } from "../services/api";
import { useI18n } from "../i18n";

interface YamlEntry {
  id: string;
  title: string;
  game: string;
  player_name: string;
  custom?: boolean;
}

interface MeResponse {
  discord_id: string;
  username?: string;
  global_name?: string;
  email?: string;
  avatar_url?: string;
  terms_accepted?: boolean;
  terms_version?: string;
  terms_accepted_at?: string | null;
  presence_status?: string;
  dm_policy?: string;
}

const AccountPage: React.FC = () => {
  const { t } = useI18n();
  const [yamls, setYamls] = useState<YamlEntry[]>([]);
  const [termsStatus, setTermsStatus] = useState<"accepted" | "pending">("pending");
  const [termsVersion, setTermsVersion] = useState("v1");
  const [termsAcceptedAt, setTermsAcceptedAt] = useState<string | null>(null);
  const [termsError, setTermsError] = useState("");
  const [socialStatus, setSocialStatus] = useState("online");
  const [dmPolicy, setDmPolicy] = useState("anyone");
  const [socialStatusMsg, setSocialStatusMsg] = useState("");
  const [me, setMe] = useState<MeResponse | null>(null);
  const [importStatus, setImportStatus] = useState("");
  const navigate = useNavigate();

  const loadYamls = async () => {
    try {
      const data = await apiJson<{ yamls: YamlEntry[] }>("/api/yamls");
      setYamls(data.yamls || []);
    } catch {
      setYamls([]);
    }
  };

  const loadSocial = async () => {
    try {
      const data = await apiJson<{ presence_status: string; dm_policy: string }>("/api/social/settings");
      setSocialStatus(data.presence_status || "online");
      setDmPolicy(data.dm_policy || "anyone");
    } catch {
      // ignore
    }
  };

  useEffect(() => {
    loadYamls();
    loadSocial();
    apiJson<MeResponse>("/api/me")
      .then((data) => {
        setMe(data);
        setTermsVersion(data.terms_version || "v1");
        const accepted = Boolean(data.terms_accepted) && data.terms_version === (data.terms_version || "v1");
        setTermsStatus(accepted ? "accepted" : "pending");
        setTermsAcceptedAt(data.terms_accepted_at || null);
      })
      .catch(() => {
        setMe(null);
      });
  }, []);

  const acceptTerms = async () => {
    setTermsError("");
    try {
      const res = await apiFetch("/api/auth/terms", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ accept: true })
      });
      if (!res.ok) throw new Error("Terms acceptance failed.");
    setTermsStatus("accepted");
  } catch {
      setTermsError("Unable to save your acceptance. Please try again.");
    }
  };

  const saveSocial = async () => {
    try {
      setSocialStatusMsg(t("account.saving"));
      const res = await apiFetch("/api/social/settings", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ presence_status: socialStatus, dm_policy: dmPolicy })
      });
      if (!res.ok) throw new Error("Unable to save social settings.");
      setSocialStatusMsg(t("settings.saved"));
    } catch {
      setSocialStatusMsg("Unable to save settings.");
    }
  };

  const onImportYaml = async (event: React.ChangeEvent<HTMLInputElement>) => {
    if (!event.target.files?.length) return;
    const file = event.target.files[0];
    setImportStatus(t("account.importing"));
    try {
      const content = await file.text();
      const res = await apiFetch("/api/yamls/import", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ content })
      });
      if (!res.ok) {
        const data = await res.json().catch(() => ({}));
        throw new Error(data.error || "Unable to import YAML.");
      }
      const data = await res.json().catch(() => ({}));
      if (data.import_id) {
        navigate(`/player-options/import/${encodeURIComponent(data.import_id)}`);
        setImportStatus("Opening YAML in player options…");
      } else {
        setImportStatus("Import ready.");
      }
    } catch (err) {
      setImportStatus(err instanceof Error ? err.message : "Unable to import YAML.");
    } finally {
      event.target.value = "";
    }
  };

  const downloadYaml = async (yamlId: string) => {
    if (!yamlId) return;
    try {
      setImportStatus("Preparing download...");
      const res = await apiFetch(`/api/yamls/${encodeURIComponent(yamlId)}/raw`);
      if (!res.ok) throw new Error("Unable to download YAML.");
      const data = await res.json();
      const blob = new Blob([data.yaml || ""], { type: "text/yaml" });
      const url = URL.createObjectURL(blob);
      const anchor = document.createElement("a");
      anchor.href = url;
      anchor.download = `${yamlId}.yaml`;
      document.body.appendChild(anchor);
      anchor.click();
      document.body.removeChild(anchor);
      URL.revokeObjectURL(url);
      setImportStatus("");
    } catch (err) {
      setImportStatus(err instanceof Error ? err.message : "Unable to download YAML.");
    }
  };

  const deleteYaml = async (yamlId: string) => {
    if (!yamlId) return;
    try {
      setImportStatus("Deleting...");
      const res = await apiFetch(`/api/yamls/${encodeURIComponent(yamlId)}/delete`, { method: "POST" });
      if (!res.ok) throw new Error("Unable to delete YAML.");
      setYamls((prev) => prev.filter((item) => item.id !== yamlId));
      setImportStatus(t("account.deleted"));
    } catch (err) {
      setImportStatus(err instanceof Error ? err.message : "Unable to delete YAML.");
    }
  };

  return (
    <div className="dashboard">
      <section className="dashboard-hero">
        <div className="dashboard-hero-card">
          <div className="dashboard-title">
            <span>{t("account.dashboard")}</span>
            <h1>{t("account.control_panel")}</h1>
          </div>
          <div className="dashboard-profile">
            {me?.avatar_url ? (
              <img className="account-avatar" src={me.avatar_url} alt="Profile avatar" />
            ) : (
              <div className="account-avatar" />
            )}
            <div>
              <div className="account-name">{me?.global_name || me?.username || t("account.signed_in")}</div>
              <div className="account-id">Discord ID: {me?.discord_id || "—"}</div>
              <div className="account-email">Email: {me?.email || "Not shared"}</div>
            </div>
          </div>
        </div>
        <div className="dashboard-hero-meta">
          <div className="dashboard-stat">
            <span className="stat-label">{t("account.terms_version")}</span>
            <span className="stat-value">{termsVersion}</span>
          </div>
          <div className="dashboard-stat">
            <span className="stat-label">{t("account.status")}</span>
            <span className={`stat-value ${termsStatus === "accepted" ? "ok" : "warn"}`}>
              {termsStatus === "accepted" ? t("account.active") : t("account.action_required")}
            </span>
          </div>
        </div>
      </section>

      <section className="dashboard-grid">
        <div className="dashboard-card">
          <h3>{t("account.terms_community")}</h3>
          {termsStatus === "accepted" ? (
            <p>Status: Accepted ({termsVersion}) {termsAcceptedAt ? `· ${termsAcceptedAt}` : ""}</p>
          ) : (
            <>
              <p>{t("account.action_required")}: Please accept the latest terms.</p>
              <button className="account-accept" id="account-accept" onClick={acceptTerms}>
                {t("account.accept_terms")}
              </button>
              <span className="account-error" id="account-error">{termsError}</span>
            </>
          )}
        </div>
        <div className="dashboard-card">
          <h3>{t("account.identity")}</h3>
          <p>Username: —</p>
          <p>Global Name: —</p>
          <p>Email: Not shared</p>
        </div>
        <div className="dashboard-card">
          <h3>Social Settings</h3>
          <p>Control your online status and who can message you.</p>
          <label className="account-select">
            Status
            <select id="social-status-select" value={socialStatus} onChange={(event) => setSocialStatus(event.target.value)}>
              <option value="online">Online</option>
              <option value="dnd">Non-available</option>
              <option value="offline">Offline</option>
            </select>
          </label>
          <label className="account-select">
            Direct messages
            <select id="social-dm-policy-select" value={dmPolicy} onChange={(event) => setDmPolicy(event.target.value)}>
              <option value="anyone">Anyone</option>
              <option value="friends">Friends only</option>
              <option value="none">No one</option>
            </select>
          </label>
          <button className="skl-btn ghost" type="button" onClick={saveSocial}>Save</button>
          <span className="account-status" id="social-settings-status">{socialStatusMsg}</span>
        </div>
        <div className="dashboard-card full">
          <div className="yaml-header">
            <h3>Game Manager</h3>
              <div className="yaml-header-actions">
              <Link className="yaml-create" to="/dashboard/yaml/new">Open Game Manager</Link>
              <label className="yaml-import">
                Import YAML
                <input type="file" accept=".yml,.yaml" style={{ display: "none" }} onChange={onImportYaml} />
              </label>
            </div>
          </div>
          {importStatus && <div className="skl-ready-status">{importStatus}</div>}
          {yamls.length ? (
            <div className="yaml-list">
              {yamls.map((item) => (
                <div className="yaml-row" key={item.id}>
                  <div className="yaml-meta">
                    <div className="yaml-title">
                      {item.title}
                      {item.custom && <span className="yaml-badge">Custom</span>}
                    </div>
                    <div className="yaml-sub">Game: {item.game} • Player: {item.player_name}</div>
                  </div>
                  <div className="yaml-actions">
                    <button
                      type="button"
                      onClick={() => navigate(`/dashboard/yaml/new?tab=editor&yaml=${encodeURIComponent(item.id)}`)}
                    >
                      Edit
                    </button>
                    <button type="button" onClick={() => downloadYaml(item.id)}>Download</button>
                    <button type="button" onClick={() => deleteYaml(item.id)}>Delete</button>
                  </div>
                </div>
              ))}
            </div>
          ) : (
            <p>No YAMLs saved yet.</p>
          )}
        </div>
      </section>
    </div>
  );
};

export default AccountPage;
