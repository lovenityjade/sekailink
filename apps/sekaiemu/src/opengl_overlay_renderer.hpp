#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <libretro.h>

namespace sekaiemu::spike {

class OpenGlOverlayRenderer {
 public:
  bool Draw(retro_hw_context_type context_type,
            const std::vector<std::uint8_t>& rgba_pixels,
            unsigned width,
            unsigned height,
            int window_width,
            int window_height,
            bool update_pixels,
            std::string& error);
  void Destroy();

 private:
  bool EnsureResources(retro_hw_context_type context_type, std::string& error);

  unsigned texture_ = 0;
  unsigned texture_width_ = 0;
  unsigned texture_height_ = 0;
  unsigned program_ = 0;
  unsigned vao_ = 0;
  unsigned vbo_ = 0;
};

}  // namespace sekaiemu::spike
