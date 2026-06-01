import { useState } from 'react';
import { X, Sparkles, Settings, ChevronDown, Search } from 'lucide-react';

interface AddSeedConfigModalProps {
  onClose: () => void;
}

export default function AddSeedConfigModal({ onClose }: AddSeedConfigModalProps) {
  const [activeTab, setActiveTab] = useState<'easy' | 'advanced'>('easy');
  const [searchQuery, setSearchQuery] = useState('');

  return (
    <div className="fixed inset-0 bg-black/80 flex items-center justify-center z-50 p-8">
      <div className="w-full max-w-4xl h-[80vh] bg-[#161b22] border-2 border-[#4dffd2] flex flex-col shadow-[0_0_40px_rgba(77,255,210,0.2)]">
        {/* Header */}
        <div className="border-b-2 border-[#30363d] p-6 flex items-center justify-between">
          <div>
            <h2 className="text-xl font-semibold text-[#e6edf3] mb-1">Add Seed Config</h2>
            <p className="text-sm text-[#8b949e]">The Legend of Zelda: A Link to the Past</p>
          </div>
          <button
            onClick={onClose}
            className="w-10 h-10 clip-octagon-sm bg-[#21262d] hover:bg-[#f85149] border-2 border-[#30363d] hover:border-[#f85149] transition-all flex items-center justify-center"
          >
            <X className="w-5 h-5 text-[#e6edf3]" />
          </button>
        </div>

        {/* Tabs */}
        <div className="border-b-2 border-[#30363d] flex">
          <button
            onClick={() => setActiveTab('easy')}
            className={`
              flex-1 px-6 py-4 font-medium transition-all flex items-center justify-center gap-2
              ${activeTab === 'easy'
                ? 'bg-[#21262d] border-b-2 border-[#4dffd2] text-[#4dffd2]'
                : 'text-[#8b949e] hover:text-[#e6edf3]'
              }
            `}
          >
            <Sparkles className="w-5 h-5" />
            Easy (Pulse Assistant)
          </button>
          <button
            onClick={() => setActiveTab('advanced')}
            className={`
              flex-1 px-6 py-4 font-medium transition-all flex items-center justify-center gap-2
              ${activeTab === 'advanced'
                ? 'bg-[#21262d] border-b-2 border-[#4dffd2] text-[#4dffd2]'
                : 'text-[#8b949e] hover:text-[#e6edf3]'
              }
            `}
          >
            <Settings className="w-5 h-5" />
            Advanced
          </button>
        </div>

        {/* Content */}
        <div className="flex-1 overflow-auto p-6">
          {activeTab === 'easy' && <EasyTab />}
          {activeTab === 'advanced' && <AdvancedTab searchQuery={searchQuery} setSearchQuery={setSearchQuery} />}
        </div>

        {/* Footer */}
        <div className="border-t-2 border-[#30363d] p-6 flex justify-between">
          <button
            onClick={onClose}
            className="px-6 py-3 bg-[#21262d] border-2 border-[#30363d] text-[#8b949e] hover:border-[#4dffd2] hover:text-[#e6edf3] transition-all"
          >
            Cancel
          </button>
          <button className="px-8 py-3 bg-[#4dffd2] border-2 border-[#4dffd2] text-[#0d1117] font-medium hover:shadow-[0_0_20px_rgba(77,255,210,0.3)] transition-all">
            Save Config
          </button>
        </div>
      </div>
    </div>
  );
}

function EasyTab() {
  return (
    <div className="max-w-2xl mx-auto space-y-6">
      {/* Pulse Assistant Header */}
      <div className="p-6 border-2 border-[#4dffd2] bg-gradient-to-br from-[#4dffd2]/10 to-transparent">
        <div className="flex items-start gap-4">
          <div className="w-12 h-12 clip-hex bg-gradient-to-br from-[#4dffd2] to-[#58a6ff] flex items-center justify-center flex-shrink-0">
            <Sparkles className="w-6 h-6 text-white" />
          </div>
          <div>
            <h3 className="text-lg font-semibold text-[#4dffd2] mb-2">Pulse Configuration Assistant</h3>
            <p className="text-sm text-[#8b949e]">
              Answer a few simple questions and Pulse will create the perfect seed configuration for you
            </p>
          </div>
        </div>
      </div>

      {/* Questions */}
      <QuestionCard
        question="What difficulty level would you like?"
        options={['Beginner', 'Standard', 'Intermediate', 'Expert']}
      />
      <QuestionCard
        question="How long should the run take?"
        options={['Short (2-3 hours)', 'Medium (4-6 hours)', 'Long (7+ hours)', 'No preference']}
      />
      <QuestionCard
        question="Do you want entrance shuffle?"
        options={['No', 'Simple shuffle', 'Full shuffle']}
      />
      <QuestionCard
        question="Starting equipment comfort level?"
        options={['Start with basics', 'Standard start', 'Randomized start', 'Hard mode']}
      />

      <div className="p-4 bg-[#0d1117] border-2 border-[#30363d] text-sm text-[#8b949e]">
        <div className="flex items-center gap-2 mb-2">
          <Sparkles className="w-4 h-4 text-[#4dffd2]" />
          <span className="text-[#4dffd2] font-medium">Pulse Suggestion:</span>
        </div>
        Based on your selections, this config will be balanced for intermediate players with moderate shuffle and a 4-6 hour estimated completion time.
      </div>
    </div>
  );
}

