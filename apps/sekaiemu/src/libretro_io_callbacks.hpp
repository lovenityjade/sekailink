#pragma once

#include "audio_output.hpp"
#include "video_backend.hpp"

#include <cstddef>
#include <cstdint>
#include <functional>

namespace sekaiemu::spike {

void HandleVideoRefreshForHost(const void* data,
                               unsigned width,
                               unsigned height,
                               std::size_t pitch,
                               std::uint64_t& video_callback_count,
                               VideoBackend* video_backend,
                               unsigned& latest_frame_width,
                               unsigned& latest_frame_height,
                               std::size_t& latest_frame_pitch,
                               const std::function<void(const void*, unsigned, unsigned, std::size_t)>& maybe_dump_frame,
                               bool& frame_ready);

std::size_t HandleAudioSampleBatchForHost(const int16_t* data,
                                          std::size_t frames,
                                          std::uint64_t& audio_callback_count,
                                          AudioOutput& audio_output);

}  // namespace sekaiemu::spike
