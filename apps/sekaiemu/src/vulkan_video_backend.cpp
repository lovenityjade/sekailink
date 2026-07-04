#include "vulkan_video_backend.hpp"

namespace sekaiemu::spike {

bool VulkanVideoBackend::Initialize(const VideoGeometry&, std::string& error) {
  error = "Vulkan backend is not implemented yet.";
  return false;
}

bool VulkanVideoBackend::UpdateGeometry(const VideoGeometry&, std::string& error) {
  error = "Vulkan backend is not implemented yet.";
  return false;
}

bool VulkanVideoBackend::UploadSoftwareFrame(const void*, unsigned, unsigned, std::size_t) {
  return false;
}

void VulkanVideoBackend::UploadOverlayFrame(const std::uint8_t*, unsigned, unsigned) {}

void VulkanVideoBackend::ClearOverlay() {}

void VulkanVideoBackend::UploadChatOverlayFrame(const std::uint8_t*, unsigned, unsigned) {}

void VulkanVideoBackend::ClearChatOverlay() {}

void VulkanVideoBackend::NotifyHardwareFrame(const void*, unsigned, unsigned, std::size_t) {}

void VulkanVideoBackend::SetMenuVisible(bool, const VideoGeometry&) {}

void VulkanVideoBackend::SetTrackerSidebarLayout(bool, unsigned, const VideoGeometry&) {}

bool VulkanVideoBackend::SetWindowMode(WindowMode, std::string& error) {
  error = "Vulkan backend is not implemented yet.";
  return false;
}

void VulkanVideoBackend::Present() {}

void VulkanVideoBackend::Shutdown() {}

bool VulkanVideoBackend::SupportsHardwareContext(retro_hw_context_type context_type) const {
  return context_type == RETRO_HW_CONTEXT_VULKAN;
}

bool VulkanVideoBackend::PrepareHardwareContext(retro_hw_render_callback&,
                                                const VideoGeometry&,
                                                std::string& error) {
  error = "Vulkan backend is not implemented yet.";
  return false;
}

bool VulkanVideoBackend::ActivateHardwareContext(std::string& error) {
  error = "Vulkan backend is not implemented yet.";
  return false;
}

}  // namespace sekaiemu::spike
