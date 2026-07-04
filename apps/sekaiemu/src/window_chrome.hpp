#pragma once

#include <SDL.h>

namespace sekaiemu::spike {

void EnableBorderlessTitlebarDrag(SDL_Window* window, int titlebar_height = 34);

}  // namespace sekaiemu::spike
