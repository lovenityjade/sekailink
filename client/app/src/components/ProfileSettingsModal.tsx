import React, { useCallback, useEffect, useMemo, useState } from "react";
import { useI18n } from "../i18n";
import { apiFetch, apiUrl } from "../services/api";
import { runtime } from "../services/runtime";

interface MeLike {
  discord_id: string;
  username?: string;
  global_name?: string;
  avatar_url?: string;
}

interface ProfileSettingsModalProps {
  open: boolean;
  me?: MeLike | null;
  onClose: () => void;
}

interface LocalProfileSettings {
  displayName: string;
  bio: string;
  twitchUsername: string;
  twitchLinked: boolean;
}

interface RemoteProfileResponse {
  display_name?: string;
  bio?: string;
  twitch_username?: string;
  twitch_login?: string;
  twitch_name?: string;
  twitch_linked?: boolean;
  twitch_connected?: boolean;
  twitch_user_id?: string | null;
  linked_twitch?: boolean;
}

interface MeResponse {
  username?: string;
  global_name?: string;
  email?: string;
}

const emptySettings: LocalProfileSettings = {
  displayName: "",
  bio: "",
  twitchUsername: "",
  twitchLinked: false,
};

const ProfileSettingsModal: React.FC<ProfileSettingsModalProps> = ({ open, me, onClose }) => {
  const { t } = useI18n();
  const [settings, setSettings] = useState<LocalProfileSettings>(emptySettings);
  const [isLoading, setIsLoading] = useState(false);
  const [isSaving, setIsSaving] = useState(false);
  const [saved, setSaved] = useState(false);

  const storageKey = useMemo(
    () => `skl_profile_settings_${me?.discord_id || "guest"}`,
    [me?.discord_id]
  );

  const loadLocalSettings = useCallback(() => {
    const fallbackName = me?.global_name || me?.username || "";
    try {
      const raw = window.localStorage.getItem(storageKey);
      if (!raw) {
        return { ...emptySettings, displayName: fallbackName };
      }
      const parsed = JSON.parse(raw) as Partial<LocalProfileSettings>;
      return {
        displayName: typeof parsed.displayName === "string" && parsed.displayName.trim() ? parsed.displayName : fallbackName,
        bio: typeof parsed.bio === "string" ? parsed.bio : "",
        twitchUsername: typeof parsed.twitchUsername === "string" ? parsed.twitchUsername : "",
        twitchLinked: Boolean(parsed.twitchLinked),
      } as LocalProfileSettings;
    } catch {
      return { ...emptySettings, displayName: fallbackName };
    }
  }, [me?.global_name, me?.username, storageKey]);

  const loadFromServer = useCallback(async () => {
    setIsLoading(true);
    try {
      const res = await apiFetch("/api/me/profile");
      if (res.ok) {
        const data = (await res.json()) as RemoteProfileResponse;
        setSettings((prev) => ({
          displayName: (data.display_name || prev.displayName || me?.global_name || me?.username || "").trim(),
          bio: String(data.bio || ""),
          twitchUsername: String(data.twitch_username || data.twitch_login || data.twitch_name || ""),
          twitchLinked: Boolean(
            data.twitch_linked
            || data.twitch_connected
            || data.linked_twitch
            || (typeof data.twitch_user_id === "string" && data.twitch_user_id.length > 0)
          ),
        }));
        return;
      }
      // fall through to legacy endpoints
      const meRes = await apiFetch("/api/me");
      if (meRes.ok) {
        const meData = (await meRes.json()) as MeResponse;
        setSettings((prev) => ({
          ...prev,
          displayName: String(meData.global_name || meData.username || prev.displayName || ""),
        }));
      }
    } catch {
      // keep local data fallback
      try {
        const meRes = await apiFetch("/api/me");
        if (meRes.ok) {
          const meData = (await meRes.json()) as MeResponse;
          setSettings((prev) => ({
            ...prev,
            displayName: String(meData.global_name || meData.username || prev.displayName || ""),
          }));
        }
      } catch {
        // ignore
      }
    } finally {
      setIsLoading(false);
    }
  }, [me?.global_name, me?.username]);

  useEffect(() => {
    if (!open) return;
    setSaved(false);
    setSettings(loadLocalSettings());
    void loadFromServer();
  }, [open, loadLocalSettings, loadFromServer]);

  const save = async () => {
    const next = {
      displayName: settings.displayName.trim(),
      bio: settings.bio.trim(),
      twitchUsername: settings.twitchUsername.trim(),
      twitchLinked: settings.twitchLinked,
    };

    setIsSaving(true);
    try {
      const res = await apiFetch("/api/me/profile", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          display_name: next.displayName,
          bio: next.bio,
          twitch_username: next.twitchUsername,
          twitch_linked: next.twitchLinked,
        }),
      });
      if (!res.ok) {
        throw new Error(`save_profile_failed:${res.status}`);
      }
      const maybeData = (await res.json().catch(() => null)) as RemoteProfileResponse | null;
      if (maybeData && typeof maybeData === "object") {
        setSettings((prev) => ({
          ...prev,
          twitchLinked: Boolean(
            maybeData.twitch_linked
            || maybeData.twitch_connected
            || maybeData.linked_twitch
            || (typeof maybeData.twitch_user_id === "string" && maybeData.twitch_user_id.length > 0)
          ),
          twitchUsername: String(maybeData.twitch_username || maybeData.twitch_login || maybeData.twitch_name || prev.twitchUsername),
        }));
      }
    } catch {
      // fallback to legacy server profile endpoint (/api/account/profile)
      try {
        const fd = new FormData();
        fd.set("bio", next.bio);
        // Preserve current badges if server expects this field.
        fd.set("badges", "");
        const legacyRes = await apiFetch("/api/account/profile", {
          method: "POST",
          body: fd,
        });
        if (!legacyRes.ok) throw new Error(`legacy_profile_save_failed:${legacyRes.status}`);
      } catch {
        // final fallback is local persistence only
      }
    } finally {
      setIsSaving(false);
    }

    try {
      window.localStorage.setItem(storageKey, JSON.stringify(next));
    } catch {
      // ignore local storage issues
    }

    setSaved(true);
    window.setTimeout(() => setSaved(false), 1600);
  };

  const openTwitchLink = () => {
    const target = apiUrl("/api/auth/twitch/login");
    if (runtime.openExternal) {
      runtime.openExternal(target);
      return;
    }
    window.location.href = target;
  };

  return (
    <div className={`skl-modal${open ? " open" : ""}`} aria-hidden={!open}>
      <div className="skl-modal-backdrop" onClick={onClose}></div>
      <div className="skl-modal-panel skl-profile-settings-modal" role="dialog" aria-modal="true" aria-labelledby="skl-profile-settings-title">
        <div className="skl-modal-header">
          <h3 id="skl-profile-settings-title">{t("profile.settings.title")}</h3>
          <button className="skl-btn ghost" type="button" onClick={onClose}>{t("common.close")}</button>
        </div>
        <div className="skl-modal-body skl-profile-settings-body">
          <p className="skl-profile-settings-subtitle">{t("profile.settings.subtitle")}</p>
          {isLoading ? <p className="skl-profile-settings-help">{t("profile.settings.loading")}</p> : null}

          <label className="skl-profile-settings-field">
            <span>{t("profile.settings.display_name")}</span>
            <input
              className="input"
              value={settings.displayName}
              onChange={(e) => setSettings((prev) => ({ ...prev, displayName: e.target.value }))}
              maxLength={48}
            />
          </label>

          <label className="skl-profile-settings-field">
            <span>{t("profile.settings.bio")}</span>
            <textarea
              className="input skl-profile-settings-textarea"
              value={settings.bio}
              onChange={(e) => setSettings((prev) => ({ ...prev, bio: e.target.value }))}
              maxLength={220}
            />
          </label>

          <label className="skl-profile-settings-field">
            <span>{t("profile.settings.twitch_username")}</span>
            <input
              className="input"
              value={settings.twitchUsername}
              onChange={(e) => setSettings((prev) => ({ ...prev, twitchUsername: e.target.value }))}
              placeholder="your_twitch_name"
              maxLength={64}
            />
          </label>
          <p className="skl-profile-settings-help">{t("profile.settings.twitch_help")}</p>
          <p className={`skl-profile-settings-help${settings.twitchLinked ? " is-linked" : ""}`}>
            {settings.twitchLinked ? t("profile.settings.twitch_status_linked") : t("profile.settings.twitch_status_not_linked")}
          </p>

          <div className="skl-profile-settings-row">
            <button
              type="button"
              className="skl-btn ghost"
              onClick={() => {
                setSettings((prev) => ({ ...prev, twitchLinked: true }));
                openTwitchLink();
              }}
            >
              {t("profile.settings.link_twitch")}
            </button>
            <button
              type="button"
              className="skl-btn ghost"
              onClick={() => setSettings((prev) => ({ ...prev, twitchLinked: false }))}
            >
              {t("profile.settings.unlink_twitch")}
            </button>
          </div>

          <div className="skl-profile-settings-actions">
            <button type="button" className="skl-btn primary" onClick={save} disabled={isSaving}>
              {isSaving ? t("account.saving") : t("profile.settings.save")}
            </button>
            {saved ? <span className="skl-profile-settings-saved">{t("profile.settings.saved")}</span> : null}
          </div>
        </div>
      </div>
    </div>
  );
};

export default ProfileSettingsModal;
