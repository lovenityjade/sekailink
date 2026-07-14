#include "opengl_loader.hpp"
#include "opengl_video_backend.hpp"
#include "opengl_video_utils.hpp"
#include "window_chrome.hpp"

#include <algorithm>
#include <cstring>
#include <iostream>
#include <utility>

namespace sekaiemu::spike {

namespace {

constexpr auto kSoftwareContextType = RETRO_HW_CONTEXT_OPENGL_CORE;
constexpr int kClientCoreTitlebarHeight = 34;

std::uint32_t ReadU32(const std::uint8_t* source) {
  std::uint32_t value = 0;
  std::memcpy(&value, source, sizeof(value));
  return value;
}

std::uint16_t ReadU16(const std::uint8_t* source) {
  std::uint16_t value = 0;
  std::memcpy(&value, source, sizeof(value));
  return value;
}

std::uint8_t Expand5(std::uint16_t value) {
  return static_cast<std::uint8_t>((value * 255u) / 31u);
}

std::uint8_t Expand6(std::uint16_t value) {
  return static_cast<std::uint8_t>((value * 255u) / 63u);
}

std::size_t BytesPerPixel(retro_pixel_format pixel_format) {
  switch (pixel_format) {
    case RETRO_PIXEL_FORMAT_RGB565:
    case RETRO_PIXEL_FORMAT_0RGB1555:
      return 2;
    case RETRO_PIXEL_FORMAT_XRGB8888:
    default:
      return 4;
  }
}

bool IsVisiblePixel(const std::uint8_t* source, retro_pixel_format pixel_format) {
  if (pixel_format == RETRO_PIXEL_FORMAT_RGB565 || pixel_format == RETRO_PIXEL_FORMAT_0RGB1555) {
    return ReadU16(source) != 0;
  }
  return (ReadU32(source) & 0x00ffffffu) != 0;
}

std::size_t CountVisiblePixels(const void* data,
                               unsigned start_x,
                               unsigned width,
                               unsigned height,
                               std::size_t pitch,
                               retro_pixel_format pixel_format) {
  if (!data || width == 0 || height == 0) {
    return 0;
  }
  const auto bytes_per_pixel = BytesPerPixel(pixel_format);
  const auto* rows = static_cast<const std::uint8_t*>(data);
  std::size_t visible = 0;
  for (unsigned y = 0; y < height; ++y) {
    const auto* row = rows + static_cast<std::size_t>(y) * pitch;
    for (unsigned x = 0; x < width; ++x) {
      if (IsVisiblePixel(row + static_cast<std::size_t>(start_x + x) * bytes_per_pixel, pixel_format)) {
        ++visible;
      }
    }
  }
  return visible;
}

unsigned ResolveSoftwareFrameWidth(const void* data,
                                   unsigned callback_width,
                                   unsigned height,
                                   std::size_t pitch,
                                   retro_pixel_format pixel_format) {
  const auto bytes_per_pixel = BytesPerPixel(pixel_format);
  if (!data || callback_width == 0 || height == 0 || bytes_per_pixel == 0) {
    return callback_width;
  }
  const auto stride_width = static_cast<unsigned>(pitch / bytes_per_pixel);
  if (stride_width <= callback_width || stride_width > 1024 || stride_width < callback_width * 2u) {
    return callback_width;
  }

  const auto right_visible = CountVisiblePixels(data, callback_width, callback_width, height, pitch, pixel_format);
  const auto right_visible_threshold = static_cast<std::size_t>(callback_width) * height / 128u;
  if (right_visible > right_visible_threshold) {
    return callback_width * 2u;
  }
  return callback_width;
}

void ConvertSoftwareFrameToRgba(const void* data,
                                unsigned width,
                                unsigned height,
                                std::size_t pitch,
                                retro_pixel_format pixel_format,
                                std::vector<std::uint8_t>& out) {
  out.resize(static_cast<std::size_t>(width) * height * 4u);
  const auto* source_rows = static_cast<const std::uint8_t*>(data);
  for (unsigned y = 0; y < height; ++y) {
    const auto* source = source_rows + static_cast<std::size_t>(y) * pitch;
    auto* target = out.data() + static_cast<std::size_t>(height - 1u - y) * width * 4u;
    for (unsigned x = 0; x < width; ++x) {
      std::uint8_t r = 0;
      std::uint8_t g = 0;
      std::uint8_t b = 0;
      if (pixel_format == RETRO_PIXEL_FORMAT_RGB565) {
        const std::uint16_t pixel = ReadU16(source + static_cast<std::size_t>(x) * 2u);
        r = Expand5(static_cast<std::uint16_t>((pixel >> 11) & 0x1fu));
        g = Expand6(static_cast<std::uint16_t>((pixel >> 5) & 0x3fu));
        b = Expand5(static_cast<std::uint16_t>(pixel & 0x1fu));
      } else if (pixel_format == RETRO_PIXEL_FORMAT_0RGB1555) {
        const std::uint16_t pixel = ReadU16(source + static_cast<std::size_t>(x) * 2u);
        r = Expand5(static_cast<std::uint16_t>((pixel >> 10) & 0x1fu));
        g = Expand5(static_cast<std::uint16_t>((pixel >> 5) & 0x1fu));
        b = Expand5(static_cast<std::uint16_t>(pixel & 0x1fu));
      } else {
        const std::uint32_t pixel = ReadU32(source + static_cast<std::size_t>(x) * 4u);
        r = static_cast<std::uint8_t>((pixel >> 16) & 0xffu);
        g = static_cast<std::uint8_t>((pixel >> 8) & 0xffu);
        b = static_cast<std::uint8_t>(pixel & 0xffu);
      }
      *target++ = r;
      *target++ = g;
      *target++ = b;
      *target++ = 0xffu;
    }
  }
}

}  // namespace

OpenGlVideoBackend::~OpenGlVideoBackend() { Shutdown(); }

bool OpenGlVideoBackend::Initialize(const VideoGeometry& geometry, std::string& error) {
  geometry_ = geometry;
  active_frame_width_ = geometry.width;
  active_frame_height_ = geometry.height;
  if (!CreateSoftwareContext(geometry, error)) {
    return false;
  }
  ApplyWindowSizing(geometry);
  return EnsureCoreFramebufferResources(error);
}

bool OpenGlVideoBackend::UpdateGeometry(const VideoGeometry& geometry, std::string& error) {
  const bool had_window = window_ != nullptr;
  geometry_ = geometry;
  active_frame_width_ = geometry.width;
  active_frame_height_ = geometry.height;
  if (!CreateSoftwareContext(geometry, error)) {
    return false;
  }
  if (!had_window) {
    ApplyWindowSizing(geometry);
  }
  return true;
}

bool OpenGlVideoBackend::UploadSoftwareFrame(const void* data,
                                             unsigned width,
                                             unsigned height,
                                             std::size_t pitch) {
  if (!data || data == RETRO_HW_FRAME_BUFFER_VALID || width == 0 || height == 0 || !window_ || !gl_context_) {
    return false;
  }

  SDL_GL_MakeCurrent(window_, gl_context_);
  const auto bytes_per_pixel = BytesPerPixel(geometry_.pixel_format);
  const auto stride_width = bytes_per_pixel > 0 ? static_cast<unsigned>(pitch / bytes_per_pixel) : width;
  if (stride_width > width &&
      (!stride_probe_logged_ || stride_probe_sample_count_ < 3 || !stride_probe_visible_logged_)) {
    const auto left_visible = CountVisiblePixels(data, 0, width, height, pitch, geometry_.pixel_format);
    const auto right_visible = stride_width >= width * 2u
      ? CountVisiblePixels(data, width, width, height, pitch, geometry_.pixel_format)
      : 0u;
    const bool has_visible_pixels = left_visible > 0 || right_visible > 0;
    const bool should_log_sample =
        !stride_probe_logged_ || stride_probe_sample_count_ < 3 ||
        (!stride_probe_visible_logged_ && has_visible_pixels);
    if (should_log_sample) {
      std::cerr << "[sekaiemu-libretro-spike] software frame stride probe callback="
                << width << " stride_width=" << stride_width
                << " height=" << height
                << " pitch=" << pitch
                << " left_visible=" << left_visible
                << " right_visible=" << right_visible
                << " sample=" << stride_probe_sample_count_ << "\n";
    }
    stride_probe_logged_ = true;
    if (has_visible_pixels) {
      stride_probe_visible_logged_ = true;
    }
    ++stride_probe_sample_count_;
  }
  const unsigned display_width = ResolveSoftwareFrameWidth(data, width, height, pitch, geometry_.pixel_format);
  if (display_width != width && !stride_width_logged_) {
    std::cerr << "[sekaiemu-libretro-spike] software frame widened from callback width "
              << width << " to stride width " << display_width
              << " pitch=" << pitch << "\n";
    stride_width_logged_ = true;
  }
  if (active_frame_width_ != display_width || active_frame_height_ != height) {
    active_frame_width_ = display_width;
    active_frame_height_ = height;
    DestroyCoreFramebufferResources();
  }

  std::string error;
  if (!EnsureCoreFramebufferResources(error)) {
    std::cerr << "[sekaiemu-libretro-spike] OpenGL software frame texture failed: "
              << error << "\n";
    return false;
  }

  ConvertSoftwareFrameToRgba(data, display_width, height, pitch, geometry_.pixel_format, software_frame_rgba_);
  glBindTexture(GL_TEXTURE_2D, core_color_texture_);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
  glTexSubImage2D(GL_TEXTURE_2D,
                  0,
                  0,
                  0,
                  static_cast<GLsizei>(display_width),
                  static_cast<GLsizei>(height),
                  GL_RGBA,
                  GL_UNSIGNED_BYTE,
                  software_frame_rgba_.data());
  glBindTexture(GL_TEXTURE_2D, 0);
  frame_ready_ = true;
  return true;
}

void OpenGlVideoBackend::UploadOverlayFrame(const std::uint8_t* rgba_pixels,
                                            unsigned width,
                                            unsigned height) {
  if (!rgba_pixels || width == 0 || height == 0) {
    overlay_ready_ = false;
    overlay_pixels_dirty_ = false;
    overlay_pixels_.clear();
    return;
  }

  overlay_width_ = width;
  overlay_height_ = height;
  overlay_pixels_.assign(rgba_pixels, rgba_pixels + static_cast<std::size_t>(width) * height * 4u);
  overlay_ready_ = true;
  overlay_pixels_dirty_ = true;
  frame_ready_ = true;
}

void OpenGlVideoBackend::ClearOverlay() {
  overlay_ready_ = false;
  overlay_pixels_dirty_ = false;
  overlay_pixels_.clear();
}

void OpenGlVideoBackend::UploadChatOverlayFrame(const std::uint8_t* rgba_pixels,
                                                unsigned width,
                                                unsigned height) {
  if (!rgba_pixels || width == 0 || height == 0) {
    chat_overlay_ready_ = false;
    chat_overlay_pixels_dirty_ = false;
    chat_overlay_pixels_.clear();
    return;
  }

  chat_overlay_width_ = width;
  chat_overlay_height_ = height;
  chat_overlay_pixels_.assign(rgba_pixels, rgba_pixels + static_cast<std::size_t>(width) * height * 4u);
  chat_overlay_ready_ = true;
  chat_overlay_pixels_dirty_ = true;
  frame_ready_ = true;
}

void OpenGlVideoBackend::ClearChatOverlay() {
  chat_overlay_ready_ = false;
  chat_overlay_pixels_dirty_ = false;
  chat_overlay_pixels_.clear();
}

void OpenGlVideoBackend::NotifyHardwareFrame(const void* data,
                                             unsigned width,
                                             unsigned height,
                                             std::size_t pitch) {
  if (data && data != RETRO_HW_FRAME_BUFFER_VALID) {
    UploadSoftwareFrame(data, width, height, pitch);
    return;
  }

  if (width > 0 && height > 0) {
    if ((core_frame_width_ != 0 && core_frame_width_ != width) ||
        (core_frame_height_ != 0 && core_frame_height_ != height)) {
      DestroyCoreFramebufferResources();
    }
    geometry_.width = width;
    geometry_.height = height;
  }
  frame_ready_ = true;
}

void OpenGlVideoBackend::SetMenuVisible(bool visible, const VideoGeometry& geometry) {
  const bool changed = menu_visible_ != visible;
  if (changed && visible && window_ && window_mode_ != WindowMode::Fullscreen) {
    SDL_GetWindowSize(window_, &menu_restore_width_, &menu_restore_height_);
    menu_restore_size_valid_ = menu_restore_width_ > 0 && menu_restore_height_ > 0;
  }
  menu_visible_ = visible;
  geometry_ = geometry;
  if (changed && !visible && window_ && window_mode_ != WindowMode::Fullscreen && menu_restore_size_valid_) {
    SDL_SetWindowSize(window_, menu_restore_width_, menu_restore_height_);
    menu_restore_size_valid_ = false;
    frame_ready_ = true;
    return;
  }
  if (changed && visible) {
    ApplyWindowSizing(geometry_);
  }
  frame_ready_ = true;
}

void OpenGlVideoBackend::SetTrackerSidebarLayout(bool enabled,
                                                 unsigned sidebar_width,
                                                 const VideoGeometry& geometry) {
  const unsigned effective_sidebar_width = enabled ? sidebar_width : 0;
  const bool changed = tracker_sidebar_enabled_ != enabled ||
                       tracker_sidebar_width_ != effective_sidebar_width;
  tracker_sidebar_enabled_ = enabled;
  tracker_sidebar_width_ = effective_sidebar_width;
  geometry_ = geometry;
  if (changed && enabled && !menu_visible_) {
    ApplyWindowSizing(geometry_);
  }
  frame_ready_ = true;
}

bool OpenGlVideoBackend::SetWindowMode(WindowMode mode, std::string& error) {
  if (!window_) {
    error = "OpenGL window is not initialized.";
    return false;
  }
  const Uint32 fullscreen_flag = mode == WindowMode::Fullscreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0u;
  if (SDL_SetWindowFullscreen(window_, fullscreen_flag) != 0) {
    error = std::string("SDL_SetWindowFullscreen failed: ") + SDL_GetError();
    return false;
  }
  window_mode_ = mode;
  if (mode != WindowMode::Fullscreen) {
    SDL_SetWindowBordered(window_, mode == WindowMode::Window ? SDL_TRUE : SDL_FALSE);
    ApplyWindowSizing(geometry_);
  }
  frame_ready_ = true;
  return true;
}

void OpenGlVideoBackend::SetImGuiDrawCallback(std::function<void()> callback) {
  imgui_draw_callback_ = std::move(callback);
  frame_ready_ = true;
}

void OpenGlVideoBackend::Present() {
  if (!window_ || !gl_context_ || (!frame_ready_ && !overlay_ready_ && !imgui_draw_callback_)) {
    return;
  }

  SDL_GL_MakeCurrent(window_, gl_context_);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  glDisable(GL_SCISSOR_TEST);
  glDisable(GL_STENCIL_TEST);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  int drawable_width = 0;
  int drawable_height = 0;
  SDL_GL_GetDrawableSize(window_, &drawable_width, &drawable_height);
  glViewport(0, 0, std::max(1, drawable_width), std::max(1, drawable_height));
  glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);
  std::string error;
  EnsureCoreFramebufferResources(error);
  DrawHardwareFrame();
  if (overlay_ready_) {
    DrawOverlay();
  }
  if (chat_overlay_ready_) {
    DrawChatOverlay();
  }
  if (imgui_draw_callback_) {
    std::string imgui_error;
    if (imgui_runtime_.Initialize(window_, gl_context_, imgui_error)) {
      imgui_runtime_.Render(imgui_draw_callback_);
    }
  }
  SDL_GL_SwapWindow(window_);
  frame_ready_ = false;
}

