#include "software_video_backend.hpp"

#include <algorithm>
#include <cstring>

namespace sekaiemu::spike {

namespace {

Uint32 ToSdlPixelFormat(retro_pixel_format pixel_format) {
  switch (pixel_format) {
    case RETRO_PIXEL_FORMAT_RGB565:
      return SDL_PIXELFORMAT_RGB565;
    case RETRO_PIXEL_FORMAT_0RGB1555:
      return SDL_PIXELFORMAT_XRGB1555;
    case RETRO_PIXEL_FORMAT_XRGB8888:
    default:
      return SDL_PIXELFORMAT_XRGB8888;
  }
}

SDL_Rect ComputePresentationRect(SDL_Renderer* renderer,
                                 const sekaiemu::spike::VideoGeometry& geometry,
                                 bool reserve_sidebar,
                                 unsigned sidebar_width) {
  int output_width = 0;
  int output_height = 0;
  SDL_GetRendererOutputSize(renderer, &output_width, &output_height);
  if (output_width <= 0 || output_height <= 0 || geometry.width == 0 || geometry.height == 0) {
    return SDL_Rect{0, 0, output_width, output_height};
  }

  if (reserve_sidebar) {
    output_width = std::max(1, output_width - static_cast<int>(sidebar_width));
  }

  const double source_aspect = static_cast<double>(geometry.width) / static_cast<double>(geometry.height);
  int target_width = output_width;
  int target_height = static_cast<int>(target_width / source_aspect);
  if (target_height > output_height) {
    target_height = output_height;
    target_width = static_cast<int>(target_height * source_aspect);
  }
  const int target_x = (output_width - target_width) / 2;
  const int target_y = (output_height - target_height) / 2;
  return SDL_Rect{target_x, target_y, std::max(target_width, 1), std::max(target_height, 1)};
}

}  // namespace

SoftwareVideoBackend::~SoftwareVideoBackend() { Shutdown(); }

bool SoftwareVideoBackend::Initialize(const VideoGeometry& geometry, std::string& error) {
  geometry_ = geometry;
  if (!window_) {
    window_ = SDL_CreateWindow("Sekaiemu",
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               640,
                               480,
                               SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!window_) {
      error = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
      return false;
    }
  }

  if (!renderer_) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "nearest");
    renderer_ = SDL_CreateRenderer(window_, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer_) {
      error = std::string("SDL_CreateRenderer failed: ") + SDL_GetError();
      return false;
    }
  }

  ApplyWindowSizing(geometry);
  return RecreateTexture(geometry, error);
}

bool SoftwareVideoBackend::UpdateGeometry(const VideoGeometry& geometry, std::string& error) {
  geometry_ = geometry;
  if (!window_ || !renderer_) {
    return Initialize(geometry, error);
  }

  ApplyWindowSizing(geometry);
  return RecreateTexture(geometry, error);
}

bool SoftwareVideoBackend::UploadSoftwareFrame(const void* data,
                                               unsigned width,
                                               unsigned height,
                                               std::size_t pitch) {
  if (!data || !texture_) {
    return false;
  }

  void* pixels = nullptr;
  int texture_pitch = 0;
  if (SDL_LockTexture(texture_, nullptr, &pixels, &texture_pitch) != 0) {
    return false;
  }

  const auto* source = static_cast<const std::uint8_t*>(data);
  auto* destination = static_cast<std::uint8_t*>(pixels);
  for (unsigned row = 0; row < height; ++row) {
    std::memcpy(destination + row * texture_pitch,
                source + row * pitch,
                std::min<std::size_t>(pitch, static_cast<std::size_t>(texture_pitch)));
  }

  SDL_UnlockTexture(texture_);
  frame_ready_ = true;
  return true;
}

