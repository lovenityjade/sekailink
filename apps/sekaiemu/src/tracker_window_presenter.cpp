#include "tracker_window_presenter.hpp"

#include "window_chrome.hpp"

#include "overlay_canvas.hpp"
#include "opengl_loader.hpp"
#include "tracker_overlay_renderer.hpp"

#include <SDL.h>

#include <algorithm>
#include <iostream>

namespace sekaiemu::spike {
namespace {

void RestoreGlContext(SDL_Window* window, SDL_GLContext context) {
  SDL_GL_MakeCurrent(context != nullptr ? window : nullptr, context);
}

}  // namespace

TrackerWindowPresenter::~TrackerWindowPresenter() {
  Shutdown();
}

bool TrackerWindowPresenter::EnsureWindow(unsigned width, unsigned height) {
  bool created_window = false;
  if (!window_) {
    window_ = SDL_CreateWindow("Sekaiemu Tracker",
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               static_cast<int>(width),
                               static_cast<int>(height),
                               SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                   SDL_WINDOW_BORDERLESS);
    if (!window_) {
      return false;
    }
    EnableBorderlessTitlebarDrag(window_);
    created_window = true;
  }
  if (!gl_context_) {
    SDL_GL_ResetAttributes();
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    gl_context_ = SDL_GL_CreateContext(window_);
    if (!gl_context_) {
      return false;
    }
    auto* previous_window = SDL_GL_GetCurrentWindow();
    const auto previous_context = SDL_GL_GetCurrentContext();
    if (SDL_GL_MakeCurrent(window_, gl_context_) != 0) {
      return false;
    }
    std::string error;
    if (!LoadOpenGlFunctions(error)) {
      std::cerr << "[sekaiemu-libretro-spike] tracker separate window OpenGL loader failed: "
                << error << "\n";
      RestoreGlContext(previous_window, previous_context);
      return false;
    }
    SDL_GL_SetSwapInterval(0);
    if (!context_logged_) {
      const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
      const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
      std::cerr << "[sekaiemu-libretro-spike] tracker separate window OpenGL active: renderer="
                << (renderer != nullptr ? renderer : "unknown")
                << " version=" << (version != nullptr ? version : "unknown")
                << " swap_interval=" << SDL_GL_GetSwapInterval() << "\n";
      context_logged_ = true;
    }
    RestoreGlContext(previous_window, previous_context);
  }
  if (created_window) {
    width_ = width;
    height_ = height;
  }
  return true;
}

void TrackerWindowPresenter::Render(const TrackerRuntime& runtime,
                                    const TrackerResolvedViewState& resolved,
                                    const TrackerOverlayAssetResolver* asset_resolver) {
  constexpr unsigned kTrackerWindowWidth = 520;
  constexpr unsigned kTrackerWindowHeight = 760;
  if (!EnsureWindow(kTrackerWindowWidth, kTrackerWindowHeight)) {
    return;
  }
  int drawable_width = 0;
  int drawable_height = 0;
  SDL_GL_GetDrawableSize(window_, &drawable_width, &drawable_height);
  const unsigned render_width =
      static_cast<unsigned>(std::max<int>(360, drawable_width > 0 ? drawable_width : kTrackerWindowWidth));
  const unsigned render_height =
      static_cast<unsigned>(std::max<int>(360, drawable_height > 0 ? drawable_height : kTrackerWindowHeight));

  OverlayCanvas canvas(render_width, render_height);
  canvas.Clear({0, 0, 0, 255});
  RenderTrackerPanel(canvas,
                     runtime,
                     resolved,
                     TrackerPanelLayout{0, 0, static_cast<int>(render_width), static_cast<int>(render_height)},
                     false,
                     "WINDOW",
                     asset_resolver);
  pixels_.assign(canvas.Data(),
                 canvas.Data() + static_cast<std::size_t>(canvas.Width()) * canvas.Height() * 4u);
  width_ = render_width;
  height_ = render_height;
  dirty_ = true;
}

void TrackerWindowPresenter::Present() {
  if (!window_ || !gl_context_ || pixels_.empty() || !dirty_) {
    return;
  }
  auto* previous_window = SDL_GL_GetCurrentWindow();
  const auto previous_context = SDL_GL_GetCurrentContext();
  SDL_GL_MakeCurrent(window_, gl_context_);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  int drawable_width = 0;
  int drawable_height = 0;
  SDL_GL_GetDrawableSize(window_, &drawable_width, &drawable_height);
  glViewport(0, 0, std::max(1, drawable_width), std::max(1, drawable_height));
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  std::string error;
  renderer_.Draw(RETRO_HW_CONTEXT_OPENGL_CORE,
                 pixels_,
                 width_,
                 height_,
                 std::max(1, drawable_width),
                 std::max(1, drawable_height),
                 true,
                 error);
  SDL_GL_SwapWindow(window_);
  RestoreGlContext(previous_window, previous_context);
  dirty_ = false;
}

void TrackerWindowPresenter::Shutdown() {
  auto* previous_window = SDL_GL_GetCurrentWindow();
  const auto previous_context = SDL_GL_GetCurrentContext();
  const bool previous_context_is_ours = previous_context == gl_context_;
  if (window_ && gl_context_) {
    SDL_GL_MakeCurrent(window_, gl_context_);
  }
  renderer_.Destroy();
  if (gl_context_) {
    SDL_GL_DeleteContext(gl_context_);
    gl_context_ = nullptr;
  }
  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  pixels_.clear();
  width_ = 0;
  height_ = 0;
  dirty_ = false;
  context_logged_ = false;
  if (!previous_context_is_ours) {
    RestoreGlContext(previous_window, previous_context);
  } else {
    RestoreGlContext(nullptr, nullptr);
  }
}

}  // namespace sekaiemu::spike
