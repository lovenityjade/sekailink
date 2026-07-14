import React from "react";
import ErrorBoundary from "./components/ErrorBoundary";
import RedesignApp from "./redesign/App";
import AuthGatePrototype from "./ui-prototype/ui/auth/AuthGatePrototype";

const App: React.FC = () => {
  const isRuntimeSocialWindow =
    typeof window !== "undefined" && window.location.hash.startsWith("#/runtime-social/");
  const isHostConsoleWindow =
    typeof window !== "undefined" && window.location.hash.startsWith("#/host-console/");

  return (
    <ErrorBoundary>
      {isRuntimeSocialWindow || isHostConsoleWindow ? (
        <RedesignApp />
      ) : (
        <AuthGatePrototype>
          <RedesignApp />
        </AuthGatePrototype>
      )}
    </ErrorBoundary>
  );
};

export default App;
