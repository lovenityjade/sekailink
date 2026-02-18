import React, { useEffect } from "react";
import Sidebar from "./Sidebar";
import AboutModal from "./AboutModal";
import ContributeModal from "./ContributeModal";
import SocialDrawer from "./SocialDrawer";
import { apiJson } from "../services/api";

interface MeResponse {
  discord_id: string;
  username?: string;
  global_name?: string;
  avatar_url?: string;
  presence_status?: string;
}

interface AppShellProps {
  children: React.ReactNode;
}

const AppShell: React.FC<AppShellProps> = ({ children }) => {
  const [aboutOpen, setAboutOpen] = React.useState(false);
  const [contributeOpen, setContributeOpen] = React.useState(false);
  const [friendsOpen, setFriendsOpen] = React.useState(false);
  const [me, setMe] = React.useState<MeResponse | null>(null);

  useEffect(() => {
    document.body.classList.add("app-ui");
    return () => {
      document.body.classList.remove("app-ui");
    };
  }, []);

  useEffect(() => {
    apiJson<MeResponse>("/api/me")
      .then((data) => setMe(data))
      .catch(() => setMe(null));
  }, []);

  return (
    <>
      <div className="page-bg"></div>
      <div className="skl-app-shell">
        <Sidebar
          me={me}
          onOpenAbout={() => setAboutOpen(true)}
          onOpenContribute={() => setContributeOpen(true)}
          onOpenFriends={() => setFriendsOpen(true)}
        />
        <div className="skl-app-main">{children}</div>
      </div>
      <SocialDrawer open={friendsOpen} onClose={() => setFriendsOpen(false)} />
      <AboutModal open={aboutOpen} onClose={() => setAboutOpen(false)} />
      <ContributeModal open={contributeOpen} onClose={() => setContributeOpen(false)} />
    </>
  );
};

export default AppShell;
