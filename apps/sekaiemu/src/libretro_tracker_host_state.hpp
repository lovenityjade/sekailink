#pragma once

#include "libretro_host.hpp"
#include "sklmi_companion_runtime.hpp"
#include "tracker_asset_resolver.hpp"
#include "tracker_runtime.hpp"
#include "tracker_snapshot_io.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace sekaiemu::spike {

std::filesystem::path ResolveTrackerStatePathForHost(const HostOptions& options);
std::filesystem::path ResolveTrackerSnapshotPathForHost(const HostOptions& options,
                                                        const SklmiCompanionRuntime& runtime);
std::filesystem::path ResolveTrackerCommandLogPathForHost(const HostOptions& options,
                                                          const SklmiCompanionRuntime& runtime);
nlohmann::json BuildTrackerErrorSnapshot(std::string code, std::string message);
nlohmann::json BuildTrackerLoadingSnapshot(std::string message);
void RefreshTrackerAssetResolverForHost(const HostOptions& options,
                                        const std::optional<TrackerBundle>& tracker_bundle,
                                        const TrackerRuntime* tracker_runtime,
                                        HostTrackerAssetResolver& asset_resolver);
bool InitializeTrackerRuntimeForHost(const HostOptions& options,
                                     const SklmiCompanionRuntime& sklmi_companion_runtime,
                                     std::optional<TrackerBundle>& tracker_bundle,
                                     TrackerRuntime& tracker_runtime,
                                     HostTrackerAssetResolver& asset_resolver,
                                     std::filesystem::path& tracker_state_path,
                                     std::filesystem::path& tracker_snapshot_path,
                                     std::filesystem::path& tracker_command_log_path,
                                     std::uint64_t& last_mutation_serial,
                                     bool& tracker_dirty,
                                     bool& tracker_active,
                                     std::string& last_error);
void SaveTrackerStateForHost(TrackerRuntime& tracker_runtime,
                             bool tracker_active,
                             const std::filesystem::path& tracker_state_path,
                             std::uint64_t frame_counter,
                             std::uint64_t& last_save_frame,
                             bool& tracker_dirty,
                             const char* reason);
void TickTrackerRuntimeForHost(TrackerRuntime& tracker_runtime,
                               bool tracker_active,
                               const std::filesystem::path& tracker_snapshot_path,
                               TrackerSnapshotReader& tracker_snapshot_reader,
                               const SklmiCompanionRuntime& sklmi_companion_runtime,
                               std::optional<std::filesystem::file_time_type>& room_state_last_write_time,
                               std::uintmax_t& trace_offset,
                               std::unordered_map<std::string, std::string>& item_label_by_key,
                               std::uint64_t frame_counter,
                               std::uint64_t& last_poll_frame,
                               std::uint64_t& last_mutation_serial,
                               std::uint64_t& last_save_frame,
                               bool& tracker_dirty,
                               const std::filesystem::path& tracker_state_path,
                               std::uint64_t poll_frame_interval,
                               std::uint64_t snapshot_poll_frame_interval,
                               std::uint64_t autosave_frame_interval);

}  // namespace sekaiemu::spike
