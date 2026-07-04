#pragma once

#include "sekailink_server/room_session.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace sekailink_server {

class RoomAuditStore {
 public:
  explicit RoomAuditStore(std::filesystem::path root);

  void append_event(const std::string& room_id, const RoomEvent& event);
  void append_client_report(const std::string& room_id, const ClientReport& report);
  void append_snapshot(const RoomStateSnapshot& snapshot);

  [[nodiscard]] std::vector<nlohmann::json> read_events(const std::string& room_id) const;
  [[nodiscard]] std::vector<nlohmann::json> read_client_reports(const std::string& room_id) const;
  [[nodiscard]] std::vector<nlohmann::json> read_snapshots(const std::string& room_id) const;
  [[nodiscard]] std::vector<std::string> read_room_ids() const;
  [[nodiscard]] std::filesystem::path room_directory(const std::string& room_id) const;

 private:
  void append_jsonl(const std::filesystem::path& path, const nlohmann::json& payload);
  [[nodiscard]] std::vector<nlohmann::json> read_jsonl(const std::filesystem::path& path) const;
  void ensure_room_index_entry(const std::string& room_id);
  [[nodiscard]] std::filesystem::path room_index_path() const;

  std::filesystem::path root_;
};

}  // namespace sekailink_server
