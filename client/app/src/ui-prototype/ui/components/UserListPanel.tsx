import React, { useMemo } from "react";
import { useI18n } from "@/i18n";

type UserRow = { name: string; ready: boolean; avatar_url?: string; status?: string };

type Props = {
  users: UserRow[];
};

const UserListPanel: React.FC<Props> = ({ users }) => {
  const { t } = useI18n();
  const clock = useMemo(
    () => new Date().toLocaleTimeString(undefined, { hour: "2-digit", minute: "2-digit" }),
    // eslint-disable-next-line react-hooks/exhaustive-deps
    []
  );

  return (
    <section className="sklp-panel sklp-users">
      <header className="sklp-panel-head">
        <div className="sklp-title">{t("proto.user_list.title")}</div>
      </header>
      <div className="sklp-users-scroll">
        {users.length ? (
          users.map((u) => (
            <div key={u.name} className="sklp-userrow">
              <div className="sklp-avatar" aria-hidden="true">
                {u.avatar_url ? <img src={u.avatar_url} alt="" /> : null}
              </div>
              <div className="sklp-username">{u.name}</div>
              <div className={`sklp-status${u.ready ? " ok" : ""}`}>{u.ready ? t("proto.ready").toUpperCase() : (u.status ? String(u.status).toUpperCase() : t("status.online").toUpperCase())}</div>
            </div>
          ))
        ) : (
          <div style={{ padding: 12, opacity: 0.72 }}>{t("proto.user_list.empty")}</div>
        )}
      </div>
      <div className="sklp-footer">
        <div className="sklp-clock">{clock}</div>
      </div>
    </section>
  );
};

export default UserListPanel;
