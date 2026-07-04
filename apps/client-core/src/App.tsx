import React from "react";
import ErrorBoundary from "./components/ErrorBoundary";
import RedesignApp from "./redesign/App";
import AuthGatePrototype from "./ui-prototype/ui/auth/AuthGatePrototype";

const App: React.FC = () => {
  const isRuntimeSocialWindow =
    typeof window !== "undefined" && window.location.hash.startsWith("#/runtime-social/");

  return (
    <ErrorBoundary>
      {isRuntimeSocialWindow ? (
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
