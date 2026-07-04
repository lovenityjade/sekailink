import React from "react";
import { Navigate, Route, Routes } from "react-router-dom";
import ErrorBoundary from "@/components/ErrorBoundary";
import LobbyPage from "@/pages/Lobby";
import GameManagerPage from "@/pages/GameManager";
import AccountPage from "@/pages/Account";
import HelpPage from "@/pages/Help";
import PlayerOptionsPage from "@/pages/PlayerOptions";
import SettingsPage from "@/pages/Settings";
import AuthGatePrototype from "./ui/auth/AuthGatePrototype";
import PrototypeShell from "./ui/shell/PrototypeShell";
import DashboardPage from "./ui/pages/Dashboard";

const PrototypeApp: React.FC = () => {
  return (
    <ErrorBoundary>
      <AuthGatePrototype>
        <PrototypeShell>
          <Routes>
            <Route path="/" element={<DashboardPage />} />
            <Route path="/lobby/:lobbyId" element={<LobbyPage />} />
            <Route path="/dashboard/yaml/new" element={<GameManagerPage />} />
            <Route path="/player-options/:game/create" element={<PlayerOptionsPage mode="create" />} />
            <Route path="/player-options/:game/edit/:yamlId" element={<PlayerOptionsPage mode="edit" />} />
            <Route path="/player-options/import/:importId" element={<PlayerOptionsPage mode="import" />} />
            <Route path="/settings" element={<SettingsPage />} />
            <Route path="/account" element={<AccountPage />} />
            <Route path="/help" element={<HelpPage />} />
            <Route path="*" element={<Navigate to="/" replace />} />
          </Routes>
        </PrototypeShell>
      </AuthGatePrototype>
    </ErrorBoundary>
  );
};

export default PrototypeApp;
