#pragma once

#include <SDL.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace sekaiemu::spike {

struct AudioBufferStatus {
  bool active = false;
  unsigned occupancy_percent = 0;
  bool underrun_likely = false;
};

class AudioOutput {
 public:
  AudioOutput() = default;
  ~AudioOutput();

  AudioOutput(const AudioOutput&) = delete;
  AudioOutput& operator=(const AudioOutput&) = delete;

  bool Initialize(double core_sample_rate, double core_fps, std::string& error);
  void Shutdown();

  std::size_t QueueSamples(const int16_t* data,
                           std::size_t frames,
                           std::size_t fallback_sample_rate);
  AudioBufferStatus CurrentBufferStatus(std::size_t fallback_sample_rate) const;
  bool ShouldDeferOptionalWork(std::size_t fallback_sample_rate) const;
  unsigned EffectiveSampleRate(unsigned fallback_sample_rate) const;
  void SetMasterVolumePercent(int percent);
  int MasterVolumePercent() const { return master_volume_percent_; }
  void SetPlaybackPaused(bool paused);

 private:
  static void StaticAudioCallback(void* userdata, Uint8* stream, int len);

  void AudioCallback(Uint8* stream, int len);
  void UpdateRateControl(Uint32 output_sample_rate);
  void LogQueueSummary() const;
  const int16_t* ResolveInputFrame(const int16_t* data,
                                   std::size_t frames,
                                   std::size_t index,
                                   int16_t (&tail_frame)[2]) const;
  void ResampleToOutputRate(const int16_t* data,
                            std::size_t frames,
                            Uint32 output_sample_rate);
  void WriteOutputSamples(const int16_t* samples, std::size_t sample_count);

  SDL_AudioDeviceID device_ = 0;
  SDL_AudioSpec obtained_{};
  double core_sample_rate_ = 48000.0;
  double adjusted_input_rate_ = 48000.0;
  double rate_adjust_ = 1.0;
  double resample_position_ = 0.0;
  int16_t resample_tail_[2] = {0, 0};
  int16_t last_output_frame_[2] = {0, 0};
  bool resample_tail_valid_ = false;
  bool last_output_frame_valid_ = false;
  std::vector<int16_t> resample_buffer_;
  std::vector<int16_t> ring_buffer_;
  std::size_t ring_read_index_ = 0;
  std::size_t ring_write_index_ = 0;
  std::size_t ring_size_samples_ = 0;
  bool audio_started_ = false;
  bool playback_paused_ = false;
  std::uint64_t queued_batches_ = 0;
  std::uint64_t high_water_observations_ = 0;
  std::uint64_t possible_underruns_ = 0;
  std::uint64_t callback_count_ = 0;
  std::uint64_t callback_underflows_ = 0;
  std::uint64_t underflow_samples_ = 0;
  std::uint64_t full_waits_ = 0;
  std::uint64_t panic_clears_ = 0;
  std::size_t max_buffered_samples_ = 0;
  std::size_t min_buffered_samples_ = 0;
  int master_volume_percent_ = 35;
  bool observed_buffered_samples_ = false;
};

}  // namespace sekaiemu::spike
