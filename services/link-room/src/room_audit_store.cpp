#include "sekailink_server/room_audit_store.hpp"

#include <fstream>
#include <algorithm>
#include <stdexcept>

namespace sekailink_server {

RoomAuditStore::RoomAuditStore(std::filesystem::path root) : root_(std::move(root)) {
  std::filesystem::create_directories(root_);
}

void RoomAuditStore::append_event(const std::string& room_id, const RoomEvent& event) {
  append_jsonl(
      room_directory(room_id) / "events.jsonl",
      {
          {"event_type", event.event_type},
          {"timestamp", event.timestamp},
          {"payload", event.payload},
      });
}

void RoomAuditStore::append_client_report(const std::string& room_id, const ClientReport& report) {
  append_jsonl(room_directory(room_id) / "client_reports.jsonl", to_json(report));
}

void RoomAuditStore::append_snapshot(const RoomStateSnapshot& snapshot) {
  ensure_room_index_entry(snapshot.room_id);
  append_jsonl(room_directory(snapshot.room_id) / "snapshots.jsonl", to_json(snapshot));
}

std::vector<nlohmann::json> RoomAuditStore::read_events(const std::string& room_id) const {
  return read_jsonl(room_directory(room_id) / "events.jsonl");
}

std::vector<nlohmann::json> RoomAuditStore::read_client_reports(const std::string& room_id) const {
  return read_jsonl(room_directory(room_id) / "client_reports.jsonl");
}

std::vector<nlohmann::json> RoomAuditStore::read_snapshots(const std::string& room_id) const {
  return read_jsonl(room_directory(room_id) / "snapshots.jsonl");
}

std::vector<std::string> RoomAuditStore::read_room_ids() const {
  if (!std::filesystem::exists(room_index_path())) {
    return {};
  }
  std::ifstream in(room_index_path());
  if (!in.is_open()) {
    throw std::runtime_error("room_audit_store_index_read_failed");
  }
  nlohmann::json value;
  in >> value;
  return value.value("room_ids", std::vector<std::string>{});
}

std::filesystem::path RoomAuditStore::room_directory(const std::string& room_id) const {
  return root_ / room_id;
}

void RoomAuditStore::append_jsonl(const std::filesystem::path& path, const nlohmann::json& payload) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream out(path, std::ios::app);
  if (!out.is_open()) {
    throw std::runtime_error("room_audit_store_open_failed");
  }
  out << payload.dump() << "\n";
}

std::vector<nlohmann::json> RoomAuditStore::read_jsonl(const std::filesystem::path& path) const {
  std::vector<nlohmann::json> entries;
  if (!std::filesystem::exists(path)) {
    return entries;
  }
  std::ifstream in(path);
  if (!in.is_open()) {
    throw std::runtime_error("room_audit_store_read_failed");
  }
  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }
    entries.push_back(nlohmann::json::parse(line));
  }
  return entries;
}

void RoomAuditStore::ensure_room_index_entry(const std::string& room_id) {
  auto room_ids = read_room_ids();
  if (std::find(room_ids.begin(), room_ids.end(), room_id) == room_ids.end()) {
    room_ids.push_back(room_id);
    std::sort(room_ids.begin(), room_ids.end());
    std::filesystem::create_directories(root_);
    std::ofstream out(room_index_path(), std::ios::trunc);
    if (!out.is_open()) {
      throw std::runtime_error("room_audit_store_index_write_failed");
    }
    out << nlohmann::json{{"room_ids", room_ids}}.dump(2);
  }
}

std::filesystem::path RoomAuditStore::room_index_path() const {
  return root_ / "rooms.json";
}

}  // namespace sekailink_server