void SoftwareVideoBackend::UploadOverlayFrame(const std::uint8_t* rgba_pixels,
                                              unsigned width,
                                              unsigned height) {
  if (!renderer_ || !rgba_pixels) {
    return;
  }

  if (!overlay_texture_ || width != overlay_width_ || height != overlay_height_) {
    if (overlay_texture_) {
      SDL_DestroyTexture(overlay_texture_);
      overlay_texture_ = nullptr;
    }
    overlay_texture_ = SDL_CreateTexture(renderer_,
                                         SDL_PIXELFORMAT_ABGR8888,
                                         SDL_TEXTUREACCESS_STREAMING,
                                         static_cast<int>(width),
                                         static_cast<int>(height));
    if (!overlay_texture_) {
      return;
    }
    SDL_SetTextureBlendMode(overlay_texture_, SDL_BLENDMODE_BLEND);
    overlay_width_ = width;
    overlay_height_ = height;
  }

  SDL_UpdateTexture(overlay_texture_, nullptr, rgba_pixels, static_cast<int>(width * 4));
  overlay_ready_ = true;
}

void SoftwareVideoBackend::ClearOverlay() {
  overlay_ready_ = false;
}

void SoftwareVideoBackend::UploadChatOverlayFrame(const std::uint8_t* rgba_pixels,
                                                  unsigned width,
                                                  unsigned height) {
  if (!renderer_ || !rgba_pixels) {
    return;
  }

  if (!chat_overlay_texture_ || width != chat_overlay_width_ || height != chat_overlay_height_) {
    if (chat_overlay_texture_) {
      SDL_DestroyTexture(chat_overlay_texture_);
      chat_overlay_texture_ = nullptr;
    }
    chat_overlay_texture_ = SDL_CreateTexture(renderer_,
                                              SDL_PIXELFORMAT_ABGR8888,
                                              SDL_TEXTUREACCESS_STREAMING,
                                              static_cast<int>(width),
                                              static_cast<int>(height));
    if (!chat_overlay_texture_) {
      return;
    }
    SDL_SetTextureBlendMode(chat_overlay_texture_, SDL_BLENDMODE_BLEND);
    chat_overlay_width_ = width;
    chat_overlay_height_ = height;
  }

  SDL_UpdateTexture(chat_overlay_texture_, nullptr, rgba_pixels, static_cast<int>(width * 4));
  chat_overlay_ready_ = true;
}

void SoftwareVideoBackend::ClearChatOverlay() {
  chat_overlay_ready_ = false;
}

void SoftwareVideoBackend::NotifyHardwareFrame(const void*, unsigned, unsigned, std::size_t) {}

void SoftwareVideoBackend::SetMenuVisible(bool visible, const VideoGeometry& geometry) {
  menu_visible_ = visible;
  geometry_ = geometry;
  ApplyWindowSizing(geometry_);
}

void SoftwareVideoBackend::SetTrackerSidebarLayout(bool enabled,
                                                   unsigned sidebar_width,
                                                   const VideoGeometry& geometry) {
  tracker_sidebar_enabled_ = enabled;
  tracker_sidebar_width_ = enabled ? sidebar_width : 0;
  geometry_ = geometry;
  ApplyWindowSizing(geometry_);
}

bool SoftwareVideoBackend::ToggleFullscreen(std::string& error) {
  if (!window_) {
    error = "Software video window is not initialized.";
    return false;
  }
  const Uint32 flag = fullscreen_ ? 0u : SDL_WINDOW_FULLSCREEN_DESKTOP;
  if (SDL_SetWindowFullscreen(window_, flag) != 0) {
    error = std::string("SDL_SetWindowFullscreen failed: ") + SDL_GetError();
    return false;
  }
  fullscreen_ = !fullscreen_;
  if (!fullscreen_) {
    ApplyWindowSizing(geometry_);
  }
  return true;
}

