#pragma once

#include <string>

#if !defined(_WIN32)
#define GL_GLEXT_PROTOTYPES
#endif
#include <SDL_opengl.h>

#ifdef DrawText
#undef DrawText
#endif

namespace sekaiemu::spike {

bool LoadOpenGlFunctions(std::string& error);

#if defined(_WIN32)
extern PFNGLACTIVETEXTUREPROC sekai_glActiveTexture;
extern PFNGLATTACHSHADERPROC sekai_glAttachShader;
extern PFNGLBINDBUFFERPROC sekai_glBindBuffer;
extern PFNGLBINDFRAMEBUFFERPROC sekai_glBindFramebuffer;
extern PFNGLBINDRENDERBUFFERPROC sekai_glBindRenderbuffer;
extern PFNGLBINDVERTEXARRAYPROC sekai_glBindVertexArray;
extern PFNGLBLITFRAMEBUFFERPROC sekai_glBlitFramebuffer;
extern PFNGLBUFFERDATAPROC sekai_glBufferData;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC sekai_glCheckFramebufferStatus;
extern PFNGLCOMPILESHADERPROC sekai_glCompileShader;
extern PFNGLCREATEPROGRAMPROC sekai_glCreateProgram;
extern PFNGLCREATESHADERPROC sekai_glCreateShader;
extern PFNGLDELETEBUFFERSPROC sekai_glDeleteBuffers;
extern PFNGLDELETEFRAMEBUFFERSPROC sekai_glDeleteFramebuffers;
extern PFNGLDELETEPROGRAMPROC sekai_glDeleteProgram;
extern PFNGLDELETERENDERBUFFERSPROC sekai_glDeleteRenderbuffers;
extern PFNGLDELETESHADERPROC sekai_glDeleteShader;
extern PFNGLDELETEVERTEXARRAYSPROC sekai_glDeleteVertexArrays;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC sekai_glEnableVertexAttribArray;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC sekai_glFramebufferRenderbuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC sekai_glFramebufferTexture2D;
extern PFNGLGENBUFFERSPROC sekai_glGenBuffers;
extern PFNGLGENFRAMEBUFFERSPROC sekai_glGenFramebuffers;
extern PFNGLGENRENDERBUFFERSPROC sekai_glGenRenderbuffers;
extern PFNGLGENVERTEXARRAYSPROC sekai_glGenVertexArrays;
extern PFNGLGETPROGRAMINFOLOGPROC sekai_glGetProgramInfoLog;
extern PFNGLGETPROGRAMIVPROC sekai_glGetProgramiv;
extern PFNGLGETSHADERINFOLOGPROC sekai_glGetShaderInfoLog;
extern PFNGLGETSHADERIVPROC sekai_glGetShaderiv;
extern PFNGLGETUNIFORMLOCATIONPROC sekai_glGetUniformLocation;
extern PFNGLLINKPROGRAMPROC sekai_glLinkProgram;
extern PFNGLRENDERBUFFERSTORAGEPROC sekai_glRenderbufferStorage;
extern PFNGLSHADERSOURCEPROC sekai_glShaderSource;
extern PFNGLUNIFORM1IPROC sekai_glUniform1i;
extern PFNGLUSEPROGRAMPROC sekai_glUseProgram;
extern PFNGLVERTEXATTRIBPOINTERPROC sekai_glVertexAttribPointer;

#if !defined(SEKAIEMU_OPENGL_LOADER_IMPLEMENTATION)
#define glActiveTexture sekaiemu::spike::sekai_glActiveTexture
#define glAttachShader sekaiemu::spike::sekai_glAttachShader
#define glBindBuffer sekaiemu::spike::sekai_glBindBuffer
#define glBindFramebuffer sekaiemu::spike::sekai_glBindFramebuffer
#define glBindRenderbuffer sekaiemu::spike::sekai_glBindRenderbuffer
#define glBindVertexArray sekaiemu::spike::sekai_glBindVertexArray
#define glBlitFramebuffer sekaiemu::spike::sekai_glBlitFramebuffer
#define glBufferData sekaiemu::spike::sekai_glBufferData
#define glCheckFramebufferStatus sekaiemu::spike::sekai_glCheckFramebufferStatus
#define glCompileShader sekaiemu::spike::sekai_glCompileShader
#define glCreateProgram sekaiemu::spike::sekai_glCreateProgram
#define glCreateShader sekaiemu::spike::sekai_glCreateShader
#define glDeleteBuffers sekaiemu::spike::sekai_glDeleteBuffers
#define glDeleteFramebuffers sekaiemu::spike::sekai_glDeleteFramebuffers
#define glDeleteProgram sekaiemu::spike::sekai_glDeleteProgram
#define glDeleteRenderbuffers sekaiemu::spike::sekai_glDeleteRenderbuffers
#define glDeleteShader sekaiemu::spike::sekai_glDeleteShader
#define glDeleteVertexArrays sekaiemu::spike::sekai_glDeleteVertexArrays
#define glEnableVertexAttribArray sekaiemu::spike::sekai_glEnableVertexAttribArray
#define glFramebufferRenderbuffer sekaiemu::spike::sekai_glFramebufferRenderbuffer
#define glFramebufferTexture2D sekaiemu::spike::sekai_glFramebufferTexture2D
#define glGenBuffers sekaiemu::spike::sekai_glGenBuffers
#define glGenFramebuffers sekaiemu::spike::sekai_glGenFramebuffers
#define glGenRenderbuffers sekaiemu::spike::sekai_glGenRenderbuffers
#define glGenVertexArrays sekaiemu::spike::sekai_glGenVertexArrays
#define glGetProgramInfoLog sekaiemu::spike::sekai_glGetProgramInfoLog
#define glGetProgramiv sekaiemu::spike::sekai_glGetProgramiv
#define glGetShaderInfoLog sekaiemu::spike::sekai_glGetShaderInfoLog
#define glGetShaderiv sekaiemu::spike::sekai_glGetShaderiv
#define glGetUniformLocation sekaiemu::spike::sekai_glGetUniformLocation
#define glLinkProgram sekaiemu::spike::sekai_glLinkProgram
#define glRenderbufferStorage sekaiemu::spike::sekai_glRenderbufferStorage
#define glShaderSource sekaiemu::spike::sekai_glShaderSource
#define glUniform1i sekaiemu::spike::sekai_glUniform1i
#define glUseProgram sekaiemu::spike::sekai_glUseProgram
#define glVertexAttribPointer sekaiemu::spike::sekai_glVertexAttribPointer
#endif
#endif

}  // namespace sekaiemu::spike
