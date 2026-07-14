#pragma once

#include "tracker_runtime.hpp"

#include <SDL.h>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace sekaiemu::spike {

struct GoalCompletionStats {
  std::string game_name;
  std::string player_name;
  std::string config_name;
  std::uint64_t elapsed_seconds = 0;
  std::size_t checks_sent = 0;
  std::size_t checks_received = 0;
  std::size_t checked_count = 0;
  std::size_t total_locations = 0;
  int completion_percent = 100;
};

class RuntimeGoalCompletion {
 public:
  ~RuntimeGoalCompletion();

  void Initialize(const std::filesystem::path& save_directory,
                  std::string game_name,
                  std::string player_name);
  void Shutdown();

  void TickEmulationFrame(double seconds_per_frame, std::uint64_t frame_counter);
  void TickScreen(std::uint64_t frame_counter);
  void Trigger(std::string source, std::uint64_t frame_counter);
  bool HandleSdlEvent(const SDL_Event& event);

  [[nodiscard]] bool Active() const { return active_; }
  [[nodiscard]] bool Completed() const { return completed_; }
  [[nodiscard]] GoalCompletionStats BuildStats(const TrackerRuntime& tracker_runtime) const;

  void RenderImGui(const GoalCompletionStats& stats);

 private:
  void LoadState();
  void SaveState() const;
  void SaveStateIfDue(std::uint64_t frame_counter);
  void StartMusic();
  void StopMusic();
  void TickMusic();

  std::filesystem::path state_path_;
  std::string game_name_;
  std::string player_name_;
  std::string trigger_source_;
  double elapsed_seconds_ = 0.0;
  double screen_elapsed_seconds_ = 0.0;
  std::uint64_t last_persist_frame_ = 0;
  bool completed_ = false;
  bool active_ = false;
  struct MusicState {
    SDL_AudioDeviceID device = 0;
    SDL_AudioSpec spec{};
    Uint8* goal_buffer = nullptr;
    Uint32 goal_length = 0;
    Uint8* after_buffer = nullptr;
    Uint32 after_length = 0;
    bool after_queued = false;

    ~MusicState() {
      if (device != 0) {
        SDL_ClearQueuedAudio(device);
        SDL_CloseAudioDevice(device);
      }
      if (goal_buffer != nullptr) {
        SDL_FreeWAV(goal_buffer);
      }
      if (after_buffer != nullptr) {
        SDL_FreeWAV(after_buffer);
      }
    }
  };
  std::unique_ptr<MusicState> music_;
};

std::string FormatGoalCompletionDuration(std::uint64_t seconds);

}  // namespace sekaiemu::spike
