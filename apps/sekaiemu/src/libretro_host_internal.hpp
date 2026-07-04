#pragma once

#include "audio_output.hpp"
#include "activity_feed_window_presenter.hpp"
#include "bridge_runtime_status.hpp"
#include "core_option_manager.hpp"
#include "input_script.hpp"
#include "input_state.hpp"
#include "libretro_core_api.hpp"
#include "libretro_environment.hpp"
#include "libretro_host.hpp"
#include "memory_domain_registry.hpp"
#include "memory_profile.hpp"
#include "profile_bridge.hpp"
#include "runtime_chat_overlay.hpp"
#include "runtime_chat_bridge.hpp"
#include "runtime_client_core_hud.hpp"
#include "bridge_terminal_presenter.hpp"
#include "runtime_frontend_settings.hpp"
#include "runtime_goal_completion.hpp"
#include "runtime_memory_server.hpp"
#include "runtime_menu.hpp"
#include "save_state_manager.hpp"
#include "sklmi_bridge_registry.hpp"
#include "sklmi_companion_runtime.hpp"
#include "tracker_asset_resolver.hpp"
#include "tracker_pack_layout_interaction.hpp"
#include "tracker_runtime.hpp"
#include "tracker_snapshot_io.hpp"
#include "tracker_window_presenter.hpp"
#include "video_backend.hpp"

#include <libretro.h>
#include <nlohmann/json.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sekaiemu::spike {

struct LibretroHost::Impl {
  explicit Impl(HostOptions host_options);
  ~Impl();

  bool Initialize();
  int Run();
  std::string LastError() const;

  static bool EnvironmentCallback(unsigned cmd, void* data);
  static void VideoRefreshCallback(const void* data, unsigned width, unsigned height, size_t pitch);
  static size_t AudioSampleBatchCallback(const int16_t* data, size_t frames);
  static void AudioSampleCallback(int16_t left, int16_t right);
  static void InputPollCallback();
  static int16_t InputStateCallback(unsigned port, unsigned device, unsigned index, unsigned id);
  static void LogPrintf(enum retro_log_level level, const char* fmt, ...);
  static bool SetRumbleState(unsigned, enum retro_rumble_effect, std::uint16_t);

  bool InitializeSdl();
  bool LoadCore();
  bool InitializeCore();
  bool LoadContent();
  bool InitializeVideoBackend();
  bool LoadLaunchProfileMetadata();
  void LoadFrontendSettings();
  void ApplyFrontendWindowSettings();
  void ApplyFrontendTrackerSettings();
  void SaveFrontendSettingsNow();
  void PersistRuntimeMenuSettingsIfNeeded();
  void AdjustMasterVolume(int delta_percent);
  void ToggleNotifications();
  void ToggleActivityFeed();
  void ToggleBridgeTerminal();
  void RenderBridgeTerminal();
  bool InitializeTrackerRuntime();
  void InitializeChatBridge();
  bool InitializeInputScript();
  void ApplyInputScriptFrame();
  bool ApplyStartupStateActions();
  void ApplyFrameAutomation();
  void MaybeDumpFrame(const void* data, unsigned width, unsigned height, std::size_t pitch);
  bool InitializeProfileBridge();
  bool InitializeSklmiCompanion();
  bool InitializeRuntimeMemoryServer();

  bool OnEnvironment(unsigned cmd, void* data);
  void OnVideoRefresh(const void* data, unsigned width, unsigned height, size_t pitch);
  size_t OnAudioSampleBatch(const int16_t* data, size_t frames);

