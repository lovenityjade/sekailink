import { Minus, Square, X } from 'lucide-react';
import { ImageWithFallback } from './figma/ImageWithFallback';
import logoImage from '../../imports/sekailink-logo-image.png';
import logoText from '../../imports/sekailink-logo-text.png';

export default function TitleBar() {
  const handleMinimize = () => {
    window.sekailink?.windowMinimize?.();
  };

  const handleMaximize = () => {
    window.sekailink?.windowToggleMaximize?.();
  };

  const handleClose = () => {
    window.sekailink?.windowClose?.();
  };

  return (
    <div className="h-8 bg-[#0e0f13] flex items-center justify-between px-3 select-none border-b border-[#2a2b30]" style={{ WebkitAppRegion: 'drag' } as any}>
      {/* Logo */}
      <div className="flex items-center gap-2">
        <ImageWithFallback
          src={logoImage}
          alt="SekaiLink"
          className="h-5 w-auto"
          style={{ WebkitAppRegion: 'no-drag' } as any}
        />
        <ImageWithFallback
          src={logoText}
          alt="SekaiLink"
          className="h-4 w-auto"
          style={{ WebkitAppRegion: 'no-drag' } as any}
        />
      </div>

      {/* Window Controls */}
      <div className="flex items-center gap-1" style={{ WebkitAppRegion: 'no-drag' } as any}>
        <button
          type="button"
          onClick={handleMinimize}
          className="w-8 h-8 flex items-center justify-center hover:bg-[#2a2b30] transition-colors rounded"
          aria-label="Minimize"
        >
          <Minus className="w-4 h-4 text-[#8e8f94]" />
        </button>
        <button
          type="button"
          onClick={handleMaximize}
          className="w-8 h-8 flex items-center justify-center hover:bg-[#2a2b30] transition-colors rounded"
          aria-label="Maximize"
        >
          <Square className="w-3.5 h-3.5 text-[#8e8f94]" />
        </button>
        <button
          type="button"
          onClick={handleClose}
          className="w-8 h-8 flex items-center justify-center hover:bg-[#f85149] transition-colors rounded group"
          aria-label="Close"
        >
          <X className="w-4 h-4 text-[#8e8f94] group-hover:text-white" />
        </button>
      </div>
    </div>
  );
}
