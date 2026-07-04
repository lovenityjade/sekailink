/*
 * Circuit Forge Design System — ChamferedPanel
 * A panel with chamfered (clipped) corners, animated rotating light border,
 * and power indicator dots at corners.
 *
 * The border uses a spinning conic-gradient that creates the illusion of
 * a bright teal light orbiting around the panel edges. The gradient has
 * a sharp bright head and a softer trailing glow.
 */
import { motion } from "framer-motion";
import { ReactNode } from "react";

interface ChamferedPanelProps {
  children: ReactNode;
  className?: string;
  title?: string;
  titleRight?: ReactNode;
  delay?: number;
  noPadding?: boolean;
}

export default function ChamferedPanel({
  children,
  className = "",
  title,
  titleRight,
  delay = 0,
  noPadding = false,
}: ChamferedPanelProps) {

  return (
    <motion.div
      initial={{ opacity: 0, scale: 0.98 }}
      animate={{ opacity: 1, scale: 1 }}
      transition={{
        duration: 0.4,
        delay,
        ease: [0.16, 1, 0.3, 1],
      }}
      className={`relative flex flex-col ${className}`}
    >
      {/* ====== Animated rotating border ====== */}
      <div
        className="absolute inset-0 panel-chamfer overflow-hidden"
        style={{ padding: "1.5px" }}
      >
        {/* Static glowing border */}
        <div
          className="absolute inset-0"
          style={{
            background:
              "linear-gradient(135deg, rgba(0,255,200,0.10) 0%, rgba(0,255,200,0.03) 40%, rgba(0,255,200,0.03) 60%, rgba(0,255,200,0.08) 100%)",
          }}
        />

        {/* Inner cutout — the actual panel background that "punches through" */}
        <div
          className="relative w-full h-full panel-chamfer"
          style={{ background: "rgba(13, 17, 23, 0.60)" }}
        />
      </div>

      {/* Outer glow halo that pulses subtly */}
      <div
        className="absolute -inset-[1px] pointer-events-none panel-chamfer"
        style={{
          boxShadow:
            "0 0 20px rgba(0,255,200,0.05), 0 0 40px rgba(0,255,200,0.02)",
        }}
      />

      {/* ====== Content ====== */}
      <div
        className="relative panel-chamfer overflow-hidden flex flex-col flex-1 min-h-0"
        style={{ background: "rgba(13, 17, 23, 0.60)" }}
      >
        {/* Power dots */}
        <div className="absolute top-2 left-2 power-dot rounded-none" />
        <div className="absolute top-2 right-2 power-dot rounded-none" />

        {/* Title bar */}
        {title && (
          <div className="flex items-center justify-between px-4 py-2.5 border-b border-teal/15 shrink-0">
            <h3 className="font-header text-sm text-teal tracking-wider text-glow">
              {title}
            </h3>
            {titleRight && <div>{titleRight}</div>}
          </div>
        )}

        {/* Body */}
        <div className={`flex-1 min-h-0 flex flex-col ${noPadding ? "" : "p-4"}`}>{children}</div>
      </div>

    </motion.div>
  );
}
