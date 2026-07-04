#include "tracker_snapshot_io.hpp"

#include <chrono>
#include <fstream>
#include <sstream>
#include <system_error>

namespace sekaiemu::spike {
namespace {

std::string ReadTextFile(const std::filesystem::path& path) {
  std::ifstream input(path, std::ios::binary);
  if (!input) {
    throw std::runtime_error("tracker_snapshot_open_failed:" + path.string());
  }
  std::ostringstream out;
  out << input.rdbuf();
  return out.str();
}

std::int64_t CurrentUnixTimestampMs() {
  const auto now = std::chrono::system_clock::now().time_since_epoch();
  return std::chrono::duration_cast<std::chrono::milliseconds>(now).count();
}

}  // namespace

void TrackerSnapshotReader::Reset() {
  last_path_.clear();
  last_write_time_.reset();
  last_size_ = 0;
  has_last_observation_ = false;
}

TrackerSnapshotPollResult TrackerSnapshotReader::Poll(const std::filesystem::path& path) {
  TrackerSnapshotPollResult result;
  if (path.empty()) {
    return result;
  }

  std::error_code ec;
  const bool exists = std::filesystem::exists(path, ec);
  if (ec || !exists) {
    return result;
  }

  const auto write_time = std::filesystem::last_write_time(path, ec);
  if (ec) {
    result.changed = true;
    result.error = "tracker_snapshot_stat_failed:" + path.string();
    return result;
  }
  const auto size = std::filesystem::file_size(path, ec);
  if (ec) {
    result.changed = true;
    result.error = "tracker_snapshot_stat_failed:" + path.string();
    return result;
  }

  const bool same_path = has_last_observation_ && last_path_ == path;
  const bool unchanged =
      same_path && last_write_time_.has_value() && *last_write_time_ == write_time && last_size_ == size;
  if (unchanged) {
    return result;
  }

  last_path_ = path;
  last_write_time_ = write_time;
  last_size_ = size;
  has_last_observation_ = true;
  result.changed = true;

  try {
    auto parsed = nlohmann::json::parse(ReadTextFile(path));
    if (!parsed.is_object()) {
      result.error = "tracker_snapshot_root_not_object:" + path.string();
      return result;
    }
    result.ok = true;
    result.snapshot = std::move(parsed);
    return result;
  } catch (const std::exception& exception) {
    result.error = std::string("tracker_snapshot_parse_failed:") + path.string() + ":" + exception.what();
    return result;
  }
}

bool AppendTrackerCommand(const std::filesystem::path& path,
                          nlohmann::json command,
                          std::string& error) {
  if (path.empty()) {
    error = "tracker_command_log_missing";
    return false;
  }
  if (!command.is_object()) {
    error = "tracker_command_invalid";
    return false;
  }
  if (!command.contains("ts")) {
    command["ts"] = CurrentUnixTimestampMs();
  }

  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  if (ec) {
    error = "tracker_command_mkdir_failed:" + path.parent_path().string();
    return false;
  }

  std::ofstream output(path, std::ios::binary | std::ios::app);
  if (!output) {
    error = "tracker_command_open_failed:" + path.string();
    return false;
  }
  output << command.dump() << '\n';
  if (!output) {
    error = "tracker_command_write_failed:" + path.string();
    return false;
  }
  return true;
}

}  // namespace sekaiemu::spike