void OpenGlVideoBackend::Shutdown() {
  if (window_ && gl_context_) {
    SDL_GL_MakeCurrent(window_, gl_context_);
  }

  overlay_renderer_.Destroy();
  chat_overlay_renderer_.Destroy();
  imgui_runtime_.Shutdown();
  DestroyCoreFramebufferResources();

  if (hw_context_ready_ && hw_callback_.context_destroy) {
    SDL_GL_MakeCurrent(window_, gl_context_);
    hw_callback_.context_destroy();
  }
  hw_context_ready_ = false;

  if (gl_context_) {
    SDL_GL_DeleteContext(gl_context_);
    gl_context_ = nullptr;
  }
  if (window_) {
    SDL_DestroyWindow(window_);
    window_ = nullptr;
  }
  if (active_backend_ == this) {
    active_backend_ = nullptr;
  }
  frame_ready_ = false;
  overlay_ready_ = false;
  overlay_pixels_dirty_ = false;
  overlay_pixels_.clear();
  chat_overlay_ready_ = false;
  chat_overlay_pixels_dirty_ = false;
  chat_overlay_pixels_.clear();
  imgui_draw_callback_ = nullptr;
}

bool OpenGlVideoBackend::SupportsHardwareContext(retro_hw_context_type context_type) const {
  return IsOpenGlLike(context_type);
}