void SoftwareVideoBackend::Present() {
  if (!renderer_ || !texture_) {
    return;
  }

  SDL_RenderClear(renderer_);
  const SDL_Rect presentation_rect =
      ComputePresentationRect(renderer_, geometry_, tracker_sidebar_enabled_, tracker_sidebar_width_);
  SDL_RenderCopy(renderer_, texture_, nullptr, &presentation_rect);
  if (overlay_ready_ && overlay_texture_) {
    SDL_RenderCopy(renderer_, overlay_texture_, nullptr, nullptr);
  }
  if (chat_overlay_ready_ && chat_overlay_texture_) {
    SDL_RenderCopy(renderer_, chat_overlay_texture_, nullptr, nullptr);
  }
  SDL_RenderPresent(renderer_);
  frame_ready_ = false;
}

void SoftwareVideoBackend::Shutdown() {
  if (texture_) {
    SDL_DestroyTexture(texture_);
    texture_ = nullptr;
  }
  if (overlay_texture_) {
    SDL_DestroyTexture(overlay_texture_);
    overlay_texture_ = nullptr;
  }
  if (chat_overlay_texture_) {
    SDL_DestroyTexture(chat_overlay_texture_);
    chat_overlay_texture_ = nullptr;
  }
  if (renderer_) {
    SDL_DestroyRenderer(renderer_);
    renderer_ = nullptr;
  }
  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  frame_ready_ = false;
  overlay_ready_ = false;
  chat_overlay_ready_ = false;
}

bool SoftwareVideoBackend::SupportsHardwareContext(retro_hw_context_type) const {
  return false;
}

bool SoftwareVideoBackend::PrepareHardwareContext(retro_hw_render_callback&,
                                                  const VideoGeometry&,
                                                  std::string& error) {
  error = "Software backend does not support hardware contexts.";
  return false;
}

bool SoftwareVideoBackend::ActivateHardwareContext(std::string& error) {
  error = "Software backend does not support hardware contexts.";
  return false;
}

bool SoftwareVideoBackend::RecreateTexture(const VideoGeometry& geometry, std::string& error) {
  if (!renderer_) {
    error = "SDL renderer not initialized.";
    return false;
  }

  if (texture_) {
    SDL_DestroyTexture(texture_);
    texture_ = nullptr;
  }

  texture_ = SDL_CreateTexture(renderer_,
                               ToSdlPixelFormat(geometry.pixel_format),
                               SDL_TEXTUREACCESS_STREAMING,
                               static_cast<int>(geometry.width),
                               static_cast<int>(geometry.height));
  if (!texture_) {
    error = std::string("SDL_CreateTexture failed: ") + SDL_GetError();
    return false;
  }

  frame_ready_ = false;
  return true;
}

int SoftwareVideoBackend::ScaledDimension(unsigned size) const {
  return std::max(static_cast<int>(size * kWindowScale), static_cast<int>(size));
}

void SoftwareVideoBackend::ApplyWindowSizing(const VideoGeometry& geometry) {
  if (!window_) {
    return;
  }
  if (fullscreen_) {
    return;
  }

  const unsigned base_width = geometry.width == 0 ? 320u : geometry.width;
  const unsigned base_height = geometry.height == 0 ? 240u : geometry.height;

  int target_width = ScaledDimension(base_width);
  int target_height = ScaledDimension(base_height);

  if (menu_visible_) {
    const int scale_x =
        std::max(1, (kMenuMinimumWidth + static_cast<int>(base_width) - 1) / static_cast<int>(base_width));
    const int scale_y =
        std::max(1, (kMenuMinimumHeight + static_cast<int>(base_height) - 1) / static_cast<int>(base_height));
    const int scale = std::max(scale_x, scale_y);
    target_width = static_cast<int>(base_width) * scale;
    target_height = static_cast<int>(base_height) * scale;
  }

  if (tracker_sidebar_enabled_ && !menu_visible_) {
    target_width += static_cast<int>(tracker_sidebar_width_);
  }

  SDL_SetWindowSize(window_, target_width, target_height);
}

}  // namespace sekaiemu::spike
