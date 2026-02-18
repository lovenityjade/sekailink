import React from "react";
import { Navigate, Route, Routes } from "react-router-dom";
import AppShell from "./components/AppShell";
import ErrorBoundary from "./components/ErrorBoundary";
import AuthGate from "./components/AuthGate";
import RoomListPage from "./pages/RoomList";
import LobbyPage from "./pages/Lobby";
import GameManagerPage from "./pages/GameManager";
import AccountPage from "./pages/Account";
import HelpPage from "./pages/Help";
import PlayerOptionsPage from "./pages/PlayerOptions";
import SettingsPage from "./pages/Settings";

const App: React.FC = () => {
  return (
    <ErrorBoundary>
      <AuthGate>
        <AppShell>
          <Routes>
            <Route path="/" element={<Navigate to="/rooms" replace />} />
            <Route path="/rooms" element={<RoomListPage />} />
            <Route path="/lobby/:lobbyId" element={<LobbyPage />} />
            <Route path="/dashboard/yaml/new" element={<GameManagerPage />} />
            <Route path="/player-options/:game/create" element={<PlayerOptionsPage mode="create" />} />
            <Route path="/player-options/:game/edit/:yamlId" element={<PlayerOptionsPage mode="edit" />} />
            <Route path="/player-options/import/:importId" element={<PlayerOptionsPage mode="import" />} />
            <Route path="/settings" element={<SettingsPage />} />
            <Route path="/account" element={<AccountPage />} />
            <Route path="/help" element={<HelpPage />} />
            <Route path="*" element={<Navigate to="/rooms" replace />} />
          </Routes>
        </AppShell>
      </AuthGate>
    </ErrorBoundary>
  );
};

export default App;
