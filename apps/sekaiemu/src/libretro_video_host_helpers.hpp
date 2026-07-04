#pragma once

#include "video_backend.hpp"

#include <libretro.h>

#include <memory>
#include <string>

namespace sekaiemu::spike {

bool InitializeVideoBackendForHost(bool hw_render_requested,
                                   retro_hw_render_callback& hw_render_callback,
                                   bool& hw_render_context_ready,
                                   std::unique_ptr<VideoBackend>& video_backend,
                                   const VideoGeometry& geometry,
                                   std::string& error);

bool EnsureHardwareVideoBackendForHost(bool hw_render_requested,
                                       retro_hw_render_callback& hw_render_callback,
                                       bool& hw_render_context_ready,
                                       std::unique_ptr<VideoBackend>& video_backend,
                                       const VideoGeometry& geometry,
                                       std::string& error);

bool PrepareHardwareVideoBackendForHost(bool hw_render_requested,
                                        retro_hw_render_callback& hw_render_callback,
                                        bool& hw_render_context_ready,
                                        std::unique_ptr<VideoBackend>& video_backend,
                                        const VideoGeometry& geometry,
                                        std::string& error);

void MaybeUpdateVideoBackendGeometry(VideoBackend* video_backend,
                                     const VideoGeometry& geometry);

}  // namespace sekaiemu::spike