bool OpenGlVideoBackend::PrepareHardwareContext(retro_hw_render_callback& callback,
                                                const VideoGeometry& geometry,
                                                std::string& error) {
  if (!SupportsHardwareContext(callback.context_type)) {
    error = "OpenGL backend does not support the requested hardware context.";
    return false;
  }

  geometry_ = geometry;
  if (!CreateContext(callback, geometry, error)) {
    return false;
  }

  callback.get_current_framebuffer = &OpenGlVideoBackend::StaticGetCurrentFramebuffer;
  callback.get_proc_address = &OpenGlVideoBackend::StaticGetProcAddress;

  hw_callback_ = callback;
  active_backend_ = this;

  return true;
}

bool OpenGlVideoBackend::ActivateHardwareContext(std::string& error) {
  if (!window_ || !gl_context_) {
    error = "OpenGL context has not been prepared.";
    return false;
  }

  if (hw_context_ready_) {
    return true;
  }

  SDL_GL_MakeCurrent(window_, gl_context_);
  if (!EnsureCoreFramebufferResources(error)) {
    return false;
  }
  if (hw_callback_.context_reset) {
    hw_callback_.context_reset();
  }
  hw_context_ready_ = true;
  return true;
}

uintptr_t OpenGlVideoBackend::StaticGetCurrentFramebuffer() {
  return active_backend_ != nullptr ? active_backend_->CurrentFramebuffer() : 0;
}

