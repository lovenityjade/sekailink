#pragma once

#include "bridge_runtime_status.hpp"
#include "libretro_host.hpp"
#include "memory_profile.hpp"
#include "runtime_memory_server.hpp"
#include "sklmi_bridge_registry.hpp"
#include "sklmi_companion_runtime.hpp"

#include <filesystem>
#include <optional>
#include <string>

namespace sekaiemu::spike {

class ProfileBridge;
class TrackerSnapshotReader;
struct CoreApi;
class MemoryDomainRegistry;

bool InitializeProfileBridgeForHost(const HostOptions& options,
                                    BridgeOwner bridge_owner,
                                    ProfileBridge& profile_bridge,
                                    std::string& last_error);

bool InitializeRuntimeMemoryServerForHost(const HostOptions& options,
                                          CoreApi& core,
                                          MemoryDomainRegistry& memory_domains,
                                          RuntimeMemoryServer& runtime_memory_server,
                                          std::string& last_error);

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
                                     TrackerSnapshotReader& tracker_snapshot_reader);

void TickSklmiCompanionForHost(SklmiCompanionRuntime& sklmi_companion_runtime,
                               std::string& bridge_runtime_last_error);

BridgeRuntimeStatus BuildBridgeRuntimeStatusForHost(
    const HostOptions& options,
    const std::optional<MemoryProfile>& launch_profile,
    BridgeOwner bridge_owner,
    const std::optional<SklmiBridgeSpec>& active_sklmi_bridge_spec,
    const ProfileBridge& profile_bridge,
    const RuntimeMemoryServer& runtime_memory_server,
    const SklmiCompanionRuntime& sklmi_companion_runtime,
    const std::string& bridge_runtime_last_error);

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
                                  TrackerSnapshotReader& tracker_snapshot_reader);

}  // namespace sekaiemu::spike
