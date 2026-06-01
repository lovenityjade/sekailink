import { X, Edit, Trash2, CheckCircle, AlertCircle, Sparkles, Settings as SettingsIcon, Search } from 'lucide-react';
import { useState } from 'react';

interface ManageSeedConfigsModalProps {
  onClose: () => void;
}

export default function ManageSeedConfigsModal({ onClose }: ManageSeedConfigsModalProps) {
  const [searchQuery, setSearchQuery] = useState('');

  return (
    <div className="fixed inset-0 bg-black/80 flex items-center justify-center z-50 p-8">
      <div className="w-full max-w-5xl h-[80vh] bg-[#161b22] border-2 border-[#4dffd2] flex flex-col shadow-[0_0_40px_rgba(77,255,210,0.2)]">
        {/* Header */}
        <div className="border-b-2 border-[#30363d] p-6 flex items-center justify-between">
          <div>
            <h2 className="text-xl font-semibold text-[#e6edf3] mb-1">Manage Seed Configs</h2>
            <p className="text-sm text-[#8b949e]">The Legend of Zelda: A Link to the Past</p>
          </div>
          <button
            onClick={onClose}
            className="w-10 h-10 clip-octagon-sm bg-[#21262d] hover:bg-[#f85149] border-2 border-[#30363d] hover:border-[#f85149] transition-all flex items-center justify-center"
          >
            <X className="w-5 h-5 text-[#e6edf3]" />
          </button>
        </div>

        {/* Search */}
        <div className="border-b-2 border-[#30363d] p-6">
          <div className="relative">
            <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-5 h-5 text-[#8b949e]" />
            <input
              type="text"
              value={searchQuery}
              onChange={(e) => setSearchQuery(e.target.value)}
              placeholder="Search configurations..."
              className="w-full pl-11 pr-4 py-3 bg-[#0d1117] border-2 border-[#30363d] text-[#e6edf3] placeholder:text-[#8b949e] focus:border-[#4dffd2] outline-none transition-colors"
            />
          </div>
        </div>

        {/* Config List */}
        <div className="flex-1 overflow-auto p-6 space-y-3">
          <ConfigListItem
            name="Standard Run"
            source="Easy"
            sourceIcon={<Sparkles className="w-4 h-4" />}
            status="valid"
            updated="2 days ago"
            description="Beginner-friendly configuration with standard difficulty"
          />
          <ConfigListItem
            name="Hard Mode Expert"
            source="Advanced"
            sourceIcon={<SettingsIcon className="w-4 h-4" />}
            status="valid"
            updated="1 week ago"
            description="Expert settings with full entrance shuffle and hard item placement"
          />
          <ConfigListItem
            name="Beginner Friendly"
            source="Pulse"
            sourceIcon={<Sparkles className="w-4 h-4" />}
            status="valid"
            updated="2 weeks ago"
            description="Pulse-created config for new players with guided difficulty"
          />
          <ConfigListItem
            name="Quick Test Run"
            source="Advanced"
            sourceIcon={<SettingsIcon className="w-4 h-4" />}
            status="draft"
            updated="3 weeks ago"
            description="Incomplete configuration - needs validation"
          />
        </div>

        {/* Footer */}
        <div className="border-t-2 border-[#30363d] p-6 flex justify-end">
          <button
            onClick={onClose}
            className="px-6 py-3 bg-[#21262d] border-2 border-[#30363d] text-[#8b949e] hover:border-[#4dffd2] hover:text-[#e6edf3] transition-all"
          >
            Close
          </button>
        </div>
      </div>
    </div>
  );
}

function ConfigListItem({ name, source, sourceIcon, status, updated, description }: {
  name: string;
  source: string;
  sourceIcon: React.ReactNode;
  status: 'valid' | 'draft' | 'error';
  updated: string;
  description: string;
}) {
  const [showDeleteConfirm, setShowDeleteConfirm] = useState(false);

  const statusConfig = {
    valid: { icon: <CheckCircle className="w-5 h-5" />, color: 'text-[#4dffd2]' },
    draft: { icon: <AlertCircle className="w-5 h-5" />, color: 'text-[#f69d50]' },
    error: { icon: <AlertCircle className="w-5 h-5" />, color: 'text-[#f85149]' }
  }[status];

  return (
    <div className="border-2 border-[#30363d] bg-[#0d1117] hover:border-[#4dffd2] transition-all">
      <div className="p-6">
        <div className="flex items-start gap-4">
          {/* Status Indicator */}
          <div className={`${statusConfig.color} pt-1`}>
            {statusConfig.icon}
          </div>

          {/* Content */}
          <div className="flex-1 min-w-0">
            <div className="flex items-start justify-between gap-4 mb-2">
              <div>
                <h3 className="text-lg font-semibold text-[#e6edf3] mb-1">{name}</h3>
                <p className="text-sm text-[#8b949e]">{description}</p>
              </div>
              <div className="flex items-center gap-2 flex-shrink-0">
                <button className="px-4 py-2 bg-[#21262d] border-2 border-[#30363d] text-[#8b949e] hover:border-[#4dffd2] hover:text-[#4dffd2] transition-all flex items-center gap-2">
                  <Edit className="w-4 h-4" />
                  <span className="text-sm">Edit</span>
                </button>
                <button
                  onClick={() => setShowDeleteConfirm(true)}
                  className="p-2 bg-[#21262d] border-2 border-[#30363d] text-[#8b949e] hover:border-[#f85149] hover:text-[#f85149] transition-all"
                >
                  <Trash2 className="w-4 h-4" />
                </button>
              </div>
            </div>

            {/* Meta Info */}
            <div className="flex items-center gap-4 text-xs text-[#8b949e]">
              <div className="flex items-center gap-1.5 px-2 py-1 bg-[#161b22] border border-[#30363d]">
                {sourceIcon}
                <span>{source}</span>
              </div>
              <div className="flex items-center gap-1.5">
                <span className="uppercase">{status}</span>
              </div>
              <div className="flex items-center gap-1.5">
                <span>Updated {updated}</span>
              </div>
            </div>
          </div>
        </div>
      </div>

      {/* Delete Confirmation */}
      {showDeleteConfirm && (
        <div className="border-t-2 border-[#f85149] bg-[#f85149]/10 p-4 flex items-center justify-between">
          <div className="flex items-center gap-3">
            <AlertCircle className="w-5 h-5 text-[#f85149]" />
            <span className="text-sm text-[#e6edf3]">
              Are you sure you want to delete this configuration? This cannot be undone.
            </span>
          </div>
          <div className="flex gap-2">
            <button
              onClick={() => setShowDeleteConfirm(false)}
              className="px-4 py-2 bg-[#21262d] border-2 border-[#30363d] text-[#8b949e] hover:border-[#4dffd2] hover:text-[#e6edf3] transition-all text-sm"
            >
              Cancel
            </button>
            <button className="px-4 py-2 bg-[#f85149] border-2 border-[#f85149] text-white hover:shadow-[0_0_20px_rgba(248,81,73,0.3)] transition-all text-sm">
              Delete
            </button>
          </div>
        </div>
      )}
    </div>
  );
}