retro_proc_address_t OpenGlVideoBackend::StaticGetProcAddress(const char* symbol) {
  if (!symbol) {
    return nullptr;
  }
  return reinterpret_cast<retro_proc_address_t>(SDL_GL_GetProcAddress(symbol));
}

bool OpenGlVideoBackend::EnsureWindow(const VideoGeometry& geometry, std::string& error) {
  if (!window_) {
    window_ = SDL_CreateWindow("Sekaiemu",
                               SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED,
                               WindowWidth(geometry.width),
                               WindowHeight(geometry.height),
                               SDL_WINDOW_SHOWN | SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE |
                                   SDL_WINDOW_BORDERLESS);
    if (!window_) {
      error = std::string("SDL_CreateWindow failed: ") + SDL_GetError();
      return false;
    }
    EnableBorderlessTitlebarDrag(window_);
  }
  return true;
}

bool OpenGlVideoBackend::CreateSoftwareContext(const VideoGeometry& geometry, std::string& error) {
  if (gl_context_) {
    return true;
  }

  retro_hw_render_callback callback{};
  callback.context_type = kSoftwareContextType;
  callback.version_major = 3;
  callback.version_minor = 3;
  hw_callback_ = callback;
  active_backend_ = this;
  return CreateContext(callback, geometry, error);
}

