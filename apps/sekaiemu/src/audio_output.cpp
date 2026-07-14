#include "audio_output.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <iostream>
#include <limits>

namespace sekaiemu::spike {
namespace {

constexpr Uint32 kStartPrebufferMs = 120;
constexpr Uint32 kBufferMs = 240;
constexpr Uint32 kLowWaterMs = 40;
constexpr Uint32 kOptionalWorkWaterMs = 90;
constexpr Uint32 kHighWaterMs = 180;
constexpr int kPreferredOutputSampleRate = 48000;
constexpr double kMaxTimingSkew = 0.05;
constexpr double kRateControlDelta = 0.005;

Uint32 ResolveSampleRate(const SDL_AudioSpec& obtained, std::size_t fallback_sample_rate) {
  if (obtained.freq > 0) {
    return static_cast<Uint32>(obtained.freq);
  }
  return fallback_sample_rate > 0 ? static_cast<Uint32>(fallback_sample_rate) : 48000u;
}

std::size_t SamplesForMilliseconds(Uint32 sample_rate, Uint32 milliseconds) {
  return static_cast<std::size_t>(sample_rate) * 2u * milliseconds / 1000u;
}

unsigned SamplesToMilliseconds(std::size_t samples, Uint32 sample_rate) {
  const auto samples_per_second = static_cast<std::uint64_t>(sample_rate) * 2u;
  if (samples_per_second == 0) {
    return 0;
  }
  return static_cast<unsigned>((static_cast<std::uint64_t>(samples) * 1000u) / samples_per_second);
}

double AdjustInputRateForVideoPacing(double core_sample_rate, double core_fps) {
  if (!std::isfinite(core_sample_rate) || core_sample_rate <= 0.0 ||
      !std::isfinite(core_fps) || core_fps <= 1.0) {
    return core_sample_rate;
  }
  const double target_fps = std::round(core_fps);
  if (target_fps <= 1.0) {
    return core_sample_rate;
  }
  const double skew = std::abs(1.0 - (core_fps / target_fps));
  if (skew > kMaxTimingSkew) {
    return core_sample_rate;
  }
  return core_sample_rate * target_fps / core_fps;
}

int16_t InterpolateSample(int16_t a, int16_t b, double fraction) {
  const double value = static_cast<double>(a) + (static_cast<double>(b) - static_cast<double>(a)) * fraction;
  return static_cast<int16_t>(std::clamp(std::lround(value),
                                         static_cast<long>(std::numeric_limits<int16_t>::min()),
                                         static_cast<long>(std::numeric_limits<int16_t>::max())));
}

int16_t ApplyVolume(int16_t sample, int percent) {
  const long scaled = (static_cast<long>(sample) * std::clamp(percent, 0, 150)) / 100L;
  return static_cast<int16_t>(std::clamp(scaled,
                                         static_cast<long>(std::numeric_limits<int16_t>::min()),
                                         static_cast<long>(std::numeric_limits<int16_t>::max())));
}

}  // namespace

AudioOutput::~AudioOutput() { Shutdown(); }

bool AudioOutput::Initialize(double core_sample_rate, double core_fps, std::string& error) {
  Shutdown();

  core_sample_rate_ = 48000.0;
  if (std::isfinite(core_sample_rate) && core_sample_rate >= 8000.0 && core_sample_rate <= 384000.0) {
    core_sample_rate_ = core_sample_rate;
  }
  adjusted_input_rate_ = AdjustInputRateForVideoPacing(core_sample_rate_, core_fps);
  rate_adjust_ = 1.0;
  resample_position_ = 0.0;
  resample_tail_[0] = 0;
  resample_tail_[1] = 0;
  last_output_frame_[0] = 0;
  last_output_frame_[1] = 0;
  resample_tail_valid_ = false;
  last_output_frame_valid_ = false;
  resample_buffer_.clear();
  ring_buffer_.clear();
  ring_read_index_ = 0;
  ring_write_index_ = 0;
  ring_size_samples_ = 0;

  SDL_AudioSpec desired{};
  desired.freq = kPreferredOutputSampleRate;
  desired.format = AUDIO_S16SYS;
  desired.channels = 2;
  desired.samples = 512;
  desired.callback = &AudioOutput::StaticAudioCallback;
  desired.userdata = this;

  device_ = SDL_OpenAudioDevice(nullptr, 0, &desired, &obtained_, SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
  if (!device_) {
    error = std::string("SDL_OpenAudioDevice failed: ") + SDL_GetError();
    return false;
  }

  audio_started_ = false;
  queued_batches_ = 0;
  high_water_observations_ = 0;
  possible_underruns_ = 0;
  callback_count_ = 0;
  callback_underflows_ = 0;
  underflow_samples_ = 0;
  full_waits_ = 0;
  panic_clears_ = 0;
  max_buffered_samples_ = 0;
  min_buffered_samples_ = 0;
  observed_buffered_samples_ = false;
  ring_buffer_.assign(SamplesForMilliseconds(ResolveSampleRate(obtained_, kPreferredOutputSampleRate), kBufferMs),
                      0);
  SDL_PauseAudioDevice(device_, 1);
  std::cerr << "[sekaiemu-libretro-spike] audio device: requested_output=" << kPreferredOutputSampleRate
            << " obtained=" << obtained_.freq
            << " core_input=" << core_sample_rate_
            << " adjusted_input=" << adjusted_input_rate_
            << " samples=" << obtained_.samples
            << " start_prebuffer_ms=" << kStartPrebufferMs << "\n";
  return true;
}

void AudioOutput::Shutdown() {
  if (device_) {
    SDL_PauseAudioDevice(device_, 1);
    LogQueueSummary();
    SDL_CloseAudioDevice(device_);
    device_ = 0;
    std::memset(&obtained_, 0, sizeof(obtained_));
    audio_started_ = false;
    resample_buffer_.clear();
    ring_buffer_.clear();
    ring_read_index_ = 0;
    ring_write_index_ = 0;
    ring_size_samples_ = 0;
    resample_tail_valid_ = false;
    last_output_frame_valid_ = false;
    resample_position_ = 0.0;
  }
}

std::size_t AudioOutput::QueueSamples(const int16_t* data,
                                      std::size_t frames,
                                      std::size_t fallback_sample_rate) {
  if (!data) {
    return 0;
  }
  if (!device_) {
    return frames;
  }

  const Uint32 sample_rate = ResolveSampleRate(obtained_, fallback_sample_rate);
  UpdateRateControl(sample_rate);
  ResampleToOutputRate(data, frames, sample_rate);
  if (resample_buffer_.empty()) {
    return frames;
  }

  ++queued_batches_;
  WriteOutputSamples(resample_buffer_.data(), resample_buffer_.size());
  return frames;
}

AudioBufferStatus AudioOutput::CurrentBufferStatus(std::size_t fallback_sample_rate) const {
  if (!device_) {
    return {};
  }
  const Uint32 sample_rate = ResolveSampleRate(obtained_, fallback_sample_rate);
  SDL_LockAudioDevice(device_);
  const auto buffered_samples = ring_size_samples_;
  SDL_UnlockAudioDevice(device_);
  const auto window_samples = std::max<std::size_t>(1, SamplesForMilliseconds(sample_rate, kBufferMs));
  AudioBufferStatus status;
  status.active = audio_started_;
  status.occupancy_percent =
      std::min<unsigned>(100u, static_cast<unsigned>((static_cast<std::uint64_t>(buffered_samples) * 100u) / window_samples));
  status.underrun_likely =
      audio_started_ && queued_batches_ > 8 &&
      buffered_samples < SamplesForMilliseconds(sample_rate, kLowWaterMs);
  return status;
}

bool AudioOutput::ShouldDeferOptionalWork(std::size_t fallback_sample_rate) const {
  if (!device_ || !audio_started_ || queued_batches_ <= 8) {
    return false;
  }
  const Uint32 sample_rate = ResolveSampleRate(obtained_, fallback_sample_rate);
  SDL_LockAudioDevice(device_);
  const auto buffered_samples = ring_size_samples_;
  SDL_UnlockAudioDevice(device_);
  return buffered_samples < SamplesForMilliseconds(sample_rate, kOptionalWorkWaterMs);
}

unsigned AudioOutput::EffectiveSampleRate(unsigned fallback_sample_rate) const {
  return obtained_.freq ? static_cast<unsigned>(obtained_.freq) : fallback_sample_rate;
}

void AudioOutput::SetMasterVolumePercent(int percent) {
  master_volume_percent_ = std::clamp(percent, 0, 150);
}

void AudioOutput::SetPlaybackPaused(bool paused) {
  playback_paused_ = paused;
  if (device_ && audio_started_) {
    SDL_PauseAudioDevice(device_, paused ? 1 : 0);
  }
}

void AudioOutput::LogQueueSummary() const {
  if (!device_ || queued_batches_ == 0) {
    return;
  }
  const Uint32 sample_rate = ResolveSampleRate(obtained_, 48000);
  const auto clipped_underflow_samples = static_cast<std::size_t>(
      std::min<std::uint64_t>(underflow_samples_,
                              static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())));
  SDL_LockAudioDevice(device_);
  const auto final_buffered_samples = ring_size_samples_;
  SDL_UnlockAudioDevice(device_);
  std::cerr << "[sekaiemu-libretro-spike] audio queue summary: batches=" << queued_batches_
            << " callbacks=" << callback_count_
            << " max_ms=" << SamplesToMilliseconds(max_buffered_samples_, sample_rate)
            << " min_prequeue_ms=" << SamplesToMilliseconds(min_buffered_samples_, sample_rate)
            << " final_ms=" << SamplesToMilliseconds(final_buffered_samples, sample_rate)
            << " possible_underruns=" << possible_underruns_
            << " callback_underflows=" << callback_underflows_
            << " underflow_ms=" << SamplesToMilliseconds(clipped_underflow_samples, sample_rate)
            << " full_waits=" << full_waits_
            << " rate_adjust=" << rate_adjust_
            << " high_water_hits=" << high_water_observations_
            << " panic_clears=" << panic_clears_ << "\n";
}

const int16_t* AudioOutput::ResolveInputFrame(const int16_t* data,
                                              std::size_t frames,
                                              std::size_t index,
                                              int16_t (&tail_frame)[2]) const {
  if (resample_tail_valid_) {
    if (index == 0) {
      tail_frame[0] = resample_tail_[0];
      tail_frame[1] = resample_tail_[1];
      return tail_frame;
    }
    return data + ((index - 1u) * 2u);
  }
  (void)frames;
  return data + (index * 2u);
}

void AudioOutput::StaticAudioCallback(void* userdata, Uint8* stream, int len) {
  if (auto* audio = static_cast<AudioOutput*>(userdata); audio != nullptr) {
    audio->AudioCallback(stream, len);
    return;
  }
  if (stream != nullptr && len > 0) {
    std::memset(stream, 0, static_cast<std::size_t>(len));
  }
}

void AudioOutput::AudioCallback(Uint8* stream, int len) {
  if (stream == nullptr || len <= 0) {
    return;
  }
  auto* out = reinterpret_cast<int16_t*>(stream);
  const auto requested_samples = static_cast<std::size_t>(len) / sizeof(int16_t);
  std::size_t copied = 0;

  while (copied < requested_samples && ring_size_samples_ > 0 && !ring_buffer_.empty()) {
    out[copied] = ring_buffer_[ring_read_index_];
    last_output_frame_[copied % 2u] = out[copied];
    last_output_frame_valid_ = true;
    ring_read_index_ = (ring_read_index_ + 1u) % ring_buffer_.size();
    --ring_size_samples_;
    ++copied;
  }

  ++callback_count_;
  if (!observed_buffered_samples_ || ring_size_samples_ < min_buffered_samples_) {
    min_buffered_samples_ = ring_size_samples_;
    observed_buffered_samples_ = true;
  }

  if (copied < requested_samples) {
    const auto missing = requested_samples - copied;
    ++callback_underflows_;
    ++possible_underruns_;
    underflow_samples_ += missing;
    const int16_t left = last_output_frame_valid_ ? last_output_frame_[0] : 0;
    const int16_t right = last_output_frame_valid_ ? last_output_frame_[1] : 0;
    while (copied < requested_samples) {
      out[copied] = (copied % 2u == 0) ? left : right;
      ++copied;
    }
  }
}

void AudioOutput::UpdateRateControl(Uint32 output_sample_rate) {
  if (!device_ || ring_buffer_.empty() || output_sample_rate == 0) {
    rate_adjust_ = 1.0;
    return;
  }

  SDL_LockAudioDevice(device_);
  const auto free_samples = ring_buffer_.size() - ring_size_samples_;
  const auto capacity_samples = ring_buffer_.size();
  SDL_UnlockAudioDevice(device_);

  if (capacity_samples == 0) {
    rate_adjust_ = 1.0;
    return;
  }

  const double half_size = static_cast<double>(capacity_samples) / 2.0;
  const double direction = (static_cast<double>(free_samples) - half_size) / half_size;
  const double src_ratio_orig = static_cast<double>(output_sample_rate) / adjusted_input_rate_;
  const double effective_delta =
      src_ratio_orig > 1.0 ? kRateControlDelta / src_ratio_orig : kRateControlDelta;
  rate_adjust_ = std::clamp(1.0 + effective_delta * direction, 0.99, 1.01);
}

void AudioOutput::WriteOutputSamples(const int16_t* samples, std::size_t sample_count) {
  if (!device_ || samples == nullptr || sample_count == 0 || ring_buffer_.empty()) {
    return;
  }

  std::size_t written = 0;
  while (written < sample_count) {
    const auto output_sample_rate = ResolveSampleRate(obtained_, kPreferredOutputSampleRate);
    const auto high_water_samples = SamplesForMilliseconds(output_sample_rate, kHighWaterMs);
    SDL_LockAudioDevice(device_);
    const auto free_samples = ring_buffer_.size() - ring_size_samples_;
    const auto write_count = std::min(sample_count - written, free_samples);
    for (std::size_t index = 0; index < write_count; ++index) {
      ring_buffer_[ring_write_index_] = ApplyVolume(samples[written + index], master_volume_percent_);
      ring_write_index_ = (ring_write_index_ + 1u) % ring_buffer_.size();
    }
    ring_size_samples_ += write_count;
    max_buffered_samples_ = std::max(max_buffered_samples_, ring_size_samples_);
    if (ring_size_samples_ >= high_water_samples) {
      ++high_water_observations_;
    }
    const auto should_start =
        !audio_started_ &&
        ring_size_samples_ >= SamplesForMilliseconds(output_sample_rate, kStartPrebufferMs);
    SDL_UnlockAudioDevice(device_);

    written += write_count;
    if (should_start) {
      audio_started_ = true;
      SDL_PauseAudioDevice(device_, playback_paused_ ? 1 : 0);
    }
    if (write_count == 0) {
      ++full_waits_;
      SDL_Delay(1);
    }
  }
}

void AudioOutput::ResampleToOutputRate(const int16_t* data,
                                       std::size_t frames,
                                       Uint32 output_sample_rate) {
  resample_buffer_.clear();
  if (!data || frames == 0 || output_sample_rate == 0 || adjusted_input_rate_ <= 0.0) {
    return;
  }

  const std::size_t total_input_frames = frames + (resample_tail_valid_ ? 1u : 0u);
  if (total_input_frames < 2) {
    resample_tail_[0] = data[(frames - 1u) * 2u];
    resample_tail_[1] = data[(frames - 1u) * 2u + 1u];
    resample_tail_valid_ = true;
    return;
  }

  const double step =
      adjusted_input_rate_ / (static_cast<double>(output_sample_rate) * rate_adjust_);
  const std::size_t reserve_frames =
      static_cast<std::size_t>((static_cast<double>(frames) / std::max(step, 0.0001)) + 4.0);
  resample_buffer_.reserve(reserve_frames * 2u);

  int16_t tail_frame_a[2] = {0, 0};
  int16_t tail_frame_b[2] = {0, 0};
  while (resample_position_ + 1.0 < static_cast<double>(total_input_frames)) {
    const auto index = static_cast<std::size_t>(resample_position_);
    const double fraction = resample_position_ - static_cast<double>(index);
    const int16_t* a = ResolveInputFrame(data, frames, index, tail_frame_a);
    const int16_t* b = ResolveInputFrame(data, frames, index + 1u, tail_frame_b);
    resample_buffer_.push_back(InterpolateSample(a[0], b[0], fraction));
    resample_buffer_.push_back(InterpolateSample(a[1], b[1], fraction));
    resample_position_ += step;
  }
  if (resample_buffer_.size() >= 2) {
    last_output_frame_[0] = resample_buffer_[resample_buffer_.size() - 2u];
    last_output_frame_[1] = resample_buffer_[resample_buffer_.size() - 1u];
    last_output_frame_valid_ = true;
  }

  resample_position_ -= static_cast<double>(total_input_frames - 1u);
  if (resample_position_ < 0.0) {
    resample_position_ = 0.0;
  }
  resample_tail_[0] = data[(frames - 1u) * 2u];
  resample_tail_[1] = data[(frames - 1u) * 2u + 1u];
  resample_tail_valid_ = true;
}

}  // namespace sekaiemu::spike
