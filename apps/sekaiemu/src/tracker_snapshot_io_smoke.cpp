#include "tracker_snapshot_io.hpp"

#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

int main() {
  try {
    const fs::path root = fs::temp_directory_path() / "sekaiemu-tracker-snapshot-io-smoke";
    std::error_code ec;
    fs::remove_all(root, ec);
    fs::create_directories(root);

    const fs::path snapshot_path = root / "tracker.snapshot.json";
    const fs::path command_log_path = root / "tracker.commands.jsonl";

    {
      std::ofstream output(snapshot_path, std::ios::binary | std::ios::trunc);
      output << R"({"schema":"sekailink.tracker.snapshot.v1","revision":1,"active_map":"light_world"})";
    }

    sekaiemu::spike::TrackerSnapshotReader reader;
    auto first = reader.Poll(snapshot_path);
    if (!first.changed || !first.ok || first.snapshot.value("revision", 0) != 1) {
      std::cerr << "tracker_snapshot_first_poll_failed\n";
      return 1;
    }

    auto second = reader.Poll(snapshot_path);
    if (second.changed) {
      std::cerr << "tracker_snapshot_unchanged_reparsed\n";
      return 1;
    }

    {
      std::ofstream output(snapshot_path, std::ios::binary | std::ios::trunc);
      output << "{invalid";
    }

    auto invalid = reader.Poll(snapshot_path);
    if (!invalid.changed || invalid.ok || invalid.error.empty()) {
      std::cerr << "tracker_snapshot_invalid_not_reported\n";
      return 1;
    }

    {
      std::ofstream output(snapshot_path, std::ios::binary | std::ios::trunc);
      output << R"({"schema":"sekailink.tracker.snapshot.v1","revision":2,"active_map":"dark_world"})";
    }

    auto third = reader.Poll(snapshot_path);
    if (!third.changed || !third.ok || third.snapshot.value("revision", 0) != 2) {
      std::cerr << "tracker_snapshot_recovery_failed\n";
      return 1;
    }

    std::string error;
    if (!sekaiemu::spike::AppendTrackerCommand(
            command_log_path,
            nlohmann::json{{"cmd", "tracker.set_tab"}, {"tab", "light_world"}},
            error)) {
      std::cerr << "tracker_command_append_failed:" << error << "\n";
      return 1;
    }

    std::ifstream input(command_log_path, std::ios::binary);
    std::string line;
    std::getline(input, line);
    const auto record = nlohmann::json::parse(line);
    if (record.value("cmd", std::string()) != "tracker.set_tab" || !record.contains("ts")) {
      std::cerr << "tracker_command_record_invalid\n";
      return 1;
    }

    std::cout << "tracker_snapshot_io_smoke_ok\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "tracker_snapshot_io_smoke_exception:" << exception.what() << "\n";
    return 1;
  }
}
