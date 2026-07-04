#pragma once

#include "video_backend.hpp"

#include <libretro.h>

#include <string>

namespace sekaiemu::spike {

struct GlViewportRect {
  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
};

bool IsOpenGlLike(retro_hw_context_type type);
GlViewportRect ComputeGameViewport(int window_width,
                                   int window_height,
                                   const VideoGeometry& geometry,
                                   bool reserve_sidebar,
                                   unsigned sidebar_width);
std::string VertexShaderSource(retro_hw_context_type type);
std::string FragmentShaderSource(retro_hw_context_type type);
bool CompileShader(unsigned shader, const std::string& source, std::string& error);
bool LinkProgram(unsigned program, std::string& error);

}  // namespace sekaiemu::spike
