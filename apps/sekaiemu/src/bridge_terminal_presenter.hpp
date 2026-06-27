#pragma once

#include "bridge_runtime_status.hpp"
#include "opengl_overlay_renderer.hpp"
#include "runtime_debug_log.hpp"

#include <cstdint>
#include <string>
#include <vector>

#include <SDL.h>
#include <nlohmann/json.hpp>

struct SDL_Window;

namespace sekaiemu::spike {

class TrackerRuntime;

class BridgeTerminalPresenter {
 public:
  BridgeTerminalPresenter() = default;
  BridgeTerminalPresenter(const BridgeTerminalPresenter&) = delete;
  BridgeTerminalPresenter& operator=(const BridgeTerminalPresenter&) = delete;
  ~BridgeTerminalPresenter();

  void SetEnabled(bool enabled);
  bool Enabled() const { return enabled_; }
  bool HandleSdlEvent(const SDL_Event& event);
  std::vector<std::string> DrainPendingChatCommands();
  void RecordCommandDelivery(std::string_view command, bool sent, std::string_view detail);
  void Render(const BridgeRuntimeStatus& bridge_status, const TrackerRuntime* tracker_runtime);
  void Present();
  void Shutdown();

 private:
  bool EnsureWindow(unsigned width, unsigned height);

  bool enabled_ = false;
  SDL_Window* window_ = nullptr;
  void* gl_context_ = nullptr;
  OpenGlOverlayRenderer renderer_;
  std::vector<std::uint8_t> pixels_;
  unsigned width_ = 0;
  unsigned height_ = 0;
  bool dirty_ = false;
  bool raw_view_ = false;
  RuntimeDebugFilter filter_ = RuntimeDebugFilter::All;
  RuntimeDebugLog debug_log_;
  std::string last_report_;
  std::string command_input_;
  bool command_focused_ = false;
  std::vector<std::string> pending_chat_commands_;
  std::vector<std::string> command_suggestions_;
  nlohmann::json last_snapshot_;
  BridgeRuntimeStatus last_status_;
};

}  // namespace sekaiemu::spike
