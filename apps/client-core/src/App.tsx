import React from "react";
import ErrorBoundary from "./components/ErrorBoundary";
import RedesignApp from "./redesign/App";
import AuthGatePrototype from "./ui-prototype/ui/auth/AuthGatePrototype";

const App: React.FC = () => {
  return (
    <ErrorBoundary>
      <AuthGatePrototype>
        <RedesignApp />
      </AuthGatePrototype>
    </ErrorBoundary>
  );
};

export default App;
