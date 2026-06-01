#pragma once

#include "tracker_runtime.hpp"
#include "tracker_snapshot_io.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>

namespace sekaiemu::spike {

bool PumpTrackerSnapshot(const std::filesystem::path& snapshot_path,
                         TrackerSnapshotReader& snapshot_reader,
                         TrackerRuntime& tracker_runtime);

bool PumpTrackerRoomMetadata(const std::filesystem::path& room_state_path,
                             std::optional<std::filesystem::file_time_type>& last_write_time,
                             TrackerRuntime& tracker_runtime);

bool PumpTrackerTraceEvents(const std::filesystem::path& trace_path,
                            std::uintmax_t& trace_offset,
                            std::unordered_map<std::string, std::string>& item_label_by_key,
                            TrackerRuntime& tracker_runtime);

}  // namespace sekaiemu::spike
