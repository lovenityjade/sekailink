#define SEKAIEMU_OPENGL_LOADER_IMPLEMENTATION
#include "opengl_loader.hpp"

#if defined(_WIN32)
#include <SDL.h>
#endif

namespace sekaiemu::spike {

#if defined(_WIN32)
PFNGLACTIVETEXTUREPROC sekai_glActiveTexture = nullptr;
PFNGLATTACHSHADERPROC sekai_glAttachShader = nullptr;
PFNGLBINDBUFFERPROC sekai_glBindBuffer = nullptr;
PFNGLBINDFRAMEBUFFERPROC sekai_glBindFramebuffer = nullptr;
PFNGLBINDRENDERBUFFERPROC sekai_glBindRenderbuffer = nullptr;
PFNGLBINDVERTEXARRAYPROC sekai_glBindVertexArray = nullptr;
PFNGLBLITFRAMEBUFFERPROC sekai_glBlitFramebuffer = nullptr;
PFNGLBUFFERDATAPROC sekai_glBufferData = nullptr;
PFNGLCHECKFRAMEBUFFERSTATUSPROC sekai_glCheckFramebufferStatus = nullptr;
PFNGLCOMPILESHADERPROC sekai_glCompileShader = nullptr;
PFNGLCREATEPROGRAMPROC sekai_glCreateProgram = nullptr;
PFNGLCREATESHADERPROC sekai_glCreateShader = nullptr;
PFNGLDELETEBUFFERSPROC sekai_glDeleteBuffers = nullptr;
PFNGLDELETEFRAMEBUFFERSPROC sekai_glDeleteFramebuffers = nullptr;
PFNGLDELETEPROGRAMPROC sekai_glDeleteProgram = nullptr;
PFNGLDELETERENDERBUFFERSPROC sekai_glDeleteRenderbuffers = nullptr;
PFNGLDELETESHADERPROC sekai_glDeleteShader = nullptr;
PFNGLDELETEVERTEXARRAYSPROC sekai_glDeleteVertexArrays = nullptr;
PFNGLENABLEVERTEXATTRIBARRAYPROC sekai_glEnableVertexAttribArray = nullptr;
PFNGLFRAMEBUFFERRENDERBUFFERPROC sekai_glFramebufferRenderbuffer = nullptr;
PFNGLFRAMEBUFFERTEXTURE2DPROC sekai_glFramebufferTexture2D = nullptr;
PFNGLGENBUFFERSPROC sekai_glGenBuffers = nullptr;
PFNGLGENFRAMEBUFFERSPROC sekai_glGenFramebuffers = nullptr;
PFNGLGENRENDERBUFFERSPROC sekai_glGenRenderbuffers = nullptr;
PFNGLGENVERTEXARRAYSPROC sekai_glGenVertexArrays = nullptr;
PFNGLGETPROGRAMINFOLOGPROC sekai_glGetProgramInfoLog = nullptr;
PFNGLGETPROGRAMIVPROC sekai_glGetProgramiv = nullptr;
PFNGLGETSHADERINFOLOGPROC sekai_glGetShaderInfoLog = nullptr;
PFNGLGETSHADERIVPROC sekai_glGetShaderiv = nullptr;
PFNGLGETUNIFORMLOCATIONPROC sekai_glGetUniformLocation = nullptr;
PFNGLLINKPROGRAMPROC sekai_glLinkProgram = nullptr;
PFNGLRENDERBUFFERSTORAGEPROC sekai_glRenderbufferStorage = nullptr;
PFNGLSHADERSOURCEPROC sekai_glShaderSource = nullptr;
PFNGLUNIFORM1IPROC sekai_glUniform1i = nullptr;
PFNGLUSEPROGRAMPROC sekai_glUseProgram = nullptr;
PFNGLVERTEXATTRIBPOINTERPROC sekai_glVertexAttribPointer = nullptr;

namespace {

template <typename FunctionPointer>
bool LoadFunction(FunctionPointer& target, const char* name, std::string& error) {
  target = reinterpret_cast<FunctionPointer>(SDL_GL_GetProcAddress(name));
  if (target == nullptr) {
    error = std::string("OpenGL function is unavailable: ") + name;
    return false;
  }
  return true;
}

}  // namespace

bool LoadOpenGlFunctions(std::string& error) {
  static bool loaded = false;
  if (loaded) {
    return true;
  }

  if (!LoadFunction(sekai_glActiveTexture, "glActiveTexture", error) ||
      !LoadFunction(sekai_glAttachShader, "glAttachShader", error) ||
      !LoadFunction(sekai_glBindBuffer, "glBindBuffer", error) ||
      !LoadFunction(sekai_glBindFramebuffer, "glBindFramebuffer", error) ||
      !LoadFunction(sekai_glBindRenderbuffer, "glBindRenderbuffer", error) ||
      !LoadFunction(sekai_glBindVertexArray, "glBindVertexArray", error) ||
      !LoadFunction(sekai_glBlitFramebuffer, "glBlitFramebuffer", error) ||
      !LoadFunction(sekai_glBufferData, "glBufferData", error) ||
      !LoadFunction(sekai_glCheckFramebufferStatus, "glCheckFramebufferStatus", error) ||
      !LoadFunction(sekai_glCompileShader, "glCompileShader", error) ||
      !LoadFunction(sekai_glCreateProgram, "glCreateProgram", error) ||
      !LoadFunction(sekai_glCreateShader, "glCreateShader", error) ||
      !LoadFunction(sekai_glDeleteBuffers, "glDeleteBuffers", error) ||
      !LoadFunction(sekai_glDeleteFramebuffers, "glDeleteFramebuffers", error) ||
      !LoadFunction(sekai_glDeleteProgram, "glDeleteProgram", error) ||
      !LoadFunction(sekai_glDeleteRenderbuffers, "glDeleteRenderbuffers", error) ||
      !LoadFunction(sekai_glDeleteShader, "glDeleteShader", error) ||
      !LoadFunction(sekai_glDeleteVertexArrays, "glDeleteVertexArrays", error) ||
      !LoadFunction(sekai_glEnableVertexAttribArray, "glEnableVertexAttribArray", error) ||
      !LoadFunction(sekai_glFramebufferRenderbuffer, "glFramebufferRenderbuffer", error) ||
      !LoadFunction(sekai_glFramebufferTexture2D, "glFramebufferTexture2D", error) ||
      !LoadFunction(sekai_glGenBuffers, "glGenBuffers", error) ||
      !LoadFunction(sekai_glGenFramebuffers, "glGenFramebuffers", error) ||
      !LoadFunction(sekai_glGenRenderbuffers, "glGenRenderbuffers", error) ||
      !LoadFunction(sekai_glGenVertexArrays, "glGenVertexArrays", error) ||
      !LoadFunction(sekai_glGetProgramInfoLog, "glGetProgramInfoLog", error) ||
      !LoadFunction(sekai_glGetProgramiv, "glGetProgramiv", error) ||
      !LoadFunction(sekai_glGetShaderInfoLog, "glGetShaderInfoLog", error) ||
      !LoadFunction(sekai_glGetShaderiv, "glGetShaderiv", error) ||
      !LoadFunction(sekai_glGetUniformLocation, "glGetUniformLocation", error) ||
      !LoadFunction(sekai_glLinkProgram, "glLinkProgram", error) ||
      !LoadFunction(sekai_glRenderbufferStorage, "glRenderbufferStorage", error) ||
      !LoadFunction(sekai_glShaderSource, "glShaderSource", error) ||
      !LoadFunction(sekai_glUniform1i, "glUniform1i", error) ||
      !LoadFunction(sekai_glUseProgram, "glUseProgram", error) ||
      !LoadFunction(sekai_glVertexAttribPointer, "glVertexAttribPointer", error)) {
    return false;
  }

  loaded = true;
  return true;
}

#else

bool LoadOpenGlFunctions(std::string&) {
  return true;
}

#endif

}  // namespace sekaiemu::spike
