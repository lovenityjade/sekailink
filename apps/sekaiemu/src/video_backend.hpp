#pragma once

#include <SDL.h>
#include <libretro.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>

namespace sekaiemu::spike {

struct VideoGeometry {
  unsigned width = 0;
  unsigned height = 0;
  retro_pixel_format pixel_format = RETRO_PIXEL_FORMAT_XRGB8888;
};

enum class VideoBackendType {
  Software,
  OpenGl,
  Vulkan,
};

enum class WindowMode {
  Window,
  BorderlessWindow,
  Fullscreen,
};

class VideoBackend {
 public:
  virtual ~VideoBackend() = default;

  virtual VideoBackendType Type() const = 0;
  virtual bool IsHardware() const = 0;

  virtual bool Initialize(const VideoGeometry& geometry, std::string& error) = 0;
  virtual bool UpdateGeometry(const VideoGeometry& geometry, std::string& error) = 0;

  virtual bool UploadSoftwareFrame(const void* data,
                                   unsigned width,
                                   unsigned height,
                                   std::size_t pitch) = 0;
  virtual void UploadOverlayFrame(const std::uint8_t* rgba_pixels,
                                  unsigned width,
                                  unsigned height) = 0;
  virtual void ClearOverlay() = 0;
  virtual void UploadChatOverlayFrame(const std::uint8_t* rgba_pixels,
                                      unsigned width,
                                      unsigned height) = 0;
  virtual void ClearChatOverlay() = 0;
  virtual void NotifyHardwareFrame(const void* data,
                                   unsigned width,
                                   unsigned height,
                                   std::size_t pitch) = 0;
  virtual void SetMenuVisible(bool visible, const VideoGeometry& geometry) = 0;
  virtual void SetTrackerSidebarLayout(bool enabled,
                                       unsigned sidebar_width,
                                       const VideoGeometry& geometry) = 0;
  virtual bool SetWindowMode(WindowMode mode, std::string& error) = 0;
  virtual WindowMode CurrentWindowMode() const = 0;
  virtual void SetImGuiDrawCallback(std::function<void()> callback) { (void)callback; }
  virtual void Present() = 0;
  virtual void Shutdown() = 0;

  virtual bool SupportsHardwareContext(retro_hw_context_type context_type) const = 0;
  virtual bool PrepareHardwareContext(retro_hw_render_callback& callback,
                                      const VideoGeometry& geometry,
                                      std::string& error) = 0;
  virtual bool ActivateHardwareContext(std::string& error) = 0;
};

}  // namespace sekaiemu::spike
