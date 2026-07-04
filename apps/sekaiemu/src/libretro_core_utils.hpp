#pragma once

#include <libretro.h>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef DrawText
#undef DrawText
#endif
#else
#include <dlfcn.h>
#endif

#include <algorithm>
#include <cctype>
#include <string>
#include <string_view>

namespace sekaiemu::spike {

template <typename T>
bool ResolveSymbol(void* handle, const char* name, T& target) {
#if defined(_WIN32)
  void* symbol = reinterpret_cast<void*>(
      GetProcAddress(reinterpret_cast<HMODULE>(handle), name));
#else
  void* symbol = dlsym(handle, name);
#endif
  if (!symbol) {
    return false;
  }
  target = reinterpret_cast<T>(symbol);
  return true;
}

inline std::string PixelFormatName(retro_pixel_format format) {
  switch (format) {
    case RETRO_PIXEL_FORMAT_0RGB1555:
      return "0RGB1555";
    case RETRO_PIXEL_FORMAT_XRGB8888:
      return "XRGB8888";
    case RETRO_PIXEL_FORMAT_RGB565:
      return "RGB565";
    default:
      return "unknown";
  }
}

inline std::string_view LogLevelName(enum retro_log_level level) {
  switch (level) {
    case RETRO_LOG_DEBUG:
      return "debug";
    case RETRO_LOG_INFO:
      return "info";
    case RETRO_LOG_WARN:
      return "warn";
    case RETRO_LOG_ERROR:
      return "error";
    default:
      return "log";
  }
}

inline std::string Lowercase(std::string_view text) {
  std::string lowered(text);
  std::transform(lowered.begin(), lowered.end(), lowered.begin(),
                 [](unsigned char value) { return static_cast<char>(std::tolower(value)); });
  return lowered;
}

inline bool EndsWith(std::string_view text, std::string_view suffix) {
  return text.size() >= suffix.size() &&
         text.substr(text.size() - suffix.size()) == suffix;
}

}  // namespace sekaiemu::spike
