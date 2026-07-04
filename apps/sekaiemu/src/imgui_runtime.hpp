#pragma once

#include <SDL.h>

#include <functional>
#include <string>

namespace sekaiemu::spike {

class SekaiemuImGuiRuntime {
 public:
  SekaiemuImGuiRuntime() = default;
  ~SekaiemuImGuiRuntime();

  bool Initialize(SDL_Window* window, SDL_GLContext gl_context, std::string& error);
  void ProcessEvent(const SDL_Event& event);
  void Render(const std::function<void()>& draw_callback);
  void Shutdown();
  bool Active() const { return active_; }

 private:
  bool active_ = false;
};

void ApplySekaiemuImGuiStyle();
void ProcessSekaiemuImGuiEvent(const SDL_Event& event);

}  // namespace sekaiemu::spike
