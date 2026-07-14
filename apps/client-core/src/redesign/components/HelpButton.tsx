import { HelpCircle, Bug, Info, X, Book, FileText, Send } from 'lucide-react';
import { useState } from 'react';
import { ImageWithFallback } from './figma/ImageWithFallback';
import logoImage from '../../imports/sekailink-logo-image.png';
import logoText from '../../imports/sekailink-logo-text.png';

export default function HelpButton() {
  const [showMenu, setShowMenu] = useState(false);
  const [showHelpModal, setShowHelpModal] = useState(false);
  const [showBugModal, setShowBugModal] = useState(false);
  const [showAboutModal, setShowAboutModal] = useState(false);
  const [helpTab, setHelpTab] = useState('getting-started');

  // Bug report state
  const [bugTitle, setBugTitle] = useState('');
  const [bugDescription, setBugDescription] = useState('');
  const [bugCategory, setBugCategory] = useState('general');
  const [bugSeverity, setBugSeverity] = useState('medium');

  const handleBugSubmit = () => {
    console.log('Bug report:', { bugTitle, bugDescription, bugCategory, bugSeverity });
    setBugTitle('');
    setBugDescription('');
    setBugCategory('general');
    setBugSeverity('medium');
    setShowBugModal(false);
  };

  return (
    <>
      {/* Floating Help Button */}
      <div className="fixed bottom-6 right-6 z-30">
        <button
          onClick={() => setShowMenu(!showMenu)}
          className="w-14 h-14 rounded-full bg-[#2a2b30] hover:bg-[#3a3b40] border-2 border-[#4ecdc4] shadow-2xl flex items-center justify-center transition-all hover:scale-110 group"
        >
          <HelpCircle className="w-7 h-7 text-[#4ecdc4] group-hover:text-white transition-colors" />
        </button>

        {/* Context Menu */}
        {showMenu && (
          <>
            {/* Backdrop */}
            <div
              className="fixed inset-0 z-40"
              onClick={() => setShowMenu(false)}
            />

            {/* Menu */}
            <div className="absolute bottom-16 right-0 bg-[#1c1d22] rounded-lg shadow-2xl z-50 card-float border-2 border-[#2a2b30] overflow-hidden min-w-[180px]">
              <div className="py-2">
                <button
                  onClick={() => {
                    setShowHelpModal(true);
                    setShowMenu(false);
                  }}
                  className="w-full px-4 py-2.5 flex items-center gap-3 hover:bg-gradient-to-r hover:from-[#4ecdc4]/10 hover:to-transparent transition-all text-left"
                >
                  <Book className="w-4 h-4 text-[#4ecdc4]" />
                  <span className="text-sm font-medium">Help</span>
                </button>

                <button
                  onClick={() => {
                    window.dispatchEvent(new CustomEvent('sekailink:report-bug'));
                    setShowMenu(false);
                  }}
                  className="w-full px-4 py-2.5 flex items-center gap-3 hover:bg-gradient-to-r hover:from-[#ff6b35]/10 hover:to-transparent transition-all text-left"
                >
                  <Bug className="w-4 h-4 text-[#ff6b35]" />
                  <span className="text-sm font-medium">Report a Bug</span>
                </button>

                <button
                  onClick={() => {
                    setShowAboutModal(true);
                    setShowMenu(false);
                  }}
                  className="w-full px-4 py-2.5 flex items-center gap-3 hover:bg-gradient-to-r hover:from-[#aa96da]/10 hover:to-transparent transition-all text-left"
                >
                  <Info className="w-4 h-4 text-[#aa96da]" />
                  <span className="text-sm font-medium">About</span>
                </button>
              </div>
            </div>
          </>
        )}
      </div>

      {/* Help Modal */}
      {showHelpModal && (
        <div className="fixed left-0 right-0 bottom-0 top-[32px] bg-black/80 backdrop-blur-sm flex items-center justify-center z-50 p-8">
          <div className="w-full max-w-5xl h-[80vh] bg-[#161b22] rounded-xl shadow-2xl border-2 border-[#4ecdc4] card-float overflow-hidden flex flex-col">
            {/* Header */}
            <div className="p-6 border-b-2 border-[#2a2b30] bg-gradient-to-r from-[#1c1d22] to-[#161b22] flex items-center justify-between">
              <div className="flex items-center gap-3">
                <div className="w-10 h-10 rounded-lg bg-gradient-to-br from-[#4ecdc4] to-[#95e1d3] flex items-center justify-center">
                  <Book className="w-5 h-5 text-[#14151a]" />
                </div>
                <div>
                  <h2 className="text-2xl font-bold">Help Center</h2>
                  <p className="text-sm text-[#8e8f94]">Documentation and guides</p>
                </div>
              </div>
              <button
                onClick={() => setShowHelpModal(false)}
                className="w-10 h-10 rounded-lg bg-[#2a2b30] hover:bg-[#f85149] transition-colors flex items-center justify-center group"
              >
                <X className="w-5 h-5 text-[#8e8f94] group-hover:text-white" />
              </button>
            </div>

            {/* Tabs */}
            <div className="flex gap-2 px-6 pt-4 border-b border-[#2a2b30]">
              {[
                { id: 'getting-started', label: 'Getting Started' },
                { id: 'lobby', label: 'Lobby Guide' },
                { id: 'sync', label: 'Sync & Play' },
                { id: 'troubleshooting', label: 'Troubleshooting' },
              ].map((tab) => (
                <button
                  key={tab.id}
                  onClick={() => setHelpTab(tab.id)}
                  className={`px-4 py-2 text-sm font-medium transition-all border-b-2 ${
                    helpTab === tab.id
                      ? 'border-[#4ecdc4] text-[#4ecdc4]'
                      : 'border-transparent text-[#8e8f94] hover:text-white'
                  }`}
                >
                  {tab.label}
                </button>
              ))}
            </div>

            {/* Content - iframe ready */}
            <div className="flex-1 overflow-auto p-6">
              <div className="prose prose-invert max-w-none">
                <h3 className="text-xl font-bold text-[#e6edf3] mb-4">
                  {helpTab === 'getting-started' && 'Getting Started with SekaiLink'}
                  {helpTab === 'lobby' && 'Lobby Management'}
                  {helpTab === 'sync' && 'Sync & Gameplay'}
                  {helpTab === 'troubleshooting' && 'Troubleshooting'}
                </h3>

                {/* Placeholder content - ready to be replaced with iframe */}
                <div className="text-[#8e8f94] space-y-4">
                  <p>This section is ready for documentation content.</p>
                  <p className="text-xs">
                    To use an iframe: Replace this div with:<br />
                    <code className="bg-[#0d1117] px-2 py-1 rounded text-[#4ecdc4]">
                      &lt;iframe src="/help/{helpTab}.html" className="w-full h-full border-0" /&gt;
                    </code>
                  </p>
                </div>
              </div>
            </div>
          </div>
        </div>
      )}

      {/* Bug Report Modal */}
      {showBugModal && (
        <div className="fixed left-0 right-0 bottom-0 top-[32px] bg-black/80 backdrop-blur-sm flex items-center justify-center z-50 p-8">
          <div className="w-full max-w-3xl bg-[#161b22] rounded-xl shadow-2xl border-2 border-[#ff6b35] card-float overflow-hidden max-h-[90vh] flex flex-col">
            {/* Header */}
            <div className="p-6 border-b-2 border-[#2a2b30] bg-gradient-to-r from-[#1c1d22] to-[#161b22] flex items-center justify-between">
              <div className="flex items-center gap-3">
                <div className="w-10 h-10 rounded-lg bg-gradient-to-br from-[#ff6b35] to-[#f38181] flex items-center justify-center">
                  <Bug className="w-5 h-5 text-white" />
                </div>
                <div>
                  <h2 className="text-2xl font-bold">Report a Bug</h2>
                  <p className="text-sm text-[#8e8f94]">Help us improve SekaiLink</p>
                </div>
              </div>
              <button
                onClick={() => setShowBugModal(false)}
                className="w-10 h-10 rounded-lg bg-[#2a2b30] hover:bg-[#f85149] transition-colors flex items-center justify-center group"
              >
                <X className="w-5 h-5 text-[#8e8f94] group-hover:text-white" />
              </button>
            </div>

            {/* Content */}
            <div className="flex-1 overflow-y-auto p-6 space-y-6">
              {/* Bug Title */}
              <div>
                <label className="block text-sm font-semibold text-[#e6edf3] mb-2">
                  BUG TITLE
                </label>
                <input
                  type="text"
                  value={bugTitle}
                  onChange={(e) => setBugTitle(e.target.value)}
                  placeholder="Brief description of the issue..."
                  className="w-full px-4 py-3 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#ff6b35] outline-none transition-colors"
                />
              </div>

              {/* Category and Severity */}
              <div className="grid grid-cols-2 gap-4">
                <div>
                  <label className="block text-xs font-medium text-[#8e8f94] mb-2">
                    CATEGORY
                  </label>
                  <select
                    value={bugCategory}
                    onChange={(e) => setBugCategory(e.target.value)}
                    className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#ff6b35] outline-none transition-colors"
                  >
                    <option value="general">General</option>
                    <option value="lobby">Lobby</option>
                    <option value="sync">Sync/Generation</option>
                    <option value="gameplay">Gameplay</option>
                    <option value="ui">User Interface</option>
                    <option value="performance">Performance</option>
                    <option value="crash">Crash</option>
                  </select>
                </div>

                <div>
                  <label className="block text-xs font-medium text-[#8e8f94] mb-2">
                    SEVERITY
                  </label>
                  <select
                    value={bugSeverity}
                    onChange={(e) => setBugSeverity(e.target.value)}
                    className="w-full px-4 py-2.5 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white focus:border-[#ff6b35] outline-none transition-colors"
                  >
                    <option value="low">Low - Minor inconvenience</option>
                    <option value="medium">Medium - Affects functionality</option>
                    <option value="high">High - Major issue</option>
                    <option value="critical">Critical - App unusable</option>
                  </select>
                </div>
              </div>

              {/* Description */}
              <div>
                <label className="block text-sm font-semibold text-[#e6edf3] mb-2">
                  DESCRIPTION
                </label>
                <textarea
                  value={bugDescription}
                  onChange={(e) => setBugDescription(e.target.value)}
                  placeholder="Please describe the bug in detail. Include steps to reproduce, expected behavior, and actual behavior..."
                  rows={8}
                  className="w-full px-4 py-3 bg-[#0d1117] border-2 border-[#2a2b30] rounded-lg text-white placeholder:text-[#8e8f94] focus:border-[#ff6b35] outline-none transition-colors resize-none"
                />
              </div>

              {/* System Info */}
              <div className="p-4 bg-[#0d1117] border border-[#2a2b30] rounded-lg">
                <h4 className="text-xs font-bold text-[#e6edf3] mb-2">SYSTEM INFORMATION (Auto-collected)</h4>
                <div className="text-xs text-[#8e8f94] space-y-1">
                  <div>Version: BETA3-1.0.1</div>
                  <div>Platform: {navigator.platform}</div>
                  <div>User Agent: {navigator.userAgent.substring(0, 50)}...</div>
                </div>
              </div>
            </div>

            {/* Footer */}
            <div className="p-6 border-t-2 border-[#2a2b30] bg-[#14151a]/90 flex justify-end gap-3">
              <button
                onClick={() => setShowBugModal(false)}
                className="px-6 py-3 bg-[#2a2b30] hover:bg-[#3a3b40] rounded-lg font-medium transition-colors"
              >
                Cancel
              </button>
              <button
                onClick={handleBugSubmit}
                disabled={!bugTitle || !bugDescription}
                className="px-8 py-3 bg-gradient-to-r from-[#ff6b35] to-[#f38181] rounded-lg font-bold shadow-lg hover:shadow-xl transition-all disabled:opacity-50 disabled:cursor-not-allowed flex items-center gap-2"
              >
                <Send className="w-4 h-4" />
                Submit Report
              </button>
            </div>
          </div>
        </div>
      )}

      {/* About Modal */}
      {showAboutModal && (
        <div className="fixed left-0 right-0 bottom-0 top-[32px] bg-black/80 backdrop-blur-sm flex items-center justify-center z-50 p-8">
          <div className="w-full max-w-2xl bg-[#161b22] rounded-xl shadow-2xl border-2 border-[#aa96da] card-float overflow-hidden">
            {/* Header */}
            <div className="p-6 border-b-2 border-[#2a2b30] bg-gradient-to-r from-[#1c1d22] to-[#161b22] flex items-center justify-between">
              <div className="flex items-center gap-3">
                <div className="w-10 h-10 rounded-lg bg-gradient-to-br from-[#aa96da] to-[#f38181] flex items-center justify-center">
                  <Info className="w-5 h-5 text-white" />
                </div>
                <h2 className="text-2xl font-bold">About SekaiLink</h2>
              </div>
              <button
                onClick={() => setShowAboutModal(false)}
                className="w-10 h-10 rounded-lg bg-[#2a2b30] hover:bg-[#f85149] transition-colors flex items-center justify-center group"
              >
                <X className="w-5 h-5 text-[#8e8f94] group-hover:text-white" />
              </button>
            </div>

            {/* Content */}
            <div className="p-8 space-y-6">
              {/* Logo */}
              <div className="text-center">
                <div className="flex items-center justify-center gap-3 mb-4">
                  <ImageWithFallback
                    src={logoImage}
                    alt="SekaiLink Logo"
                    className="h-16 w-auto"
                  />
                  <ImageWithFallback
                    src={logoText}
                    alt="SekaiLink"
                    className="h-12 w-auto"
                  />
                </div>
                <p className="text-sm text-[#8e8f94]">Client Core</p>
              </div>

              {/* Info */}
              <div className="space-y-4">
                <div className="p-4 bg-[#0d1117] rounded-lg border border-[#2a2b30]">
                  <div className="text-sm text-[#8e8f94] mb-1">Created by</div>
                  <div className="font-bold text-[#4ecdc4]">TheLovenityJade</div>
                </div>

                <div className="p-4 bg-[#0d1117] rounded-lg border border-[#2a2b30]">
                  <div className="text-sm text-[#8e8f94] mb-2">Powered by</div>
                  <div className="space-y-2">
                    <div className="flex items-center justify-between">
                      <span className="font-medium">Archipelago</span>
                      <a
                        href="https://archipelago.gg"
                        target="_blank"
                        rel="noopener noreferrer"
                        className="text-xs text-[#4ecdc4] hover:underline"
                      >
                        archipelago.gg
                      </a>
                    </div>
                    <div className="flex items-center justify-between">
                      <span className="font-medium">Libretro</span>
                      <a
                        href="https://www.libretro.com"
                        target="_blank"
                        rel="noopener noreferrer"
                        className="text-xs text-[#4ecdc4] hover:underline"
                      >
                        libretro.com
                      </a>
                    </div>
                  </div>
                </div>

                <div className="p-4 bg-[#0d1117] rounded-lg border border-[#2a2b30]">
                  <div className="text-sm text-[#8e8f94] mb-1">Version</div>
                  <div className="font-bold">BETA3-1.0.1</div>
                </div>
              </div>

              {/* Credits Button */}
              <button className="w-full py-3 bg-gradient-to-r from-[#aa96da] to-[#f38181] rounded-lg font-bold shadow-lg hover:shadow-xl transition-all">
                Watch Credits
              </button>
            </div>
          </div>
        </div>
      )}
    </>
  );
}
