import { useCallback, useEffect, useState } from "react";

interface SfxApi {
  play?: (name: string, volume?: number) => void;
  toggleMuted?: () => void;
  getMuted?: () => boolean;
  setMuted?: (value: boolean) => void;
  getBaseVolume?: () => number;
  setBaseVolume?: (value: number) => void;
  preload?: () => void;
}

declare global {
  interface Window {
    SKL_SFX?: SfxApi;
  }
}

export const useSfx = () => {
  const [muted, setMuted] = useState(false);

  useEffect(() => {
    const api = window.SKL_SFX;
    if (!api) return;
    api.preload?.();
    setMuted(Boolean(api.getMuted?.()));
  }, []);

  const toggleMuted = useCallback(() => {
    const api = window.SKL_SFX;
    if (!api) return;
    api.toggleMuted?.();
    setMuted(Boolean(api.getMuted?.()));
  }, []);

  const play = useCallback((name: string, volume?: number) => {
    const api = window.SKL_SFX;
    api?.play?.(name, volume);
  }, []);

  return { muted, toggleMuted, play };
};
