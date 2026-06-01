#pragma once

#include <filesystem>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace sekaiemu::spike {

struct TrackerSnapshotPollResult {
  bool changed = false;
  bool ok = false;
  nlohmann::json snapshot = nlohmann::json::object();
  std::string error;
};

class TrackerSnapshotReader {
 public:
  void Reset();
  TrackerSnapshotPollResult Poll(const std::filesystem::path& path);

 private:
  std::filesystem::path last_path_;
  std::optional<std::filesystem::file_time_type> last_write_time_;
  std::uintmax_t last_size_ = 0;
  bool has_last_observation_ = false;
};

bool AppendTrackerCommand(const std::filesystem::path& path,
                          nlohmann::json command,
                          std::string& error);

}  // namespace sekaiemu::spike
