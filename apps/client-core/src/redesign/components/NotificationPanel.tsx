import { X, CheckCircle, AlertCircle, Info, Trash2 } from 'lucide-react';

interface NotificationPanelProps {
  onClose: () => void;
  onClearAll: () => void;
}

export default function NotificationPanel({ onClose, onClearAll }: NotificationPanelProps) {
  return (
    <div className="fixed right-0 top-0 bottom-0 w-96 bg-[#161b22] border-l-2 border-[#30363d] shadow-2xl flex flex-col z-50">
      {/* Header */}
      <div className="border-b-2 border-[#30363d] p-4 flex items-center justify-between">
        <h2 className="text-lg font-semibold text-[#e6edf3]">Notifications</h2>
        <div className="flex items-center gap-2">
          <button
            onClick={onClearAll}
            className="px-3 py-1.5 text-xs bg-[#21262d] border border-[#30363d] text-[#8b949e] hover:border-[#4dffd2] hover:text-[#4dffd2] transition-all"
          >
            Clear All
          </button>
          <button
            onClick={onClose}
            className="w-8 h-8 flex items-center justify-center hover:bg-[#21262d] transition-colors"
          >
            <X className="w-5 h-5 text-[#8b949e]" />
          </button>
        </div>
      </div>

      {/* Notifications List */}
      <div className="flex-1 overflow-auto p-4 space-y-3">
        <NotificationItem
          type="success"
          title="Lobby created successfully"
          message="Your lobby 'Weekend Randomizer' is now active and waiting for players."
          time="5 minutes ago"
        />
        <NotificationItem
          type="info"
          title="Player joined your lobby"
          message="Raifu has joined Weekend Randomizer"
          time="10 minutes ago"
        />
        <NotificationItem
          type="error"
          title="Config validation failed"
          message="Seed config could not be saved. Entrance Shuffle Seed has an invalid value."
          details="entrance_shuffle_seed: value does not match option type enum"
          time="2 hours ago"
        />
      </div>
    </div>
  );
}

function NotificationItem({ type, title, message, details, time }: {
  type: 'success' | 'error' | 'info';
  title: string;
  message: string;
  details?: string;
  time: string;
}) {
  const config = {
    success: { icon: <CheckCircle className="w-5 h-5" />, color: 'text-[#4dffd2]', border: 'border-[#4dffd2]' },
    error: { icon: <AlertCircle className="w-5 h-5" />, color: 'text-[#f85149]', border: 'border-[#f85149]' },
    info: { icon: <Info className="w-5 h-5" />, color: 'text-[#58a6ff]', border: 'border-[#58a6ff]' }
  }[type];

  return (
    <div className={`p-4 border-l-2 ${config.border} bg-[#0d1117] hover:bg-[#161b22] transition-colors`}>
      <div className="flex gap-3">
        <div className={config.color}>{config.icon}</div>
        <div className="flex-1 min-w-0">
          <div className="flex items-start justify-between gap-2 mb-1">
            <h4 className="font-medium text-[#e6edf3]">{title}</h4>
            <button className="flex-shrink-0 text-[#8b949e] hover:text-[#f85149] transition-colors">
              <Trash2 className="w-4 h-4" />
            </button>
          </div>
          <p className="text-sm text-[#8b949e] mb-2">{message}</p>
          {details && (
            <details className="text-xs">
              <summary className="text-[#8b949e] cursor-pointer hover:text-[#4dffd2] mb-1">
                Show details
              </summary>
              <div className="p-2 bg-[#161b22] border border-[#30363d] text-[#8b949e] font-mono">
                {details}
              </div>
            </details>
          )}
          <div className="text-xs text-[#8b949e] mt-2">{time}</div>
        </div>
      </div>
    </div>
  );
}
