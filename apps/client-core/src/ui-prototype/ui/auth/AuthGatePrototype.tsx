import React from "react";
import AuthGate from "@/components/AuthGate";

interface AuthGatePrototypeProps {
  children: React.ReactNode;
}

// Prototype entry now uses the same auth/update gate as the main client.
// This prevents auth/update behavior drift between entries.
const AuthGatePrototype: React.FC<AuthGatePrototypeProps> = ({ children }) => {
  return <AuthGate>{children}</AuthGate>;
};

export default AuthGatePrototype;