uintptr_t OpenGlVideoBackend::CurrentFramebuffer() const {
  return core_framebuffer_ != 0 ? core_framebuffer_ : 0;
}

void OpenGlVideoBackend::ApplyWindowSizing(const VideoGeometry& geometry) {
  if (!window_) {
    return;
  }
  if (window_mode_ == WindowMode::Fullscreen) {
    return;
  }

  const unsigned base_width = geometry.width == 0 ? 640u : geometry.width;
  const unsigned base_height = geometry.height == 0 ? 480u : geometry.height;

  int target_width = ScaledDimension(base_width);
  int target_height = ScaledDimension(base_height) + kClientCoreTitlebarHeight;

  if (menu_visible_) {
    const int scale_x =
        std::max(1, (kMenuMinimumWidth + static_cast<int>(base_width) - 1) / static_cast<int>(base_width));
    const int scale_y =
        std::max(1, (kMenuMinimumHeight + static_cast<int>(base_height) - 1) / static_cast<int>(base_height));
    const int scale = std::max(scale_x, scale_y);
    target_width = static_cast<int>(base_width) * scale;
    target_height = static_cast<int>(base_height) * scale + kClientCoreTitlebarHeight;
  }

  if (tracker_sidebar_enabled_ && !menu_visible_) {
    target_width += static_cast<int>(tracker_sidebar_width_);
  }

  int current_width = 0;
  int current_height = 0;
  SDL_GetWindowSize(window_, &current_width, &current_height);
  if (current_width == target_width && current_height == target_height) {
    return;
  }

  SDL_SetWindowSize(window_, target_width, target_height);
}

