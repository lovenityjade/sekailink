#include "window_chrome.hpp"

namespace sekaiemu::spike {
namespace {

SDL_HitTestResult TitlebarHitTest(SDL_Window* window, const SDL_Point* area, void* data) {
  if (window == nullptr || area == nullptr || data == nullptr) {
    return SDL_HITTEST_NORMAL;
  }
  const int titlebar_height = *static_cast<int*>(data);
  int width = 0;
  int height = 0;
  SDL_GetWindowSize(window, &width, &height);
  if (width <= 0 || height <= 0) {
    return SDL_HITTEST_NORMAL;
  }
  const int reserved_right = 92;
  if (area->y >= 0 && area->y < titlebar_height && area->x >= 0 &&
      area->x < width - reserved_right) {
    return SDL_HITTEST_DRAGGABLE;
  }
  return SDL_HITTEST_NORMAL;
}

}  // namespace

void EnableBorderlessTitlebarDrag(SDL_Window* window, int titlebar_height) {
  if (window == nullptr) {
    return;
  }
  static int hit_test_titlebar_height = titlebar_height;
  hit_test_titlebar_height = titlebar_height;
  SDL_SetWindowHitTest(window, TitlebarHitTest, &hit_test_titlebar_height);
}

}  // namespace sekaiemu::spike
