import React, { useMemo, useState } from "react";
import AuthGate from "@/components/AuthGate";
import type { AdminMe } from "@/services/adminApi";
import DashboardPage from "@/pages/DashboardPage";
import UsersPage from "@/pages/UsersPage";
import LobbiesPage from "@/pages/LobbiesPage";
import RoomsPage from "@/pages/RoomsPage";
import LogsPage from "@/pages/LogsPage";
import SupportPage from "@/pages/SupportPage";
import DbPage from "@/pages/DbPage";
import BoxArtPage from "@/pages/BoxArtPage";
import pkg from "../package.json";

type TabKey = "dashboard" | "users" | "lobbies" | "rooms" | "boxart" | "logs" | "support" | "db";

const tabs: Array<{ key: TabKey; label: string }> = [
  { key: "dashboard", label: "Dashboard" },
  { key: "users", label: "Users" },
  { key: "lobbies", label: "Lobbies" },
  { key: "rooms", label: "Room Servers" },
  { key: "boxart", label: "Box Art" },
  { key: "logs", label: "Logs" },
  { key: "support", label: "Support" },
  { key: "db", label: "DB" }
];
const APP_VERSION = String((pkg as { version?: string })?.version || "0.0.1");

const App: React.FC = () => {
  const [activeTab, setActiveTab] = useState<TabKey>("dashboard");
  const [me, setMe] = useState<AdminMe | null>(null);

  const page = useMemo(() => {
    switch (activeTab) {
      case "users":
        return <UsersPage />;
      case "lobbies":
        return <LobbiesPage />;
      case "rooms":
        return <RoomsPage />;
      case "boxart":
        return <BoxArtPage />;
      case "logs":
        return <LogsPage />;
      case "support":
        return <SupportPage />;
      case "db":
        return <DbPage />;
      case "dashboard":
      default:
        return <DashboardPage />;
    }
  }, [activeTab]);

  return (
    <AuthGate onAuthed={setMe}>
      <div className="admin-shell">
        <aside className="sidebar" aria-label="Admin navigation">
          <h1>SekaiLink Admin</h1>
          <p className="sidebar-meta">v{APP_VERSION}</p>
          {me && (
            <p className="sidebar-meta">
              {me.display_name}<br />
              <span className="role-pill">{me.role}</span>
            </p>
          )}
          <nav className="sidebar-nav">
            {tabs.map((tab) => (
              <button
                key={tab.key}
                className={`nav-btn${activeTab === tab.key ? " active" : ""}`}
                onClick={() => setActiveTab(tab.key)}
                type="button"
              >
                {tab.label}
              </button>
            ))}
          </nav>
        </aside>
        <main className="main-content">{page}</main>
      </div>
    </AuthGate>
  );
};

export default App;