  void PresentFrame();
  void UpdateMenuOverlay();
  void ProbeMemory();
  void TickProfileBridge();
  void TickRuntimeMemoryServer();
  void TickSklmiCompanion();
  void TickTrackerRuntime();
  void TickCoreChatBridge();
  void TickClientCoreHud();
  void RenderClientCoreHud();
  void TriggerGoalCompletion(std::string source);
  void TickGoalCompletionTimer();
  bool GoalCompletionScreenActive() const;
  bool HandleGoalCompletionEvent(const SDL_Event& event);
  void RenderGoalCompletionScreen();
  BridgeRuntimeStatus CurrentBridgeRuntimeStatus() const;
  void DumpMemorySnapshot();
  std::vector<std::uint8_t> ReadProfileRegion(const WatchRegion& region);
  const std::uint8_t* ResolveMemorySource(std::string_view memory_domain,
                                          std::uint32_t start,
                                          std::uint32_t length) const;
  std::uint8_t* ResolveMutableMemorySource(std::string_view memory_domain,
                                           std::uint32_t start,
                                           std::uint32_t length);
  VideoGeometry CurrentVideoGeometry() const;
  bool EnsureHardwareVideoBackend(const VideoGeometry& geometry, std::string& error);
  bool PrepareHardwareVideoBackend(const VideoGeometry& geometry, std::string& error);
  void MaybeUpdateVideoBackendGeometry();
  void Shutdown();

  void SaveBatteryNow();
  const TrackerOverlayAssetResolver* TrackerAssetResolver() const;
  void SaveTrackerState(const char* reason);
  void CycleTrackerDisplayMode();
  void ToggleTrackerScreen();
  void CycleTrackerTab();
  void ToggleTrackerAutoFollow();
  void ToggleFullscreen();
  void OpenTrackerMapMenu();
  bool OpenTrackerMapMenuAt(int mouse_x, int mouse_y);
  bool ActivateTrackerMapMenu();
  bool ActivateTrackerMapMenuAt(int mouse_x, int mouse_y);
  bool HoverTrackerMapMenuAt(int mouse_x, int mouse_y);
  bool ClickTrackerAt(int mouse_x, int mouse_y, std::string_view button);
  bool HoverTrackerAt(int mouse_x, int mouse_y);
  void StepTrackerMapMenu(int delta);
  void CloseTrackerMapMenu();
  void EmitTrackerCommand(nlohmann::json command);
  void RenderTrackerPresentation();
  void ToggleChatOverlay();
  void OpenChatInput();
  void CancelChatInput();
  void SubmitChatInput();
  void BackspaceChatInput();
  void AutocompleteChatInput();
  void AppendChatInput(std::string_view text);
  void RefreshChatOverlayFromSnapshot();
  void LoadBatteryNow();
  void SaveStateNow(int slot = 0);
  void LoadStateNow(int slot = 0);
  void TickBatteryPersistence();
  void SaveBatteryOnShutdown();
  void ApplyMenuAction(RuntimeMenuAction action);
  void RestartBridgeRuntime();

  static constexpr std::uint64_t kTrackerAutosaveFrameInterval = 30 * 60;
  static constexpr std::uint64_t kTrackerPollFrameInterval = 300;
  static constexpr std::uint64_t kTrackerSnapshotPollFrameInterval = 300;
  static constexpr std::uint64_t kTrackerRenderFrameInterval = 30;
  static constexpr std::uint64_t kChatRenderFrameInterval = 30;

