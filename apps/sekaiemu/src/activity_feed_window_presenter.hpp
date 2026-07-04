#pragma once

#include "opengl_overlay_renderer.hpp"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <string>
#include <vector>

struct SDL_Window;

namespace sekaiemu::spike {

class ActivityFeedWindowPresenter {
 public:
  ActivityFeedWindowPresenter() = default;
  ActivityFeedWindowPresenter(const ActivityFeedWindowPresenter&) = delete;
  ActivityFeedWindowPresenter& operator=(const ActivityFeedWindowPresenter&) = delete;
  ~ActivityFeedWindowPresenter();

  void Render(const nlohmann::json& snapshot);
  void Present();
  void Shutdown();
  unsigned Width() const { return width_; }
  unsigned Height() const { return height_; }

 private:
  bool EnsureWindow(unsigned width, unsigned height);

  SDL_Window* window_ = nullptr;
  void* gl_context_ = nullptr;
  OpenGlOverlayRenderer renderer_;
  std::vector<std::uint8_t> pixels_;
  unsigned width_ = 0;
  unsigned height_ = 0;
  bool dirty_ = false;
  bool context_logged_ = false;
};

}  // namespace sekaiemu::spike
