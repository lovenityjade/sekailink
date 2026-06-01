#pragma once

#include "opengl_overlay_renderer.hpp"

#include <cstdint>
#include <vector>

struct SDL_Window;

namespace sekaiemu::spike {

class TrackerOverlayAssetResolver;
class TrackerRuntime;
struct TrackerResolvedViewState;

class TrackerWindowPresenter {
 public:
  TrackerWindowPresenter() = default;
  TrackerWindowPresenter(const TrackerWindowPresenter&) = delete;
  TrackerWindowPresenter& operator=(const TrackerWindowPresenter&) = delete;
  ~TrackerWindowPresenter();

  void Render(const TrackerRuntime& runtime,
              const TrackerResolvedViewState& resolved,
              const TrackerOverlayAssetResolver* asset_resolver);
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