bool OpenGlVideoBackend::EnsureCoreFramebufferResources(std::string& error) {
  const unsigned width = active_frame_width_ == 0 ? (geometry_.width == 0 ? 1u : geometry_.width) : active_frame_width_;
  const unsigned height = active_frame_height_ == 0 ? (geometry_.height == 0 ? 1u : geometry_.height) : active_frame_height_;
  if (core_framebuffer_ != 0 && core_frame_width_ == width && core_frame_height_ == height) {
    return true;
  }

  DestroyCoreFramebufferResources();

  glGenFramebuffers(1, &core_framebuffer_);
  glBindFramebuffer(GL_FRAMEBUFFER, core_framebuffer_);

  glGenTextures(1, &core_color_texture_);
  glBindTexture(GL_TEXTURE_2D, core_color_texture_);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGBA,
               static_cast<GLsizei>(width),
               static_cast<GLsizei>(height),
               0,
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               nullptr);
  glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, core_color_texture_, 0);

  if (hw_callback_.depth || hw_callback_.stencil) {
    glGenRenderbuffers(1, &core_depth_stencil_rbo_);
    glBindRenderbuffer(GL_RENDERBUFFER, core_depth_stencil_rbo_);
    const GLenum storage = hw_callback_.stencil ? GL_DEPTH24_STENCIL8 : GL_DEPTH_COMPONENT24;
    glRenderbufferStorage(GL_RENDERBUFFER,
                          storage,
                          static_cast<GLsizei>(width),
                          static_cast<GLsizei>(height));
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,
                              hw_callback_.stencil ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER,
                              core_depth_stencil_rbo_);
  }

  if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
    error = "OpenGL core framebuffer setup failed.";
    DestroyCoreFramebufferResources();
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    return false;
  }

  glBindTexture(GL_TEXTURE_2D, 0);
  glBindRenderbuffer(GL_RENDERBUFFER, 0);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
  core_frame_width_ = width;
  core_frame_height_ = height;
  return true;
}

void OpenGlVideoBackend::DestroyCoreFramebufferResources() {
  if (core_depth_stencil_rbo_ != 0) {
    glDeleteRenderbuffers(1, &core_depth_stencil_rbo_);
    core_depth_stencil_rbo_ = 0;
  }
  if (core_color_texture_ != 0) {
    glDeleteTextures(1, &core_color_texture_);
    core_color_texture_ = 0;
  }
  if (core_framebuffer_ != 0) {
    glDeleteFramebuffers(1, &core_framebuffer_);
    core_framebuffer_ = 0;
  }
  core_frame_width_ = 0;
  core_frame_height_ = 0;
}

