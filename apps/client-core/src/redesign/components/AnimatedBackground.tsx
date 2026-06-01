export default function AnimatedBackground() {
  return (
    <div className="absolute inset-0 overflow-hidden pointer-events-none">
      {/* Gradient Orbs */}
      <div className="absolute top-20 -left-20 w-96 h-96 bg-gradient-to-br from-[#ff6b35]/10 to-transparent rounded-full blur-3xl animate-float" />
      <div className="absolute bottom-20 -right-20 w-96 h-96 bg-gradient-to-br from-[#4ecdc4]/10 to-transparent rounded-full blur-3xl animate-float-delayed" />

      {/* Tech Grid */}
      <div className="absolute inset-0 opacity-5">
        <div className="absolute inset-0" style={{
          backgroundImage: `
            linear-gradient(rgba(77, 255, 210, 0.3) 1px, transparent 1px),
            linear-gradient(90deg, rgba(77, 255, 210, 0.3) 1px, transparent 1px)
          `,
          backgroundSize: '50px 50px',
          animation: 'grid-move 20s linear infinite'
        }} />
      </div>

      {/* Floating Particles */}
      <div className="absolute top-1/4 left-1/4 w-2 h-2 bg-[#ff6b35] rounded-full opacity-30 animate-particle-1" />
      <div className="absolute top-1/3 right-1/4 w-1.5 h-1.5 bg-[#4ecdc4] rounded-full opacity-30 animate-particle-2" />
      <div className="absolute bottom-1/3 left-1/3 w-2 h-2 bg-[#aa96da] rounded-full opacity-30 animate-particle-3" />
      <div className="absolute top-2/3 right-1/3 w-1 h-1 bg-[#95e1d3] rounded-full opacity-30 animate-particle-4" />

      {/* Scanning Lines */}
      <div className="absolute inset-0 overflow-hidden">
        <div className="absolute w-full h-0.5 bg-gradient-to-r from-transparent via-[#4ecdc4]/20 to-transparent animate-scan-line" />
      </div>
    </div>
  );
}
