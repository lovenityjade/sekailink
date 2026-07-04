#include "libretro_io_callbacks.hpp"

#include "host_io_utils.hpp"

#include <algorithm>
#include <iostream>

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
                               bool& frame_ready) {
  if (video_callback_count < 5) {
    std::cerr << "[sekaiemu-libretro-spike] video callback #" << (video_callback_count + 1)
              << " data=" << data
              << " width=" << width
              << " height=" << height
              << " pitch=" << pitch
              << "\n";
    if (data) {
      const auto* preview = static_cast<const std::uint8_t*>(data);
      std::cerr << "[sekaiemu-libretro-spike] video preview="
                << HexPreview(preview, std::min<std::size_t>(pitch, 16)) << "\n";
    }
  }
  ++video_callback_count;

  if (!video_backend) {
    return;
  }

  latest_frame_width = width;
  latest_frame_height = height;
  latest_frame_pitch = pitch;
  maybe_dump_frame(data, width, height, pitch);

  if (video_backend->IsHardware()) {
    video_backend->NotifyHardwareFrame(data, width, height, pitch);
    frame_ready = true;
    return;
  }

  if (data && video_backend->UploadSoftwareFrame(data, width, height, pitch)) {
    frame_ready = true;
  }
}

std::size_t HandleAudioSampleBatchForHost(const int16_t* data,
                                          std::size_t frames,
                                          std::uint64_t& audio_callback_count,
                                          AudioOutput& audio_output) {
  if (audio_callback_count < 5) {
    std::cerr << "[sekaiemu-libretro-spike] audio callback #" << (audio_callback_count + 1)
              << " data=" << static_cast<const void*>(data)
              << " frames=" << frames << "\n";
    if (data && frames > 0) {
      std::cerr << "[sekaiemu-libretro-spike] audio preview="
                << data[0] << "," << data[1] << "\n";
    }
  }
  ++audio_callback_count;

  if (!data) {
    return 0;
  }
  return audio_output.QueueSamples(data, frames, 48000);
}

}  // namespace sekaiemu::spike