void OpenGlVideoBackend::DrawHardwareFrame() {
  if (core_framebuffer_ == 0 || core_frame_width_ == 0 || core_frame_height_ == 0) {
    return;
  }

  int window_width = 0;
  int window_height = 0;
  SDL_GL_GetDrawableSize(window_, &window_width, &window_height);
  const auto viewport =
      ComputeGameViewport(window_width, window_height, geometry_, tracker_sidebar_enabled_, tracker_sidebar_width_);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, core_framebuffer_);
  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
  glBlitFramebuffer(0,
                    0,
                    static_cast<GLint>(core_frame_width_),
                    static_cast<GLint>(core_frame_height_),
                    viewport.x,
                    viewport.y,
                    viewport.x + viewport.width,
                    viewport.y + viewport.height,
                    GL_COLOR_BUFFER_BIT,
                    GL_NEAREST);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void OpenGlVideoBackend::DrawOverlay() {
  if (overlay_pixels_.empty()) {
    return;
  }

  std::string error;
  int window_width = 0;
  int window_height = 0;
  SDL_GL_GetDrawableSize(window_, &window_width, &window_height);
  overlay_renderer_.Draw(hw_callback_.context_type,
                         overlay_pixels_,
                         overlay_width_,
                         overlay_height_,
                         window_width,
                         window_height,
                         overlay_pixels_dirty_,
                         error);
  overlay_pixels_dirty_ = false;
}

void OpenGlVideoBackend::DrawChatOverlay() {
  if (chat_overlay_pixels_.empty()) {
    return;
  }

  std::string error;
  int window_width = 0;
  int window_height = 0;
  SDL_GL_GetDrawableSize(window_, &window_width, &window_height);
  chat_overlay_renderer_.Draw(hw_callback_.context_type,
                              chat_overlay_pixels_,
                              chat_overlay_width_,
                              chat_overlay_height_,
                              window_width,
                              window_height,
                              chat_overlay_pixels_dirty_,
                              error);
  chat_overlay_pixels_dirty_ = false;
}

bool OpenGlVideoBackend::CreateContext(const retro_hw_render_callback& callback,
                                       const VideoGeometry& geometry,
                                       std::string& error) {
  if (!EnsureWindow(geometry, error)) {
    return false;
  }

  if (gl_context_) {
    return true;
  }

  ApplyContextAttributes(callback);

  gl_context_ = SDL_GL_CreateContext(window_);
  if (!gl_context_) {
    error = std::string("SDL_GL_CreateContext failed: ") + SDL_GetError();
    return false;
  }

  if (SDL_GL_MakeCurrent(window_, gl_context_) != 0) {
    error = std::string("SDL_GL_MakeCurrent failed: ") + SDL_GetError();
    return false;
  }

  if (!LoadOpenGlFunctions(error)) {
    return false;
  }

  SDL_GL_SetSwapInterval(0);
  if (!context_logged_) {
    const auto* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    const auto* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    std::cerr << "[sekaiemu-libretro-spike] OpenGL video backend active: renderer="
              << (renderer != nullptr ? renderer : "unknown")
              << " version=" << (version != nullptr ? version : "unknown")
              << " swap_interval=" << SDL_GL_GetSwapInterval() << "\n";
    context_logged_ = true;
  }
  return true;
}

void OpenGlVideoBackend::ApplyContextAttributes(const retro_hw_render_callback& callback) {
  SDL_GL_ResetAttributes();
  SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
  SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, callback.depth ? 24 : 0);
  SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, callback.stencil ? 8 : 0);

  switch (callback.context_type) {
    case RETRO_HW_CONTEXT_OPENGL_CORE:
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
      break;
    case RETRO_HW_CONTEXT_OPENGLES2:
    case RETRO_HW_CONTEXT_OPENGLES3:
    case RETRO_HW_CONTEXT_OPENGLES_VERSION:
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
      break;
    case RETRO_HW_CONTEXT_OPENGL:
    default:
      SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
      break;
  }

  if (callback.version_major > 0) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, static_cast<int>(callback.version_major));
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, static_cast<int>(callback.version_minor));
  }

  if (callback.debug_context) {
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
  }
}

int OpenGlVideoBackend::WindowWidth(unsigned width) const {
  return ScaledDimension(width == 0 ? 640u : width);
}

int OpenGlVideoBackend::WindowHeight(unsigned height) const {
  return ScaledDimension(height == 0 ? 480u : height) + kClientCoreTitlebarHeight;
}

int OpenGlVideoBackend::ScaledDimension(unsigned size) const {
  return std::max(static_cast<int>(size * kWindowScale), static_cast<int>(size));
}

}  // namespace sekaiemu::spike