function QuestionCard({ question, options }: { question: string; options: string[] }) {
  const [selected, setSelected] = useState(0);

  return (
    <div className="p-6 border-2 border-[#30363d] bg-[#0d1117]">
      <h4 className="font-medium text-[#e6edf3] mb-4">{question}</h4>
      <div className="grid grid-cols-2 gap-3">
        {options.map((option, index) => (
          <button
            key={index}
            onClick={() => setSelected(index)}
            className={`
              px-4 py-3 border-2 text-sm transition-all
              ${selected === index
                ? 'bg-[#4dffd2]/10 border-[#4dffd2] text-[#4dffd2]'
                : 'bg-[#161b22] border-[#30363d] text-[#8b949e] hover:border-[#4dffd2]'
              }
            `}
          >
            {option}
          </button>
        ))}
      </div>
    </div>
  );
}

function AdvancedTab({ searchQuery, setSearchQuery }: {
  searchQuery: string;
  setSearchQuery: (q: string) => void;
}) {
  return (
    <div className="space-y-6">
      {/* Search */}
      <div className="relative">
        <Search className="absolute left-3 top-1/2 -translate-y-1/2 w-5 h-5 text-[#8b949e]" />
        <input
          type="text"
          value={searchQuery}
          onChange={(e) => setSearchQuery(e.target.value)}
          placeholder="Search options..."
          className="w-full pl-11 pr-4 py-3 bg-[#0d1117] border-2 border-[#30363d] text-[#e6edf3] placeholder:text-[#8b949e] focus:border-[#4dffd2] outline-none transition-colors"
        />
      </div>

      {/* Config Name */}
      <div className="p-6 border-2 border-[#30363d] bg-[#0d1117]">
        <label className="block text-sm font-medium text-[#e6edf3] mb-2">Configuration Name</label>
        <input
          type="text"
          placeholder="My Custom Config"
          className="w-full px-4 py-3 bg-[#161b22] border-2 border-[#30363d] text-[#e6edf3] placeholder:text-[#8b949e] focus:border-[#4dffd2] outline-none transition-colors"
        />
      </div>

      {/* Option Groups */}
      <OptionGroup title="Gameplay">
        <FormField label="Logic" type="dropdown" options={['NoGlitches', 'OverworldGlitches', 'MajorGlitches']} />
        <FormField label="Goal" type="dropdown" options={['GanonsTower', 'AllDungeons', 'Pedestal']} />
        <FormField label="Mode" type="dropdown" options={['Standard', 'Open', 'Inverted']} />
        <FormField label="Entrance Shuffle" type="toggle" />
      </OptionGroup>

      <OptionGroup title="Items">
        <FormField label="Item Placement" type="dropdown" options={['Advanced', 'Basic', 'Minimal']} />
        <FormField label="Item Pool" type="dropdown" options={['Normal', 'Hard', 'Expert']} />
        <FormField label="Progressive Items" type="toggle" />
      </OptionGroup>

      <OptionGroup title="Difficulty">
        <FormField label="Difficulty" type="slider" value="50" />
        <FormField label="Enemy Shuffle" type="toggle" />
        <FormField label="Boss Shuffle" type="dropdown" options={['None', 'Simple', 'Full', 'Chaos']} />
      </OptionGroup>
    </div>
  );
}

function OptionGroup({ title, children }: { title: string; children: React.ReactNode }) {
  const [expanded, setExpanded] = useState(true);

  return (
    <div className="border-2 border-[#30363d] bg-[#0d1117]">
      <button
        onClick={() => setExpanded(!expanded)}
        className="w-full p-4 flex items-center justify-between hover:bg-[#161b22] transition-colors"
      >
        <span className="font-medium text-[#e6edf3]">{title}</span>
        <ChevronDown className={`w-5 h-5 text-[#8b949e] transition-transform ${expanded ? 'rotate-180' : ''}`} />
      </button>
      {expanded && (
        <div className="p-6 pt-0 space-y-4">
          {children}
        </div>
      )}
    </div>
  );
}

function FormField({ label, type, options, value }: {
  label: string;
  type: 'dropdown' | 'toggle' | 'slider';
  options?: string[];
  value?: string;
}) {
  const [enabled, setEnabled] = useState(false);

  if (type === 'toggle') {
    return (
      <div className="flex items-center justify-between">
        <span className="text-sm text-[#e6edf3]">{label}</span>
        <button
          onClick={() => setEnabled(!enabled)}
          className={`
            relative w-12 h-6 border-2 transition-all
            ${enabled ? 'bg-[#4dffd2] border-[#4dffd2]' : 'bg-[#21262d] border-[#30363d]'}
          `}
        >
          <div
            className={`
              absolute top-0.5 w-4 h-4 bg-[#0d1117] transition-all
              ${enabled ? 'right-0.5' : 'left-0.5'}
            `}
          />
        </button>
      </div>
    );
  }

  if (type === 'slider') {
    return (
      <div className="space-y-2">
        <div className="flex items-center justify-between">
          <span className="text-sm text-[#e6edf3]">{label}</span>
          <span className="text-sm text-[#4dffd2]">{value}%</span>
        </div>
        <div className="h-2 bg-[#21262d] border-2 border-[#30363d] relative">
          <div className="absolute left-0 top-0 bottom-0 w-1/2 bg-[#4dffd2]" />
        </div>
      </div>
    );
  }

  return (
    <div className="space-y-2">
      <label className="text-sm text-[#e6edf3]">{label}</label>
      <select className="w-full px-4 py-2 bg-[#161b22] border-2 border-[#30363d] text-[#e6edf3] focus:border-[#4dffd2] outline-none transition-colors">
        {options?.map((option) => (
          <option key={option} value={option}>{option}</option>
        ))}
      </select>
    </div>
  );
}
