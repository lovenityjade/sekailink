#pragma once

#include "opengl_overlay_renderer.hpp"
#include "video_backend.hpp"
#include "imgui_runtime.hpp"

#include <functional>
#include <vector>

namespace sekaiemu::spike {

class OpenGlVideoBackend final : public VideoBackend {
 public:
  OpenGlVideoBackend() = default;
  ~OpenGlVideoBackend() override;

  VideoBackendType Type() const override { return VideoBackendType::OpenGl; }
  bool IsHardware() const override { return true; }

  bool Initialize(const VideoGeometry& geometry, std::string& error) override;
  bool UpdateGeometry(const VideoGeometry& geometry, std::string& error) override;

  bool UploadSoftwareFrame(const void* data,
                           unsigned width,
                           unsigned height,
                           std::size_t pitch) override;
  void UploadOverlayFrame(const std::uint8_t* rgba_pixels,
                          unsigned width,
                          unsigned height) override;
  void ClearOverlay() override;
  void UploadChatOverlayFrame(const std::uint8_t* rgba_pixels,
                              unsigned width,
                              unsigned height) override;
  void ClearChatOverlay() override;
  void NotifyHardwareFrame(const void* data,
                           unsigned width,
                           unsigned height,
                           std::size_t pitch) override;
  void SetMenuVisible(bool visible, const VideoGeometry& geometry) override;
  void SetTrackerSidebarLayout(bool enabled,
                               unsigned sidebar_width,
                               const VideoGeometry& geometry) override;
  bool ToggleFullscreen(std::string& error) override;
  void SetImGuiDrawCallback(std::function<void()> callback) override;
  void Present() override;
  void Shutdown() override;

  bool SupportsHardwareContext(retro_hw_context_type context_type) const override;
  bool PrepareHardwareContext(retro_hw_render_callback& callback,
                              const VideoGeometry& geometry,
                              std::string& error) override;
  bool ActivateHardwareContext(std::string& error) override;

 private:
  static uintptr_t StaticGetCurrentFramebuffer();
  static retro_proc_address_t StaticGetProcAddress(const char* symbol);
  uintptr_t CurrentFramebuffer() const;

  bool EnsureWindow(const VideoGeometry& geometry, std::string& error);
  bool CreateSoftwareContext(const VideoGeometry& geometry, std::string& error);
  bool CreateContext(const retro_hw_render_callback& callback,
                     const VideoGeometry& geometry,
                     std::string& error);
  void ApplyContextAttributes(const retro_hw_render_callback& callback);
  bool EnsureCoreFramebufferResources(std::string& error);
  void DestroyCoreFramebufferResources();
  void DrawHardwareFrame();
  void DrawOverlay();
  void DrawChatOverlay();
  void ApplyWindowSizing(const VideoGeometry& geometry);
  int ScaledDimension(unsigned size) const;
  int WindowWidth(unsigned width) const;
  int WindowHeight(unsigned height) const;

  static constexpr int kWindowScale = 3;
  static constexpr int kMenuMinimumWidth = 960;
  static constexpr int kMenuMinimumHeight = 720;

  static inline OpenGlVideoBackend* active_backend_ = nullptr;

  SDL_Window* window_ = nullptr;
  SDL_GLContext gl_context_ = nullptr;
  VideoGeometry geometry_{};
  retro_hw_render_callback hw_callback_{};
  bool hw_context_ready_ = false;
  bool frame_ready_ = false;
  bool overlay_ready_ = false;
  bool overlay_pixels_dirty_ = false;
  unsigned overlay_width_ = 0;
  unsigned overlay_height_ = 0;
  OpenGlOverlayRenderer overlay_renderer_;
  std::vector<std::uint8_t> overlay_pixels_;
  bool chat_overlay_ready_ = false;
  bool chat_overlay_pixels_dirty_ = false;
  unsigned chat_overlay_width_ = 0;
  unsigned chat_overlay_height_ = 0;
  OpenGlOverlayRenderer chat_overlay_renderer_;
  std::vector<std::uint8_t> chat_overlay_pixels_;
  std::vector<std::uint8_t> software_frame_rgba_;
  unsigned core_framebuffer_ = 0;
  unsigned core_color_texture_ = 0;
  unsigned core_depth_stencil_rbo_ = 0;
  unsigned core_frame_width_ = 0;
  unsigned core_frame_height_ = 0;
  bool context_logged_ = false;
  bool stride_width_logged_ = false;
  bool stride_probe_logged_ = false;
  unsigned stride_probe_sample_count_ = 0;
  bool stride_probe_visible_logged_ = false;
  bool menu_visible_ = false;
  bool tracker_sidebar_enabled_ = false;
  unsigned tracker_sidebar_width_ = 0;
  bool fullscreen_ = false;
  SekaiemuImGuiRuntime imgui_runtime_;
  std::function<void()> imgui_draw_callback_;
};

}  // namespace sekaiemu::spike
