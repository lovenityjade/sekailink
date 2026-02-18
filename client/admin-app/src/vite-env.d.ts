/// <reference types="vite/client" />

declare const __APP_VERSION__: string;
import type React from "react";

declare global {
  interface Window {
    sekailinkAdmin?: {
      openExternal?: (url: string) => Promise<{ ok: boolean; error?: string }>;
      showItemInFolder?: (targetPath: string) => Promise<{ ok: boolean; error?: string }>;
      getVersion?: () => Promise<{ ok: boolean; version?: string; error?: string }>;
      onAuthCallback?: (handler: (url: string) => void) => () => void;
    };
  }

  namespace JSX {
    interface IntrinsicElements {
      "service-health-widget": React.DetailedHTMLProps<React.HTMLAttributes<HTMLElement>, HTMLElement> & {
        snapshot?: string;
      };
    }
  }
}
