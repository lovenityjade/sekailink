#pragma once

#include "video_backend.hpp"

namespace sekaiemu::spike {

class SoftwareVideoBackend final : public VideoBackend {
 public:
  SoftwareVideoBackend() = default;
  ~SoftwareVideoBackend() override;

  VideoBackendType Type() const override { return VideoBackendType::Software; }
  bool IsHardware() const override { return false; }

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
  bool SetWindowMode(WindowMode mode, std::string& error) override;
  WindowMode CurrentWindowMode() const override { return window_mode_; }
  void Present() override;
  void Shutdown() override;

  bool SupportsHardwareContext(retro_hw_context_type context_type) const override;
  bool PrepareHardwareContext(retro_hw_render_callback& callback,
                              const VideoGeometry& geometry,
                              std::string& error) override;
  bool ActivateHardwareContext(std::string& error) override;

 private:
  bool RecreateTexture(const VideoGeometry& geometry, std::string& error);
  int ScaledDimension(unsigned size) const;
  void ApplyWindowSizing(const VideoGeometry& geometry);

  static constexpr int kWindowScale = 3;
  static constexpr int kMenuMinimumWidth = 960;
  static constexpr int kMenuMinimumHeight = 720;

  SDL_Window* window_ = nullptr;
  SDL_Renderer* renderer_ = nullptr;
  SDL_Texture* texture_ = nullptr;
  SDL_Texture* overlay_texture_ = nullptr;
  SDL_Texture* chat_overlay_texture_ = nullptr;
  VideoGeometry geometry_{};
  bool frame_ready_ = false;
  bool overlay_ready_ = false;
  bool chat_overlay_ready_ = false;
  unsigned overlay_width_ = 0;
  unsigned overlay_height_ = 0;
  unsigned chat_overlay_width_ = 0;
  unsigned chat_overlay_height_ = 0;
  bool menu_visible_ = false;
  bool menu_restore_size_valid_ = false;
  int menu_restore_width_ = 0;
  int menu_restore_height_ = 0;
  bool tracker_sidebar_enabled_ = false;
  unsigned tracker_sidebar_width_ = 0;
  WindowMode window_mode_ = WindowMode::BorderlessWindow;
};

}  // namespace sekaiemu::spike
