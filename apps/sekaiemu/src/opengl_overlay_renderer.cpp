#include "opengl_loader.hpp"
#include "opengl_overlay_renderer.hpp"

#include "opengl_video_utils.hpp"

#include <array>

namespace sekaiemu::spike {

bool OpenGlOverlayRenderer::Draw(retro_hw_context_type context_type,
                                 const std::vector<std::uint8_t>& rgba_pixels,
            unsigned width,
            unsigned height,
            int window_width,
            int window_height,
            bool update_pixels,
            std::string& error) {
  if (rgba_pixels.empty()) {
    return true;
  }
  if (!EnsureResources(context_type, error)) {
    return false;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glViewport(0, 0, window_width, window_height);

  glUseProgram(program_);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  if (texture_width_ != width || texture_height_ != height) {
    glTexImage2D(GL_TEXTURE_2D,
                 0,
                 GL_RGBA,
                 static_cast<GLsizei>(width),
                 static_cast<GLsizei>(height),
                 0,
                 GL_RGBA,
                 GL_UNSIGNED_BYTE,
                 rgba_pixels.data());
    texture_width_ = width;
    texture_height_ = height;
  } else if (update_pixels) {
    glTexSubImage2D(GL_TEXTURE_2D,
                    0,
                    0,
                    0,
                    static_cast<GLsizei>(width),
                    static_cast<GLsizei>(height),
                    GL_RGBA,
                    GL_UNSIGNED_BYTE,
                    rgba_pixels.data());
  }

  const GLint sampler_location = glGetUniformLocation(program_, "u_overlay");
  if (sampler_location >= 0) {
    glUniform1i(sampler_location, 0);
  }

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_DEPTH_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_STENCIL_TEST);

  glBindVertexArray(vao_);
  glDrawArrays(GL_TRIANGLES, 0, 6);
  glBindVertexArray(0);

  glDisable(GL_BLEND);
  glBindTexture(GL_TEXTURE_2D, 0);
  glUseProgram(0);
  return true;
}

void OpenGlOverlayRenderer::Destroy() {
  if (texture_ != 0) {
    glDeleteTextures(1, &texture_);
    texture_ = 0;
  }
  texture_width_ = 0;
  texture_height_ = 0;
  if (vbo_ != 0) {
    glDeleteBuffers(1, &vbo_);
    vbo_ = 0;
  }
  if (vao_ != 0) {
    glDeleteVertexArrays(1, &vao_);
    vao_ = 0;
  }
  if (program_ != 0) {
    glDeleteProgram(program_);
    program_ = 0;
  }
}

bool OpenGlOverlayRenderer::EnsureResources(retro_hw_context_type context_type,
                                            std::string& error) {
  if (!LoadOpenGlFunctions(error)) {
    return false;
  }

  if (program_ != 0 && texture_ != 0 && vao_ != 0 && vbo_ != 0) {
    return true;
  }

  const auto vertex_source = VertexShaderSource(context_type);
  const auto fragment_source = FragmentShaderSource(context_type);

  GLuint vertex_shader = glCreateShader(GL_VERTEX_SHADER);
  GLuint fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
  if (vertex_shader == 0 || fragment_shader == 0) {
    error = "OpenGL shader allocation failed.";
    return false;
  }

  if (!CompileShader(vertex_shader, vertex_source, error) ||
      !CompileShader(fragment_shader, fragment_source, error)) {
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return false;
  }

  program_ = glCreateProgram();
  glAttachShader(program_, vertex_shader);
  glAttachShader(program_, fragment_shader);
  if (!LinkProgram(program_, error)) {
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    glDeleteProgram(program_);
    program_ = 0;
    return false;
  }

  glDeleteShader(vertex_shader);
  glDeleteShader(fragment_shader);

  constexpr std::array<float, 24> quad_vertices{
      -1.0f, -1.0f, 0.0f, 1.0f,
       1.0f, -1.0f, 1.0f, 1.0f,
       1.0f,  1.0f, 1.0f, 0.0f,
      -1.0f, -1.0f, 0.0f, 1.0f,
       1.0f,  1.0f, 1.0f, 0.0f,
      -1.0f,  1.0f, 0.0f, 0.0f,
  };

  glGenVertexArrays(1, &vao_);
  glGenBuffers(1, &vbo_);
  glBindVertexArray(vao_);
  glBindBuffer(GL_ARRAY_BUFFER, vbo_);
  glBufferData(GL_ARRAY_BUFFER,
               static_cast<GLsizeiptr>(quad_vertices.size() * sizeof(float)),
               quad_vertices.data(),
               GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void*>(0));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1,
                        2,
                        GL_FLOAT,
                        GL_FALSE,
                        4 * sizeof(float),
                        reinterpret_cast<void*>(2 * sizeof(float)));
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);

  glGenTextures(1, &texture_);
  glBindTexture(GL_TEXTURE_2D, texture_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);

  return true;
}

}  // namespace sekaiemu::spike
