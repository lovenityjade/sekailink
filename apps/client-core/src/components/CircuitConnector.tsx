/*
 * Circuit Forge Design System — CircuitConnector
 * SVG decorative circuit traces that connect panels visually.
 */

export function HorizontalTrace({ className = "" }: { className?: string }) {
  return (
    <svg
      className={`w-full h-3 ${className}`}
      viewBox="0 0 200 12"
      preserveAspectRatio="none"
      fill="none"
    >
      <line
        x1="0"
        y1="6"
        x2="80"
        y2="6"
        stroke="rgba(0,255,200,0.12)"
        strokeWidth="1"
        strokeDasharray="4 4"
        className="circuit-trace"
      />
      <circle cx="80" cy="6" r="2" fill="rgba(0,255,200,0.2)" />
      <line
        x1="80"
        y1="6"
        x2="120"
        y2="6"
        stroke="rgba(0,255,200,0.08)"
        strokeWidth="1"
      />
      <circle cx="120" cy="6" r="2" fill="rgba(0,255,200,0.2)" />
      <line
        x1="120"
        y1="6"
        x2="200"
        y2="6"
        stroke="rgba(0,255,200,0.12)"
        strokeWidth="1"
        strokeDasharray="4 4"
        className="circuit-trace"
      />
    </svg>
  );
}

export function VerticalTrace({ className = "" }: { className?: string }) {
  return (
    <svg
      className={`w-3 h-full ${className}`}
      viewBox="0 0 12 200"
      preserveAspectRatio="none"
      fill="none"
    >
      <line
        x1="6"
        y1="0"
        x2="6"
        y2="80"
        stroke="rgba(0,255,200,0.12)"
        strokeWidth="1"
        strokeDasharray="4 4"
        className="circuit-trace"
      />
      <circle cx="6" cy="80" r="2" fill="rgba(0,255,200,0.2)" />
      <line
        x1="6"
        y1="80"
        x2="6"
        y2="120"
        stroke="rgba(0,255,200,0.08)"
        strokeWidth="1"
      />
      <circle cx="6" cy="120" r="2" fill="rgba(0,255,200,0.2)" />
      <line
        x1="6"
        y1="120"
        x2="6"
        y2="200"
        stroke="rgba(0,255,200,0.12)"
        strokeWidth="1"
        strokeDasharray="4 4"
        className="circuit-trace"
      />
    </svg>
  );
}

export function CornerTrace({ className = "" }: { className?: string }) {
  return (
    <svg
      className={`w-6 h-6 ${className}`}
      viewBox="0 0 24 24"
      fill="none"
    >
      <path
        d="M 0 12 L 12 12 L 12 24"
        stroke="rgba(0,255,200,0.15)"
        strokeWidth="1"
        strokeDasharray="3 3"
        className="circuit-trace"
      />
      <circle cx="12" cy="12" r="2" fill="rgba(0,255,200,0.25)" />
    </svg>
  );
}

export function JunctionDot({ className = "" }: { className?: string }) {
  return (
    <div className={`relative ${className}`}>
      <div className="w-2 h-2 bg-teal/30 rounded-none" />
      <div
        className="absolute inset-0 w-2 h-2 bg-teal/15 rounded-none"
        style={{
          animation: "power-pulse 2s ease-in-out infinite",
          transform: "scale(2)",
          transformOrigin: "center",
        }}
      />
    </div>
  );
}
