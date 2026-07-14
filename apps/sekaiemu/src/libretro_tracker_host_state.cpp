#include "libretro_tracker_host_state.hpp"

#include "libretro_tracker_metadata_pump.hpp"

#include <filesystem>
#include <iostream>
#include <vector>

namespace sekaiemu::spike {

namespace {

std::string SnapshotStringAt(const nlohmann::json& snapshot,
                             std::initializer_list<const char*> keys) {
  if (!snapshot.is_object()) {
    return {};
  }
  for (const char* key : keys) {
    const auto found = snapshot.find(key);
    if (found != snapshot.end() && found->is_string()) {
      return found->get<std::string>();
    }
  }
  if (const auto status = snapshot.find("status"); status != snapshot.end() && status->is_object()) {
    for (const char* key : keys) {
      const auto found = status->find(key);
      if (found != status->end() && found->is_string()) {
        return found->get<std::string>();
      }
    }
  }
  return {};
}

void AddRootIfUsable(std::vector<std::filesystem::path>& roots,
                     const std::filesystem::path& root) {
  if (root.empty()) {
    return;
  }
  std::error_code ec;
  if (std::filesystem::is_directory(root, ec) && !ec) {
    roots.push_back(root);
  }
}

}  // namespace

std::filesystem::path ResolveTrackerStatePathForHost(const HostOptions& options) {
  if (!options.tracker_state_path.empty()) {
    return options.tracker_state_path;
  }
  const auto bundle_name = !options.tracker_bundle_path.empty()
                               ? options.tracker_bundle_path.filename().string()
                               : (!options.tracker_pack_path.empty()
                                      ? options.tracker_pack_path.filename().string()
                                      : std::string("default"));
  return options.save_directory / "tracker" / bundle_name / "state.json";
}

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

nlohmann::json BuildTrackerErrorSnapshot(std::string code, std::string message) {
  return nlohmann::json{
      {"schema", "sekailink.tracker.snapshot.v1"},
      {"status",
       {{"state", "error"},
        {"error", std::move(code)},
        {"message", std::move(message)}}},
      {"summary", {{"checked", 0}, {"total", 0}}},
  };
}

nlohmann::json BuildTrackerLoadingSnapshot(std::string message) {
  return nlohmann::json{
      {"schema", "sekailink.tracker.snapshot.v1"},
      {"status",
       {{"state", "loading"},
        {"message", std::move(message)}}},
      {"summary", {{"checked", 0}, {"total", 0}}},
  };
}

void RefreshTrackerAssetResolverForHost(const HostOptions& options,
                                        const std::optional<TrackerBundle>& tracker_bundle,
                                        const TrackerRuntime* tracker_runtime,
                                        HostTrackerAssetResolver& asset_resolver) {
  std::vector<std::filesystem::path> roots;
  AddRootIfUsable(roots, options.tracker_assets_root);
  if (tracker_runtime != nullptr) {
    const auto snapshot_root =
        SnapshotStringAt(tracker_runtime->AuthoritativeState().snapshot,
                         {"assets_root", "assetsRoot", "tracker_assets_root", "trackerAssetsRoot"});
    AddRootIfUsable(roots, snapshot_root);
  }
  AddRootIfUsable(roots, options.tracker_pack_path);
  if (tracker_bundle.has_value() && !tracker_bundle->bundle_root.empty()) {
    AddRootIfUsable(roots, tracker_bundle->bundle_root);
  }
  asset_resolver.SetRoots(std::move(roots));
}

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
                                     std::string& last_error) {
  if (!options.tracker_required) {
    return true;
  }
  if (options.tracker_bundle_path.empty() && options.tracker_snapshot_path.empty() &&
      options.tracker_pack_path.empty()) {
    tracker_runtime.ApplyServerSnapshot(BuildTrackerErrorSnapshot(
        "tracker_missing",
        "No compatible tracker pack or snapshot was provided. Legacy tracker fallback is disabled."));
    RefreshTrackerAssetResolverForHost(options, tracker_bundle, &tracker_runtime, asset_resolver);
    tracker_state_path = ResolveTrackerStatePathForHost(options);
    tracker_snapshot_path = ResolveTrackerSnapshotPathForHost(options, sklmi_companion_runtime);
    tracker_command_log_path = ResolveTrackerCommandLogPathForHost(options, sklmi_companion_runtime);
    last_mutation_serial = tracker_runtime.MutationSerial();
    tracker_dirty = true;
    tracker_active = true;
    std::cerr << "[sekaiemu-libretro-spike] tracker runtime active: error=tracker_missing"
              << " snapshot=" << (tracker_snapshot_path.empty() ? std::filesystem::path("-")
                                                                : tracker_snapshot_path)
              << " commands=" << (tracker_command_log_path.empty() ? std::filesystem::path("-")
                                                                   : tracker_command_log_path)
              << "\n";
    return true;
  }

  tracker_runtime.ApplyServerSnapshot(BuildTrackerLoadingSnapshot("Loading tracker..."));
  RefreshTrackerAssetResolverForHost(options, tracker_bundle, &tracker_runtime, asset_resolver);
  tracker_state_path = ResolveTrackerStatePathForHost(options);
  tracker_snapshot_path = ResolveTrackerSnapshotPathForHost(options, sklmi_companion_runtime);
  tracker_command_log_path = ResolveTrackerCommandLogPathForHost(options, sklmi_companion_runtime);
  last_mutation_serial = tracker_runtime.MutationSerial();
  tracker_dirty = true;
  tracker_active = true;

  try {
    if (!options.tracker_bundle_path.empty()) {
      tracker_bundle = TrackerBundle::LoadFromPath(options.tracker_bundle_path);
      tracker_runtime.LoadBundle(*tracker_bundle);
    }
    RefreshTrackerAssetResolverForHost(options, tracker_bundle, &tracker_runtime, asset_resolver);
    tracker_state_path = ResolveTrackerStatePathForHost(options);
    tracker_snapshot_path = ResolveTrackerSnapshotPathForHost(options, sklmi_companion_runtime);
    tracker_command_log_path = ResolveTrackerCommandLogPathForHost(options, sklmi_companion_runtime);
    if (!tracker_state_path.empty() && std::filesystem::exists(tracker_state_path)) {
      tracker_runtime.LoadPersistedState(TrackerRuntime::ReadPersistedState(tracker_state_path));
    }
    last_mutation_serial = tracker_runtime.MutationSerial();
    tracker_dirty = true;
    tracker_active = true;
    std::cerr << "[sekaiemu-libretro-spike] tracker runtime active: bundle="
              << (options.tracker_bundle_path.empty() ? std::filesystem::path("-")
                                                      : options.tracker_bundle_path)
              << " snapshot=" << (tracker_snapshot_path.empty() ? std::filesystem::path("-")
                                                                : tracker_snapshot_path)
              << " commands=" << (tracker_command_log_path.empty() ? std::filesystem::path("-")
                                                                   : tracker_command_log_path)
              << " state=" << tracker_state_path << "\n";
  } catch (const std::exception& exception) {
    last_error = std::string("tracker_initialize_failed: ") + exception.what();
    tracker_runtime.ApplyServerSnapshot(BuildTrackerErrorSnapshot(
        "tracker_initialize_failed",
        exception.what()));
    RefreshTrackerAssetResolverForHost(options, tracker_bundle, &tracker_runtime, asset_resolver);
    last_mutation_serial = tracker_runtime.MutationSerial();
    tracker_dirty = true;
    tracker_active = true;
    std::cerr << "[sekaiemu-libretro-spike] tracker runtime active: error="
              << last_error << "\n";
    return true;
  }
  return true;
}

