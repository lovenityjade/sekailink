#include "libretro_bridge_host_helpers.hpp"

#include "memory_domain_registry.hpp"
#include "profile_bridge.hpp"
#include "tracker_snapshot_io.hpp"

#include <iomanip>
#include <iostream>

namespace sekaiemu::spike {
namespace {

std::filesystem::path ResolveTrackerSnapshotPathForHost(const HostOptions& options,
                                                        const SklmiCompanionRuntime& runtime) {
  if (!options.tracker_snapshot_path.empty()) {
    return options.tracker_snapshot_path;
  }
  if (runtime.RuntimeStateRoot().empty()) {
    return {};
  }
  return runtime.RuntimeStateRoot() / "tracker.snapshot.json";
}

std::filesystem::path ResolveTrackerCommandLogPathForHost(const HostOptions& options,
                                                          const SklmiCompanionRuntime& runtime) {
  if (!options.tracker_command_log_path.empty()) {
    return options.tracker_command_log_path;
  }
  if (runtime.RuntimeStateRoot().empty()) {
    return {};
  }
  return runtime.RuntimeStateRoot() / "tracker.commands.jsonl";
}

}  // namespace

bool InitializeProfileBridgeForHost(const HostOptions& options,
                                    BridgeOwner bridge_owner,
                                    ProfileBridge& profile_bridge,
                                    std::string& last_error) {
  if (options.profile_path.empty()) {
    return true;
  }
  if (bridge_owner == BridgeOwner::Sklmi) {
    return true;
  }

  std::string error;
  if (!profile_bridge.Initialize(options.profile_path, options.save_directory, error)) {
    last_error = error;
    return false;
  }

  const auto* profile = profile_bridge.Profile();
  if (profile) {
    std::cerr << "[sekaiemu-libretro-spike] profile bridge active: " << profile->game
              << " watch=" << profile->watch_region.memory_domain
              << " 0x" << std::hex << profile->watch_region.start
              << "..0x" << (profile->watch_region.start + profile->watch_region.length - 1)
              << std::dec << " checks=" << profile->checks.size()
              << " receive_slots=" << profile->receive_slots.size()
              << " socket=" << profile_bridge.SocketPath() << "\n";
  }
  return true;
}

bool InitializeRuntimeMemoryServerForHost(const HostOptions& options,
                                          CoreApi& core,
                                          MemoryDomainRegistry& memory_domains,
                                          RuntimeMemoryServer& runtime_memory_server,
                                          std::string& last_error) {
  if (options.probe_only) {
    return true;
  }
  std::string error;
  const auto override_socket =
      options.memory_socket_path.empty() ? std::optional<std::filesystem::path>{}
                                         : std::optional<std::filesystem::path>{options.memory_socket_path};
  if (!runtime_memory_server.Initialize(options.save_directory,
                                        override_socket,
                                        InferRuntimeSystemName(options.core_path),
                                        &core,
                                        &memory_domains,
                                        error)) {
    last_error = error;
    return false;
  }
  std::cerr << "[sekaiemu-libretro-spike] runtime memory socket active: "
            << runtime_memory_server.SocketPath() << "\n";
  return true;
}

bool InitializeSklmiCompanionForHost(const HostOptions& options,
                                     const std::optional<MemoryProfile>& launch_profile,
                                     RuntimeMemoryServer& runtime_memory_server,
                                     SklmiCompanionRuntime& sklmi_companion_runtime,
                                     std::optional<SklmiBridgeSpec>& active_sklmi_bridge_spec,
                                     std::optional<std::filesystem::path>& resolved_sklmi_runtime_binary,
                                     std::optional<std::filesystem::path>& resolved_sklmi_manifest_directory,
                                     BridgeOwner& bridge_owner,
                                     std::string& bridge_runtime_last_error,
                                     std::string& last_error,
                                     bool tracker_active,
                                     std::filesystem::path& tracker_snapshot_path,
                                     std::filesystem::path& tracker_command_log_path,
                                     TrackerSnapshotReader& tracker_snapshot_reader) {
  if (options.probe_only || options.profile_path.empty()) {
    return true;
  }
  if (!launch_profile.has_value()) {
    return true;
  }

  active_sklmi_bridge_spec = ResolveSklmiBridgeSpec(launch_profile->game);
  if (!active_sklmi_bridge_spec.has_value()) {
    return true;
  }

  bridge_runtime_last_error.clear();
  resolved_sklmi_runtime_binary = ResolveSklmiRuntimeBinary(options.sklmi_runtime_path);
  resolved_sklmi_manifest_directory =
      ResolveSklmiManifestDirectory(options.sklmi_manifest_directory, resolved_sklmi_runtime_binary);
  if (!resolved_sklmi_runtime_binary.has_value() || !resolved_sklmi_manifest_directory.has_value()) {
    bridge_runtime_last_error =
        "SKLMI companion not activated for migrated game " + launch_profile->game +
        ": runtime or manifest directory not resolved.";
    std::cerr << "[sekaiemu] " << bridge_runtime_last_error << "\n";
    return true;
  }

  const auto companion_bridge_root =
      options.save_directory / "sklmi" / active_sklmi_bridge_spec->bridge_id / "runtime";
  const auto companion_tracker_snapshot_path =
      options.tracker_snapshot_path.empty()
          ? companion_bridge_root / "tracker.snapshot.json"
          : options.tracker_snapshot_path;
  const auto companion_tracker_command_log_path =
      options.tracker_command_log_path.empty()
          ? companion_bridge_root / "tracker.commands.jsonl"
          : options.tracker_command_log_path;

  std::string error;
  if (!sklmi_companion_runtime.Start(SklmiCompanionOptions{
                                         .runtime_binary_path = *resolved_sklmi_runtime_binary,
                                         .manifest_directory = *resolved_sklmi_manifest_directory,
                                         .save_directory = options.save_directory,
                                         .memory_socket_path = runtime_memory_server.SocketPath(),
                                         .bridge_spec = *active_sklmi_bridge_spec,
                                         .ap_host = options.ap_host,
                                         .ap_port = options.ap_port,
                                         .ap_path = options.ap_path,
                                         .ap_game = options.ap_game,
                                         .ap_slot_name = options.ap_slot_name,
                                         .player_alias = options.player_alias,
                                         .ap_password = options.ap_password,
                                         .ap_uuid = options.ap_uuid,
                                         .ap_tags = options.ap_tags,
                                         .tracker_pack_path = options.tracker_pack_path,
                                         .tracker_variant = options.tracker_variant,
                                         .tracker_snapshot_path = companion_tracker_snapshot_path,
                                         .tracker_command_log_path = companion_tracker_command_log_path,
                                         .tracker_assets_root = options.tracker_assets_root,
                                     },
                                     error)) {
    last_error = error;
    return false;
  }
  bridge_owner = BridgeOwner::Sklmi;
  bridge_runtime_last_error.clear();
  std::cerr << "[sekaiemu] SKLMI companion active:"
            << " game=" << active_sklmi_bridge_spec->game_name
            << " bridge_id=" << sklmi_companion_runtime.BridgeId()
            << " manifest=" << sklmi_companion_runtime.ManifestPath()
            << " room_state=" << sklmi_companion_runtime.RoomStatePath()
            << " runtime_state=" << sklmi_companion_runtime.RuntimeStateRoot()
            << " trace_log=" << sklmi_companion_runtime.TraceLogPath()
            << "\n";
  if (tracker_active) {
    tracker_snapshot_path = ResolveTrackerSnapshotPathForHost(options, sklmi_companion_runtime);
    tracker_command_log_path = ResolveTrackerCommandLogPathForHost(options, sklmi_companion_runtime);
    tracker_snapshot_reader.Reset();
  }
  return true;
}

void TickSklmiCompanionForHost(SklmiCompanionRuntime& sklmi_companion_runtime,
                               std::string& bridge_runtime_last_error) {
  std::string companion_error;
  sklmi_companion_runtime.Tick(companion_error);
  if (!companion_error.empty()) {
    bridge_runtime_last_error = companion_error;
    std::cerr << "[sekaiemu] " << companion_error << "\n";
  }
}

BridgeRuntimeStatus BuildBridgeRuntimeStatusForHost(
    const HostOptions& options,
    const std::optional<MemoryProfile>& launch_profile,
    BridgeOwner bridge_owner,
    const std::optional<SklmiBridgeSpec>& active_sklmi_bridge_spec,
    const ProfileBridge& profile_bridge,
    const RuntimeMemoryServer& runtime_memory_server,
    const SklmiCompanionRuntime& sklmi_companion_runtime,
    const std::string& bridge_runtime_last_error) {
  BridgeRuntimeStatus status;
  status.owner = bridge_owner;
  status.migrated_game = active_sklmi_bridge_spec.has_value();
  status.sklmi_active = sklmi_companion_runtime.Active();
  status.legacy_bridge_active = profile_bridge.Active();
  status.game_name = launch_profile ? launch_profile->game : options.game_path.stem().string();
  status.bridge_id = sklmi_companion_runtime.BridgeId();
  if (status.bridge_id.empty() && active_sklmi_bridge_spec.has_value()) {
    status.bridge_id = active_sklmi_bridge_spec->bridge_id;
  }
  status.ap_host = options.ap_host;
  status.ap_port = options.ap_port;
  status.ap_path = options.ap_path;
  status.ap_slot_name = options.ap_slot_name;
  status.runtime_memory_socket_path = runtime_memory_server.SocketPath().string();
  status.legacy_bridge_socket_path = profile_bridge.Active() ? profile_bridge.SocketPath().string() : "";
  status.manifest_path = sklmi_companion_runtime.ManifestPath().string();
  status.room_state_path = sklmi_companion_runtime.RoomStatePath().string();
  status.runtime_state_root = sklmi_companion_runtime.RuntimeStateRoot().string();
  status.trace_log_path = sklmi_companion_runtime.TraceLogPath().string();
  status.companion_log_path = sklmi_companion_runtime.CompanionLogPath().string();
  status.last_error = bridge_runtime_last_error;
  return status;
}

void RestartSklmiCompanionForHost(const HostOptions& options,
                                  RuntimeMemoryServer& runtime_memory_server,
                                  SklmiCompanionRuntime& sklmi_companion_runtime,
                                  const std::optional<SklmiBridgeSpec>& active_sklmi_bridge_spec,
                                  const std::optional<std::filesystem::path>& resolved_sklmi_runtime_binary,
                                  const std::optional<std::filesystem::path>& resolved_sklmi_manifest_directory,
                                  BridgeOwner& bridge_owner,
                                  std::string& bridge_runtime_last_error,
                                  std::filesystem::path& tracker_snapshot_path,
                                  std::filesystem::path& tracker_command_log_path,
                                  TrackerSnapshotReader& tracker_snapshot_reader) {
  bridge_runtime_last_error.clear();
  if (!active_sklmi_bridge_spec.has_value()) {
    bridge_runtime_last_error = "No SKLMI bridge is configured for this game.";
    std::cerr << "[sekaiemu] " << bridge_runtime_last_error << "\n";
    return;
  }
  if (!resolved_sklmi_runtime_binary.has_value() || !resolved_sklmi_manifest_directory.has_value()) {
    bridge_runtime_last_error =
        "SKLMI runtime restart failed: runtime binary or manifest directory is unresolved.";
    std::cerr << "[sekaiemu] " << bridge_runtime_last_error << "\n";
    return;
  }

  sklmi_companion_runtime.Shutdown();
  std::string error;
  if (!sklmi_companion_runtime.Start(SklmiCompanionOptions{
                                         .runtime_binary_path = *resolved_sklmi_runtime_binary,
                                         .manifest_directory = *resolved_sklmi_manifest_directory,
                                         .save_directory = options.save_directory,
                                         .memory_socket_path = runtime_memory_server.SocketPath(),
                                         .bridge_spec = *active_sklmi_bridge_spec,
                                         .ap_host = options.ap_host,
                                         .ap_port = options.ap_port,
                                         .ap_path = options.ap_path,
                                         .ap_game = options.ap_game,
                                         .ap_slot_name = options.ap_slot_name,
                                         .player_alias = options.player_alias,
                                         .ap_password = options.ap_password,
                                         .ap_uuid = options.ap_uuid,
                                         .ap_tags = options.ap_tags,
                                         .tracker_pack_path = options.tracker_pack_path,
                                         .tracker_variant = options.tracker_variant,
                                         .tracker_snapshot_path = options.tracker_snapshot_path,
                                         .tracker_command_log_path = options.tracker_command_log_path,
                                         .tracker_assets_root = options.tracker_assets_root,
                                     },
                                     error)) {
    bridge_runtime_last_error = error;
    std::cerr << "[sekaiemu] " << bridge_runtime_last_error << "\n";
    return;
  }

  bridge_owner = BridgeOwner::Sklmi;
  tracker_snapshot_path = ResolveTrackerSnapshotPathForHost(options, sklmi_companion_runtime);
  tracker_command_log_path = ResolveTrackerCommandLogPathForHost(options, sklmi_companion_runtime);
  tracker_snapshot_reader.Reset();
  std::cerr << "[sekaiemu] SKLMI companion restarted: bridge_id="
            << sklmi_companion_runtime.BridgeId()
            << " trace_log=" << sklmi_companion_runtime.TraceLogPath()
            << "\n";
}

}  // namespace sekaiemu::spike