  HostOptions options;
  std::string last_error;
  std::optional<MemoryProfile> launch_profile;
  std::optional<TrackerBundle> tracker_bundle_;
  TrackerRuntime tracker_runtime_;
  HostTrackerAssetResolver tracker_asset_resolver_;
  TrackerWindowPresenter tracker_window_presenter_;
  ActivityFeedWindowPresenter activity_feed_window_presenter_;
  BridgeTerminalPresenter bridge_terminal_presenter_;
  std::filesystem::path tracker_state_path_;
  std::filesystem::path tracker_snapshot_path_;
  std::filesystem::path tracker_command_log_path_;
  TrackerSnapshotReader tracker_snapshot_reader_;
  std::optional<std::filesystem::file_time_type> tracker_room_state_last_write_time_;
  std::uintmax_t tracker_trace_offset_ = 0;
  std::uint64_t tracker_last_mutation_serial_ = 0;
  std::uint64_t tracker_last_save_frame_ = 0;
  std::uint64_t tracker_last_poll_frame_ = 0;
  std::uint64_t tracker_last_render_frame_ = 0;
  std::uint64_t tracker_last_render_mutation_serial_ = 0;
  std::uint64_t tracker_hit_cache_mutation_serial_ = 0;
  unsigned tracker_hit_cache_overlay_width_ = 0;
  unsigned tracker_hit_cache_overlay_height_ = 0;
  unsigned tracker_hit_cache_window_width_ = 0;
  unsigned tracker_hit_cache_window_height_ = 0;
  TrackerPanelLayout tracker_hit_cache_layout_;
  bool tracker_hit_cache_compact_ = false;
  std::vector<TrackerPackHitTarget> tracker_hit_targets_;
  std::unordered_map<std::string, std::string> tracker_item_label_by_key_;
  bool tracker_active_ = false;
  bool tracker_dirty_ = false;
  bool tracker_force_next_render_ = false;
  RuntimeChatOverlay chat_overlay_;
  RuntimeChatBridge core_chat_bridge_;
  RuntimeClientCoreHud client_core_hud_;
  RuntimeGoalCompletion goal_completion_;
  RuntimeFrontendSettingsStore frontend_settings_;
  std::uint64_t chat_snapshot_mutation_serial_ = 0;
  std::uint64_t bridge_terminal_last_mutation_serial_ = 0;
  std::uint64_t bridge_terminal_last_render_frame_ = 0;
  bool notifications_enabled_ = true;
  bool activity_feed_enabled_ = false;
  bool chat_ignore_open_key_text_ = false;
  void* core_handle = nullptr;
  CoreApi core{};
  retro_system_info system_info{};
  retro_system_av_info av_info{};
  retro_log_callback log_callback_{};
  LibretroEnvironmentContext environment_context{};
  std::vector<std::uint8_t> game_data;
  MemoryDomainRegistry memory_domains;
  ProfileBridge profile_bridge;
  RuntimeMemoryServer runtime_memory_server;
  SklmiCompanionRuntime sklmi_companion_runtime;
  std::string system_directory_string;
  std::string save_directory_string;
  CoreOptionManager core_option_manager;
  RuntimeMenu runtime_menu;
  SaveStateManager save_state_manager;

  std::unique_ptr<VideoBackend> video_backend;
  AudioOutput audio_output;
  retro_hw_render_callback hw_render_callback{};
  bool hw_render_requested = false;
  bool hw_render_context_ready = false;

  retro_pixel_format pixel_format = RETRO_PIXEL_FORMAT_XRGB8888;
  unsigned latest_frame_width = 0;
  unsigned latest_frame_height = 0;
  size_t latest_frame_pitch = 0;
  bool frame_ready = false;
  bool initialized = false;
  bool running = true;
  int run_exit_code = 0;
  bool game_loaded_for_shutdown = false;
  bool scheduled_state_saved_ = false;
  bool frame_dump_written_ = false;
  std::optional<int> pending_state_screenshot_slot_;
  nlohmann::json pending_state_metadata_ = nlohmann::json::object();
  std::size_t dump_counter = 0;
  std::uint64_t frame_counter = 0;
  retro_audio_buffer_status_callback_t audio_buffer_status_callback = nullptr;
  std::uint64_t video_callback_count = 0;
  std::uint64_t audio_callback_count = 0;

  InputState input_state{};
  InputScriptPlayer input_script{};
  BridgeOwner bridge_owner = BridgeOwner::Legacy;
  std::optional<SklmiBridgeSpec> active_sklmi_bridge_spec;
  std::optional<std::filesystem::path> resolved_sklmi_runtime_binary;
  std::optional<std::filesystem::path> resolved_sklmi_manifest_directory;
  std::string bridge_runtime_last_error;

  static inline Impl* active_host = nullptr;
};

}  // namespace sekaiemu::spike