void SaveTrackerStateForHost(TrackerRuntime& tracker_runtime,
                             bool tracker_active,
                             const std::filesystem::path& tracker_state_path,
                             std::uint64_t frame_counter,
                             std::uint64_t& last_save_frame,
                             bool& tracker_dirty,
                             const char* reason) {
  if (!tracker_active || tracker_state_path.empty()) {
    return;
  }
  try {
    tracker_runtime.SavePersistedState(tracker_state_path);
    last_save_frame = frame_counter;
    tracker_dirty = false;
    if (reason != nullptr) {
      const auto& authoritative = tracker_runtime.AuthoritativeState();
      const auto& observed = tracker_runtime.ObservedState();
      const auto resolved = tracker_runtime.ResolvedViewState();
      std::cerr << "[sekaiemu-libretro-spike] tracker state saved (" << reason
                << "): " << tracker_state_path
                << " checked=" << authoritative.checked_locations.size()
                << " local_checked=" << observed.locally_checked_locations.size()
                << " received=" << authoritative.received_items.size()
                << " local_received=" << observed.locally_received_items.size()
                << " recent=" << observed.recent_events.size()
                << " map=" << (resolved.active_map_id.empty() ? "-" : resolved.active_map_id)
                << " tab=" << (resolved.active_tab_id.empty() ? "-" : resolved.active_tab_id)
                << "\n";
    }
  } catch (const std::exception& exception) {
    std::cerr << "[sekaiemu-libretro-spike] tracker state save failed: "
              << exception.what() << "\n";
  }
}

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
                               std::uint64_t autosave_frame_interval) {
  if (!tracker_active) {
    return;
  }

  const bool snapshot_enabled = !tracker_snapshot_path.empty();
  const bool poll_now = last_poll_frame == 0 ||
                        frame_counter <= last_poll_frame ||
                        frame_counter - last_poll_frame >=
                            (snapshot_enabled ? snapshot_poll_frame_interval : poll_frame_interval);
  if (poll_now) {
    if (snapshot_enabled) {
      tracker_dirty = PumpTrackerSnapshot(tracker_snapshot_path, tracker_snapshot_reader, tracker_runtime) ||
                      tracker_dirty;
    } else {
      tracker_dirty = PumpTrackerRoomMetadata(sklmi_companion_runtime.RoomStatePath(),
                                              room_state_last_write_time,
                                              tracker_runtime) ||
                      tracker_dirty;
    }
    tracker_dirty = PumpTrackerTraceEvents(sklmi_companion_runtime.TraceLogPath(),
                                           trace_offset,
                                           item_label_by_key,
                                           tracker_runtime) ||
                    tracker_dirty;
    last_poll_frame = frame_counter;
  }

  tracker_runtime.Tick(1.0 / 60.0);
  if (tracker_runtime.MutationSerial() != last_mutation_serial) {
    last_mutation_serial = tracker_runtime.MutationSerial();
    tracker_dirty = true;
  }

  if (tracker_dirty &&
      (frame_counter == 0 || frame_counter - last_save_frame >= autosave_frame_interval)) {
    SaveTrackerStateForHost(tracker_runtime,
                            tracker_active,
                            tracker_state_path,
                            frame_counter,
                            last_save_frame,
                            tracker_dirty,
                            "autosave");
  }
}

}  // namespace sekaiemu::spike
