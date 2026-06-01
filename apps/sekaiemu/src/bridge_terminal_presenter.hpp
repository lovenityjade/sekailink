#pragma once

#include "bridge_runtime_status.hpp"
#include "opengl_overlay_renderer.hpp"

#include <cstdint>
#include <vector>

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
};

}  // namespace sekaiemu::spike
