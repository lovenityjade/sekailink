#define GL_GLEXT_PROTOTYPES
#include <SDL_opengl.h>

#include "opengl_video_utils.hpp"

#include <algorithm>

namespace sekaiemu::spike {

bool IsOpenGlLike(retro_hw_context_type type) {
  switch (type) {
    case RETRO_HW_CONTEXT_OPENGL:
    case RETRO_HW_CONTEXT_OPENGL_CORE:
    case RETRO_HW_CONTEXT_OPENGLES2:
    case RETRO_HW_CONTEXT_OPENGLES3:
    case RETRO_HW_CONTEXT_OPENGLES_VERSION:
      return true;
    default:
      return false;
  }
}

GlViewportRect ComputeGameViewport(int window_width,
                                   int window_height,
                                   const VideoGeometry& geometry,
                                   bool reserve_sidebar,
                                   unsigned sidebar_width) {
  if (window_width <= 0 || window_height <= 0 || geometry.width == 0 || geometry.height == 0) {
    return {0, 0, std::max(1, window_width), std::max(1, window_height)};
  }

  int available_width = window_width;
  if (reserve_sidebar) {
    available_width = std::max(1, available_width - static_cast<int>(sidebar_width));
  }

  const double source_aspect =
      static_cast<double>(geometry.width) / static_cast<double>(geometry.height);
  int target_width = available_width;
  int target_height = static_cast<int>(target_width / source_aspect);
  if (target_height > window_height) {
    target_height = window_height;
    target_width = static_cast<int>(target_height * source_aspect);
  }

  const int target_x = (available_width - target_width) / 2;
  const int target_y = (window_height - target_height) / 2;
  return {target_x, target_y, std::max(1, target_width), std::max(1, target_height)};
}

std::string VertexShaderSource(retro_hw_context_type type) {
  const bool es = type == RETRO_HW_CONTEXT_OPENGLES2 ||
                  type == RETRO_HW_CONTEXT_OPENGLES3 ||
                  type == RETRO_HW_CONTEXT_OPENGLES_VERSION;
  if (es) {
    return R"(#version 300 es
precision mediump float;
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
out vec2 v_uv;
void main() {
  v_uv = a_uv;
  gl_Position = vec4(a_pos, 0.0, 1.0);
})";
  }

  return R"(#version 330 core
layout(location = 0) in vec2 a_pos;
layout(location = 1) in vec2 a_uv;
out vec2 v_uv;
void main() {
  v_uv = a_uv;
  gl_Position = vec4(a_pos, 0.0, 1.0);
})";
}

std::string FragmentShaderSource(retro_hw_context_type type) {
  const bool es = type == RETRO_HW_CONTEXT_OPENGLES2 ||
                  type == RETRO_HW_CONTEXT_OPENGLES3 ||
                  type == RETRO_HW_CONTEXT_OPENGLES_VERSION;
  if (es) {
    return R"(#version 300 es
precision mediump float;
in vec2 v_uv;
uniform sampler2D u_overlay;
out vec4 frag_color;
void main() {
  frag_color = texture(u_overlay, v_uv);
})";
  }

  return R"(#version 330 core
in vec2 v_uv;
uniform sampler2D u_overlay;
out vec4 frag_color;
void main() {
  frag_color = texture(u_overlay, v_uv);
})";
}

bool CompileShader(unsigned shader, const std::string& source, std::string& error) {
  const char* text = source.c_str();
  glShaderSource(shader, 1, &text, nullptr);
  glCompileShader(shader);

  GLint status = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status == GL_TRUE) {
    return true;
  }

  GLint log_length = 0;
  glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &log_length);
  std::string log(static_cast<std::size_t>(std::max(0, log_length)), '\0');
  if (log_length > 0) {
    glGetShaderInfoLog(shader, log_length, nullptr, log.data());
  }
  error = "OpenGL shader compile failed: " + log;
  return false;
}

bool LinkProgram(unsigned program, std::string& error) {
  glLinkProgram(program);

  GLint status = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &status);
  if (status == GL_TRUE) {
    return true;
  }

  GLint log_length = 0;
  glGetProgramiv(program, GL_INFO_LOG_LENGTH, &log_length);
  std::string log(static_cast<std::size_t>(std::max(0, log_length)), '\0');
  if (log_length > 0) {
    glGetProgramInfoLog(program, log_length, nullptr, log.data());
  }
  error = "OpenGL program link failed: " + log;
  return false;
}

}  // namespace sekaiemu::spike
