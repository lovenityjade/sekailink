#include "libretro_video_host_helpers.hpp"

#include "opengl_video_backend.hpp"
#include "software_video_backend.hpp"
#include "vulkan_video_backend.hpp"

#include <iostream>

namespace sekaiemu::spike {

bool InitializeVideoBackendForHost(bool hw_render_requested,
                                   retro_hw_render_callback& hw_render_callback,
                                   bool& hw_render_context_ready,
                                   std::unique_ptr<VideoBackend>& video_backend,
                                   const VideoGeometry& geometry,
                                   std::string& error) {
  if (hw_render_requested) {
    return EnsureHardwareVideoBackendForHost(hw_render_requested,
                                             hw_render_callback,
                                             hw_render_context_ready,
                                             video_backend,
                                             geometry,
                                             error);
  }

  if (!video_backend) {
    video_backend = std::make_unique<OpenGlVideoBackend>();
  }
  return video_backend->Initialize(geometry, error);
}

bool EnsureHardwareVideoBackendForHost(bool hw_render_requested,
                                       retro_hw_render_callback& hw_render_callback,
                                       bool& hw_render_context_ready,
                                       std::unique_ptr<VideoBackend>& video_backend,
                                       const VideoGeometry& geometry,
                                       std::string& error) {
  if (!PrepareHardwareVideoBackendForHost(hw_render_requested,
                                          hw_render_callback,
                                          hw_render_context_ready,
                                          video_backend,
                                          geometry,
                                          error)) {
    return false;
  }

  if (!hw_render_context_ready && !video_backend->ActivateHardwareContext(error)) {
    return false;
  }
  hw_render_context_ready = true;

  return video_backend->UpdateGeometry(geometry, error);
}

bool PrepareHardwareVideoBackendForHost(bool hw_render_requested,
                                        retro_hw_render_callback& hw_render_callback,
                                        bool& hw_render_context_ready,
                                        std::unique_ptr<VideoBackend>& video_backend,
                                        const VideoGeometry& geometry,
                                        std::string& error) {
  if (!hw_render_requested) {
    error = "No hardware render request is active.";
    return false;
  }

  if (hw_render_callback.context_type == RETRO_HW_CONTEXT_VULKAN) {
    error = "Vulkan hardware rendering is not implemented yet.";
    return false;
  }

  if (!video_backend ||
      (hw_render_callback.context_type == RETRO_HW_CONTEXT_VULKAN &&
       video_backend->Type() != VideoBackendType::Vulkan) ||
      (hw_render_callback.context_type != RETRO_HW_CONTEXT_VULKAN &&
       video_backend->Type() != VideoBackendType::OpenGl)) {
    video_backend.reset();
    if (hw_render_callback.context_type == RETRO_HW_CONTEXT_VULKAN) {
      video_backend = std::make_unique<VulkanVideoBackend>();
    } else {
      video_backend = std::make_unique<OpenGlVideoBackend>();
    }
    hw_render_context_ready = false;
  }

  if (!video_backend->SupportsHardwareContext(hw_render_callback.context_type)) {
    error = "Requested hardware render context is not supported by the OpenGL backend.";
    return false;
  }

  return video_backend->PrepareHardwareContext(hw_render_callback, geometry, error);
}

void MaybeUpdateVideoBackendGeometry(VideoBackend* video_backend,
                                     const VideoGeometry& geometry) {
  if (!video_backend) {
    return;
  }

  std::string error;
  if (!video_backend->UpdateGeometry(geometry, error)) {
    std::cerr << "[sekaiemu-libretro-spike] failed to update video geometry: "
              << error << "\n";
  }
}

}  // namespace sekaiemu::spike
