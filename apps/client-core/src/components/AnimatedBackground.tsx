/*
 * Circuit Forge Design System — AnimatedBackground
 * Renders a dark void background with animated circuit traces,
 * floating particles, and a subtle grid overlay.
 */
import { useEffect, useRef } from "react";

interface Particle {
  x: number;
  y: number;
  vx: number;
  vy: number;
  size: number;
  opacity: number;
  life: number;
  maxLife: number;
}

interface TraceLine {
  points: { x: number; y: number }[];
  progress: number;
  speed: number;
  opacity: number;
}

export default function AnimatedBackground() {
  const canvasRef = useRef<HTMLCanvasElement>(null);

  useEffect(() => {
    const canvas = canvasRef.current;
    if (!canvas) return;

    const ctx = canvas.getContext("2d");
    if (!ctx) return;

    let animId: number;
    let particles: Particle[] = [];
    let traces: TraceLine[] = [];

    const resize = () => {
      canvas.width = window.innerWidth;
      canvas.height = window.innerHeight;
    };
    resize();
    window.addEventListener("resize", resize);

    // Initialize particles
    const initParticles = () => {
      particles = [];
      for (let i = 0; i < 40; i++) {
        particles.push({
          x: Math.random() * canvas.width,
          y: Math.random() * canvas.height,
          vx: (Math.random() - 0.5) * 0.3,
          vy: (Math.random() - 0.5) * 0.3,
          size: Math.random() * 2 + 0.5,
          opacity: Math.random() * 0.5 + 0.1,
          life: Math.random() * 200,
          maxLife: 200 + Math.random() * 200,
        });
      }
    };

    // Initialize circuit traces
    const initTraces = () => {
      traces = [];
      for (let i = 0; i < 6; i++) {
        const points: { x: number; y: number }[] = [];
        let x = Math.random() * canvas.width;
        let y = Math.random() * canvas.height;
        const segments = 4 + Math.floor(Math.random() * 6);
        for (let j = 0; j < segments; j++) {
          points.push({ x, y });
          if (Math.random() > 0.5) {
            x += (Math.random() - 0.5) * 200;
          } else {
            y += (Math.random() - 0.5) * 200;
          }
        }
        traces.push({
          points,
          progress: Math.random(),
          speed: 0.001 + Math.random() * 0.002,
          opacity: 0.1 + Math.random() * 0.2,
        });
      }
    };

    initParticles();
    initTraces();

    const drawGrid = () => {
      ctx.strokeStyle = "rgba(0, 255, 200, 0.02)";
      ctx.lineWidth = 0.5;
      const gridSize = 60;
      for (let x = 0; x < canvas.width; x += gridSize) {
        ctx.beginPath();
        ctx.moveTo(x, 0);
        ctx.lineTo(x, canvas.height);
        ctx.stroke();
      }
      for (let y = 0; y < canvas.height; y += gridSize) {
        ctx.beginPath();
        ctx.moveTo(0, y);
        ctx.lineTo(canvas.width, y);
        ctx.stroke();
      }
    };

    const drawParticles = () => {
      particles.forEach((p) => {
        p.x += p.vx;
        p.y += p.vy;
        p.life++;

        if (p.life > p.maxLife) {
          p.x = Math.random() * canvas.width;
          p.y = Math.random() * canvas.height;
          p.life = 0;
        }

        if (p.x < 0) p.x = canvas.width;
        if (p.x > canvas.width) p.x = 0;
        if (p.y < 0) p.y = canvas.height;
        if (p.y > canvas.height) p.y = 0;

        const fadeIn = Math.min(p.life / 30, 1);
        const fadeOut = Math.max(0, 1 - (p.life - p.maxLife + 30) / 30);
        const alpha = p.opacity * fadeIn * fadeOut;

        ctx.beginPath();
        ctx.arc(p.x, p.y, p.size, 0, Math.PI * 2);
        ctx.fillStyle = `rgba(0, 255, 200, ${alpha})`;
        ctx.fill();

        // Glow
        ctx.beginPath();
        ctx.arc(p.x, p.y, p.size * 3, 0, Math.PI * 2);
        ctx.fillStyle = `rgba(0, 255, 200, ${alpha * 0.15})`;
        ctx.fill();
      });
    };

    const drawTraces = () => {
      traces.forEach((trace) => {
        trace.progress += trace.speed;
        if (trace.progress > 1) trace.progress = 0;

        const totalPoints = trace.points.length;
        const currentIdx = Math.floor(trace.progress * (totalPoints - 1));
        const nextIdx = Math.min(currentIdx + 1, totalPoints - 1);

        ctx.strokeStyle = `rgba(0, 255, 200, ${trace.opacity})`;
        ctx.lineWidth = 1;
        ctx.setLineDash([4, 8]);

        ctx.beginPath();
        for (let i = 0; i < totalPoints - 1; i++) {
          const p1 = trace.points[i];
          const p2 = trace.points[i + 1];
          if (i === 0) ctx.moveTo(p1.x, p1.y);
          ctx.lineTo(p2.x, p2.y);
        }
        ctx.stroke();
        ctx.setLineDash([]);

        // Draw moving dot
        if (currentIdx < totalPoints - 1) {
          const p1 = trace.points[currentIdx];
          const p2 = trace.points[nextIdx];
          const segProgress = (trace.progress * (totalPoints - 1)) % 1;
          const dotX = p1.x + (p2.x - p1.x) * segProgress;
          const dotY = p1.y + (p2.y - p1.y) * segProgress;

          ctx.beginPath();
          ctx.arc(dotX, dotY, 3, 0, Math.PI * 2);
          ctx.fillStyle = `rgba(0, 255, 200, ${trace.opacity * 3})`;
          ctx.fill();

          ctx.beginPath();
          ctx.arc(dotX, dotY, 8, 0, Math.PI * 2);
          ctx.fillStyle = `rgba(0, 255, 200, ${trace.opacity * 0.5})`;
          ctx.fill();
        }

        // Draw junction dots
        trace.points.forEach((p) => {
          ctx.beginPath();
          ctx.arc(p.x, p.y, 2, 0, Math.PI * 2);
          ctx.fillStyle = `rgba(0, 255, 200, ${trace.opacity * 0.8})`;
          ctx.fill();
        });
      });
    };

    const animate = () => {
      ctx.clearRect(0, 0, canvas.width, canvas.height);
      drawGrid();
      drawTraces();
      drawParticles();
      animId = requestAnimationFrame(animate);
    };

    animate();

    return () => {
      cancelAnimationFrame(animId);
      window.removeEventListener("resize", resize);
    };
  }, []);

  return (
    <canvas
      ref={canvasRef}
      className="fixed inset-0 pointer-events-none"
      style={{ zIndex: 0 }}
    />
  );
}
