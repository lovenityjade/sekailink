#include "libretro_host_internal.hpp"

#include "libretro_core_utils.hpp"
#include "libretro_environment.hpp"
#include "libretro_io_callbacks.hpp"

#include <array>
#include <cstdarg>
#include <cstdio>
#include <cstdint>
#include <iostream>

namespace sekaiemu::spike {

bool LibretroHost::Impl::EnvironmentCallback(unsigned cmd, void* data) {
  if (!active_host) {
    return false;
  }
  return active_host->OnEnvironment(cmd, data);
}

void LibretroHost::Impl::VideoRefreshCallback(const void* data,
                                              unsigned width,
                                              unsigned height,
                                              size_t pitch) {
  if (active_host) {
    active_host->OnVideoRefresh(data, width, height, pitch);
  }
}

size_t LibretroHost::Impl::AudioSampleBatchCallback(const int16_t* data, size_t frames) {
  if (!active_host) {
    return 0;
  }
  return active_host->OnAudioSampleBatch(data, frames);
}

void LibretroHost::Impl::AudioSampleCallback(int16_t left, int16_t right) {
  std::array<int16_t, 2> stereo{left, right};
  if (active_host) {
    active_host->OnAudioSampleBatch(stereo.data(), 1);
  }
}

void LibretroHost::Impl::InputPollCallback() {
  if (active_host) {
    active_host->input_state.PollRequested();
  }
}

int16_t LibretroHost::Impl::InputStateCallback(unsigned port,
                                               unsigned device,
                                               unsigned index,
                                               unsigned id) {
  if (!active_host) {
    return 0;
  }
  return active_host->input_state.Read(port, device, index, id);
}

void LibretroHost::Impl::LogPrintf(enum retro_log_level level, const char* fmt, ...) {
  if (!active_host || !fmt) {
    return;
  }

  va_list args;
  va_start(args, fmt);
  std::array<char, 2048> buffer{};
  vsnprintf(buffer.data(), buffer.size(), fmt, args);
  va_end(args);

  std::cerr << "[libretro][" << LogLevelName(level) << "] " << buffer.data();
}

bool LibretroHost::Impl::SetRumbleState(unsigned, enum retro_rumble_effect, std::uint16_t) {
  return true;
}

bool LibretroHost::Impl::OnEnvironment(unsigned cmd, void* data) {
  return HandleLibretroEnvironmentCommand(environment_context, cmd, data);
}

void LibretroHost::Impl::OnVideoRefresh(const void* data,
                                        unsigned width,
                                        unsigned height,
                                        size_t pitch) {
  HandleVideoRefreshForHost(data,
                            width,
                            height,
                            pitch,
                            video_callback_count,
                            video_backend.get(),
                            latest_frame_width,
                            latest_frame_height,
                            latest_frame_pitch,
                            [this](const void* frame_data,
                                   unsigned frame_width,
                                   unsigned frame_height,
                                   std::size_t frame_pitch) {
                              MaybeDumpFrame(frame_data, frame_width, frame_height, frame_pitch);
                            },
                            frame_ready);
}

size_t LibretroHost::Impl::OnAudioSampleBatch(const int16_t* data, size_t frames) {
  return HandleAudioSampleBatchForHost(data, frames, audio_callback_count, audio_output);
}

}  // namespace sekaiemu::spike
