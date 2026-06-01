import { AlertTriangle, Loader2, X } from 'lucide-react';

const errorCodeFrom = (code?: string, message?: string) => {
  const raw = String(code || message || 'unknown_error').trim();
  const normalized = raw
    .replace(/([a-z])([A-Z])/g, '$1_$2')
    .replace(/[^A-Za-z0-9]+/g, '_')
    .replace(/^_+|_+$/g, '')
    .toUpperCase();
  return normalized.slice(0, 64) || 'UNKNOWN_ERROR';
};

export function LoadingModal({ title, message }: { title: string; message?: string }) {
  return (
    <div className="fixed inset-0 z-[80] flex items-center justify-center bg-black/80 backdrop-blur-sm p-8">
      <div className="w-full max-w-sm bg-[#161b22] rounded-xl border-2 border-[#4ecdc4] shadow-2xl card-float p-8 text-center">
        <div className="w-16 h-16 mx-auto mb-5 rounded-full bg-[#4ecdc4]/10 border border-[#4ecdc4]/40 flex items-center justify-center">
          <Loader2 className="w-8 h-8 text-[#4ecdc4] animate-spin" />
        </div>
        <h2 className="text-2xl font-bold mb-2">{title}</h2>
        {message && <p className="text-sm text-[#8e8f94] leading-relaxed">{message}</p>}
      </div>
    </div>
  );
}

export function ErrorModal({
  title = 'Something went wrong',
  message,
  code,
  onClose,
}: {
  title?: string;
  message: string;
  code?: string;
  onClose: () => void;
}) {
  const errorCode = errorCodeFrom(code, message);
  return (
    <div className="fixed inset-0 z-[90] flex items-center justify-center bg-black/80 backdrop-blur-sm p-8">
      <div className="w-full max-w-lg bg-[#161b22] rounded-xl border-2 border-[#f85149] shadow-2xl card-float overflow-hidden">
        <div className="p-5 border-b-2 border-[#2a2b30] bg-gradient-to-r from-[#1c1d22] to-[#161b22] flex items-center justify-between">
          <div className="flex items-center gap-3">
            <div className="w-10 h-10 rounded-lg bg-[#f85149]/15 border border-[#f85149]/40 flex items-center justify-center">
              <AlertTriangle className="w-5 h-5 text-[#f85149]" />
            </div>
            <div>
              <h2 className="font-bold text-lg">{title}</h2>
              <p className="text-xs text-[#f85149] font-mono mt-0.5">{errorCode}</p>
            </div>
          </div>
          <button
            onClick={onClose}
            className="w-9 h-9 rounded-lg bg-[#2a2b30] hover:bg-[#f85149] transition-colors flex items-center justify-center group"
            aria-label="Close error"
          >
            <X className="w-4 h-4 text-[#8e8f94] group-hover:text-white" />
          </button>
        </div>
        <div className="p-6">
          <p className="text-sm text-[#e6edf3] leading-relaxed">{message}</p>
          <button
            onClick={onClose}
            className="mt-6 w-full py-3 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg font-medium transition-colors"
          >
            Close
          </button>
        </div>
      </div>
    </div>
  );
}
