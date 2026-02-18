import React from "react";

interface ErrorBoundaryProps {
  children: React.ReactNode;
}

interface ErrorBoundaryState {
  hasError: boolean;
  message?: string;
}

class ErrorBoundary extends React.Component<ErrorBoundaryProps, ErrorBoundaryState> {
  state: ErrorBoundaryState = { hasError: false };

  static getDerivedStateFromError(error: unknown): ErrorBoundaryState {
    const message = error instanceof Error ? error.message : "Unexpected error.";
    return { hasError: true, message };
  }

  componentDidCatch(error: unknown) {
    // Keep a lightweight console breadcrumb for debugging.
    console.error("Renderer crash:", error);
  }

  handleReload = () => {
    window.location.reload();
  };

  render() {
    if (this.state.hasError) {
      return (
        <div className="skl-app-panel" style={{ margin: "24px" }}>
          <h1 className="skl-app-title">App failed to load</h1>
          <p style={{ color: "var(--text-1)" }}>
            {this.state.message || "Unexpected error. Check devtools for details."}
          </p>
          <button className="skl-btn primary" type="button" onClick={this.handleReload}>
            Reload
          </button>
        </div>
      );
    }

    return this.props.children;
  }
}

export default ErrorBoundary;
