#pragma once

#include "video_backend.hpp"

namespace sekaiemu::spike {

class VulkanVideoBackend final : public VideoBackend {
 public:
  VulkanVideoBackend() = default;
  ~VulkanVideoBackend() override = default;

  VideoBackendType Type() const override { return VideoBackendType::Vulkan; }
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
  void Present() override;
  void Shutdown() override;

  bool SupportsHardwareContext(retro_hw_context_type context_type) const override;
  bool PrepareHardwareContext(retro_hw_render_callback& callback,
                              const VideoGeometry& geometry,
                              std::string& error) override;
  bool ActivateHardwareContext(std::string& error) override;
};

}  // namespace sekaiemu::spike
