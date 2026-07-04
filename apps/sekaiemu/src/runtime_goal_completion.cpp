#include "runtime_goal_completion.hpp"

#include "tracker_overlay_render_state.hpp"

#include <SDL.h>
#include <imgui.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <utility>
#include <vector>

namespace sekaiemu::spike {
namespace {

constexpr std::uint64_t kPersistIntervalFrames = 300;
constexpr double kGoalMusicDurationSeconds = 20.4;

std::filesystem::path CurrentPath() {
  std::error_code ignored;
  return std::filesystem::current_path(ignored);
}

std::filesystem::path SdlBasePath() {
  char* raw = SDL_GetBasePath();
  if (raw == nullptr) {
    return {};
  }
  std::filesystem::path out(raw);
  SDL_free(raw);
  return out;
}

std::filesystem::path ResolveGoalAssetDirectory() {
  const auto base = SdlBasePath();
  const auto cwd = CurrentPath();
  const std::vector<std::filesystem::path> candidates{
      base / "assets" / "goal-completion",
      base / ".." / "assets" / "goal-completion",
      base / ".." / ".." / "assets" / "goal-completion",
      cwd / "assets" / "goal-completion",
      cwd / "apps" / "sekaiemu" / "assets" / "goal-completion",
      cwd / ".." / "assets" / "goal-completion",
  };
  for (const auto& candidate : candidates) {
    std::error_code ignored;
    if (std::filesystem::exists(candidate / "goalcompleted.wav", ignored) &&
        std::filesystem::exists(candidate / "aftergoal.wav", ignored)) {
      return candidate;
    }
  }
  return {};
}

int ClampPercent(int value) {
  return std::clamp(value, 0, 100);
}

std::string JsonString(const nlohmann::json& value, const char* key, std::string fallback = {}) {
  if (!value.is_object()) return fallback;
  const auto found = value.find(key);
  if (found == value.end() || !found->is_string()) return fallback;
  return found->get<std::string>();
}

std::string FirstNonEmpty(std::initializer_list<std::string> values) {
  for (auto value : values) {
    if (!value.empty()) {
      return value;
    }
  }
  return {};
}

}  // namespace

RuntimeGoalCompletion::~RuntimeGoalCompletion() = default;

std::string FormatGoalCompletionDuration(std::uint64_t seconds) {
  const auto hours = seconds / 3600;
  const auto minutes = (seconds / 60) % 60;
  const auto secs = seconds % 60;
  std::ostringstream out;
  if (hours > 0) {
    out << hours << ":";
    out << std::setw(2) << std::setfill('0') << minutes << ":";
  } else {
    out << minutes << ":";
  }
  out << std::setw(2) << std::setfill('0') << secs;
  return out.str();
}

void RuntimeGoalCompletion::Initialize(const std::filesystem::path& save_directory,
                                       std::string game_name,
                                       std::string player_name) {
  game_name_ = std::move(game_name);
  player_name_ = std::move(player_name);
  state_path_ = save_directory / "goal-completion-timer.json";
  LoadState();
}

void RuntimeGoalCompletion::Shutdown() {
  StopMusic();
  SaveState();
}

void RuntimeGoalCompletion::TickEmulationFrame(double seconds_per_frame, std::uint64_t frame_counter) {
  if (completed_ || active_ || !std::isfinite(seconds_per_frame) || seconds_per_frame <= 0.0) {
    return;
  }
  elapsed_seconds_ += seconds_per_frame;
  SaveStateIfDue(frame_counter);
}

void RuntimeGoalCompletion::TickScreen(std::uint64_t frame_counter) {
  if (!active_) {
    return;
  }
  screen_elapsed_seconds_ += 1.0 / 60.0;
  TickMusic();
  SaveStateIfDue(frame_counter);
}

void RuntimeGoalCompletion::Trigger(std::string source, std::uint64_t frame_counter) {
  if (completed_) {
    active_ = true;
    screen_elapsed_seconds_ = 0.0;
    trigger_source_ = std::move(source);
    StartMusic();
    SaveState();
    return;
  }
  completed_ = true;
  active_ = true;
  screen_elapsed_seconds_ = 0.0;
  trigger_source_ = std::move(source);
  last_persist_frame_ = frame_counter;
  StartMusic();
  SaveState();
}

bool RuntimeGoalCompletion::HandleSdlEvent(const SDL_Event& event) {
  if (!active_) {
    return false;
  }
  if (event.type == SDL_KEYDOWN && event.key.repeat == 0 &&
      (event.key.keysym.scancode == SDL_SCANCODE_ESCAPE ||
       event.key.keysym.scancode == SDL_SCANCODE_RETURN ||
       event.key.keysym.scancode == SDL_SCANCODE_KP_ENTER)) {
    active_ = false;
    StopMusic();
    SaveState();
    return true;
  }
  if (event.type == SDL_CONTROLLERBUTTONDOWN &&
      (event.cbutton.button == SDL_CONTROLLER_BUTTON_A ||
       event.cbutton.button == SDL_CONTROLLER_BUTTON_START)) {
    active_ = false;
    StopMusic();
    SaveState();
    return true;
  }
  return event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP ||
         event.type == SDL_MOUSEMOTION || event.type == SDL_MOUSEWHEEL;
}

GoalCompletionStats RuntimeGoalCompletion::BuildStats(const TrackerRuntime& tracker_runtime) const {
  const auto resolved = tracker_runtime.ResolvedViewState();
  const auto& authoritative = tracker_runtime.AuthoritativeState();
  GoalCompletionStats stats;
  stats.game_name = game_name_.empty() ? "SekaiLink" : game_name_;
  stats.player_name = SnapshotDisplayPlayerName(tracker_runtime, player_name_);
  if (stats.player_name.empty()) {
    stats.player_name = player_name_.empty() ? "Player" : player_name_;
  }
  stats.config_name = FirstNonEmpty({
      JsonStringAtAnyKey(authoritative.seed_metadata, {"config_name", "configName", "configuration_name",
                                                       "configurationName", "name", "title", "label"}),
      SnapshotStringAt(tracker_runtime, "room_metadata.config_name"),
      SnapshotStringAt(tracker_runtime, "room_metadata.configName"),
      SnapshotStringAt(tracker_runtime, "seed_name"),
      SnapshotStringAt(tracker_runtime, "seedName"),
      authoritative.seed_id,
      authoritative.world_instance_id,
      resolved.seed_metadata.is_object()
          ? JsonStringAtAnyKey(resolved.seed_metadata, {"config_name", "configName", "name", "title"})
          : std::string{},
      stats.game_name,
  });
  stats.elapsed_seconds = static_cast<std::uint64_t>(std::max(0.0, std::round(elapsed_seconds_)));
  stats.checked_count = CheckedCount(tracker_runtime);
  stats.checks_sent = stats.checked_count;
  stats.checks_received = ReceivedCount(tracker_runtime);
  const auto missing = MissingCount(tracker_runtime);
  stats.total_locations = std::max<std::size_t>(stats.checked_count + missing, stats.checked_count);
  stats.completion_percent = stats.total_locations == 0
                                 ? 100
                                 : ClampPercent(static_cast<int>((stats.checked_count * 100) / stats.total_locations));
  return stats;
}

void RuntimeGoalCompletion::RenderImGui(const GoalCompletionStats& stats) {
  if (!active_) {
    return;
  }

  ImGuiIO& io = ImGui::GetIO();
  const ImVec2 viewport = io.DisplaySize;
  ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
  ImGui::SetNextWindowSize(viewport);
  ImGui::SetNextWindowBgAlpha(0.0f);
  ImGui::Begin("SekaiemuGoalCompletionOverlay",
               nullptr,
               ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoNavFocus);

  ImDrawList* draw = ImGui::GetWindowDrawList();
  const float w = viewport.x;
  const float h = viewport.y;
  const float t = static_cast<float>(std::max(0.0, screen_elapsed_seconds_));
  const float flash = t < 0.22f ? 1.0f - (t / 0.22f) : 0.0f;
  draw->AddRectFilled(ImVec2(0, 0), ImVec2(w, h), IM_COL32(2, 7, 12, 198));
  draw->AddRectFilled(ImVec2(0, 0), ImVec2(w, h), IM_COL32(110, 255, 225, static_cast<int>(flash * 190.0f)));

  const float pulse = 0.5f + 0.5f * std::sin(t * 4.4f);
  const ImVec2 center(w * 0.5f, h * 0.30f);
  for (int ring = 0; ring < 4; ++ring) {
    const float radius = (120.0f + ring * 58.0f) * (1.0f + pulse * 0.04f);
    draw->AddCircle(center, radius, IM_COL32(72, 230, 216, 28 - ring * 4), 96, 2.0f);
  }

  ImGui::PushFont(nullptr);
  const char* title = "GOAL COMPLETED";
  const ImVec2 title_size = ImGui::CalcTextSize(title);
  const float title_scale = std::clamp(w / 620.0f, 1.2f, 2.2f);
  ImGui::SetWindowFontScale(title_scale);
  draw->AddText(ImVec2((w - title_size.x * title_scale) * 0.5f + 3.0f, h * 0.15f + 3.0f),
                IM_COL32(0, 0, 0, 210),
                title);
  draw->AddText(ImVec2((w - title_size.x * title_scale) * 0.5f, h * 0.15f),
                IM_COL32(206, 255, 246, 255),
                title);
  ImGui::SetWindowFontScale(1.0f);

  const float card_w = std::min(w * 0.72f, 760.0f);
  const float card_h = 250.0f;
  const float card_x = (w - card_w) * 0.5f;
  const float card_y = h * 0.39f;
  draw->AddRectFilled(ImVec2(card_x, card_y), ImVec2(card_x + card_w, card_y + card_h), IM_COL32(8, 15, 22, 220), 10.0f);
  draw->AddRect(ImVec2(card_x, card_y), ImVec2(card_x + card_w, card_y + card_h), IM_COL32(88, 230, 218, 150), 10.0f, 0, 1.5f);

  auto line = [&](float y, const char* label, const std::string& value, float reveal_at) {
    if (t < reveal_at) return;
    draw->AddText(ImVec2(card_x + 34.0f, y), IM_COL32(148, 172, 184, 255), label);
    const ImVec2 value_size = ImGui::CalcTextSize(value.c_str());
    draw->AddText(ImVec2(card_x + card_w - value_size.x - 34.0f, y), IM_COL32(235, 255, 250, 255), value.c_str());
  };
  line(card_y + 28.0f, "Player", stats.player_name, 9.8f);
  line(card_y + 64.0f, "Config", stats.config_name, 10.45f);
  line(card_y + 100.0f, "Time Taken", FormatGoalCompletionDuration(stats.elapsed_seconds), 11.25f);
  line(card_y + 136.0f, "Checks Sent / Received", std::to_string(stats.checks_sent) + " / " + std::to_string(stats.checks_received), 12.1f);
  line(card_y + 172.0f, "Checks Done / Total", std::to_string(stats.checked_count) + " / " + std::to_string(stats.total_locations), 13.55f);
  line(card_y + 208.0f, "Completion", std::to_string(stats.completion_percent) + "%", 15.3f);

  if (t >= 17.9f) {
    const char* end = "The End";
    const ImVec2 end_size = ImGui::CalcTextSize(end);
    draw->AddText(ImVec2((w - end_size.x) * 0.5f, h * 0.78f), IM_COL32(255, 250, 220, 255), end);
  }
  if (t >= kGoalMusicDurationSeconds) {
    const char* hint = "Press Enter to return";
    const ImVec2 hint_size = ImGui::CalcTextSize(hint);
    draw->AddText(ImVec2((w - hint_size.x) * 0.5f, h * 0.89f), IM_COL32(148, 172, 184, 230), hint);
  }
  ImGui::PopFont();
  ImGui::End();
}

void RuntimeGoalCompletion::LoadState() {
  elapsed_seconds_ = 0.0;
  completed_ = false;
  active_ = false;
  if (state_path_.empty() || !std::filesystem::exists(state_path_)) {
    return;
  }
  std::ifstream input(state_path_);
  if (!input) {
    return;
  }
  nlohmann::json state = nlohmann::json::object();
  try {
    input >> state;
  } catch (...) {
    return;
  }
  elapsed_seconds_ = std::max(0.0, state.value("elapsed_seconds", 0.0));
  completed_ = state.value("completed", false);
  trigger_source_ = JsonString(state, "trigger_source");
}

void RuntimeGoalCompletion::SaveState() const {
  if (state_path_.empty()) {
    return;
  }
  std::error_code ignored;
  std::filesystem::create_directories(state_path_.parent_path(), ignored);
  std::ofstream output(state_path_, std::ios::trunc);
  if (!output) {
    return;
  }
  output << nlohmann::json{
                {"version", 1},
                {"game", game_name_},
                {"player", player_name_},
                {"elapsed_seconds", elapsed_seconds_},
                {"completed", completed_},
                {"trigger_source", trigger_source_},
            }
                .dump(2);
}

void RuntimeGoalCompletion::SaveStateIfDue(std::uint64_t frame_counter) {
  if (last_persist_frame_ != 0 && frame_counter - last_persist_frame_ < kPersistIntervalFrames) {
    return;
  }
  last_persist_frame_ = frame_counter;
  SaveState();
}

void RuntimeGoalCompletion::StartMusic() {
  StopMusic();
  const auto asset_dir = ResolveGoalAssetDirectory();
  if (asset_dir.empty()) {
    std::cerr << "[sekaiemu] goal completion music assets not found\n";
    return;
  }

  auto state = std::make_unique<MusicState>();
  SDL_AudioSpec goal_spec{};
  SDL_AudioSpec after_spec{};
  const auto goal_path = (asset_dir / "goalcompleted.wav").string();
  const auto after_path = (asset_dir / "aftergoal.wav").string();
  if (SDL_LoadWAV(goal_path.c_str(), &goal_spec, &state->goal_buffer, &state->goal_length) == nullptr) {
    std::cerr << "[sekaiemu] goal completion music load failed: " << SDL_GetError() << "\n";
    return;
  }
  if (SDL_LoadWAV(after_path.c_str(), &after_spec, &state->after_buffer, &state->after_length) == nullptr) {
    std::cerr << "[sekaiemu] after-goal music load failed: " << SDL_GetError() << "\n";
    return;
  }
  if (goal_spec.format != after_spec.format ||
      goal_spec.channels != after_spec.channels ||
      goal_spec.freq != after_spec.freq) {
    std::cerr << "[sekaiemu] goal completion music format mismatch\n";
    return;
  }

  state->spec = goal_spec;
  state->device = SDL_OpenAudioDevice(nullptr, 0, &state->spec, nullptr, 0);
  if (state->device == 0) {
    std::cerr << "[sekaiemu] goal completion audio device failed: " << SDL_GetError() << "\n";
    return;
  }
  if (SDL_QueueAudio(state->device, state->goal_buffer, state->goal_length) != 0) {
    std::cerr << "[sekaiemu] goal completion audio queue failed: " << SDL_GetError() << "\n";
    return;
  }
  SDL_PauseAudioDevice(state->device, 0);
  music_ = std::move(state);
}

void RuntimeGoalCompletion::StopMusic() {
  music_.reset();
}

void RuntimeGoalCompletion::TickMusic() {
  if (!music_ || music_->device == 0 || music_->after_queued) {
    return;
  }
  if (SDL_GetQueuedAudioSize(music_->device) != 0) {
    return;
  }
  if (SDL_QueueAudio(music_->device, music_->after_buffer, music_->after_length) == 0) {
    music_->after_queued = true;
  }
}

}  // namespace sekaiemu::spike
