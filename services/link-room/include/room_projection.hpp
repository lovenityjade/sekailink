#pragma once

#include "sekailink_server/room_projection_store.hpp"

#include "sekailink_server/room_audit_store.hpp"

#include <filesystem>
#include <vector>

namespace sekailink_server {

struct RoomProjectionBatch {
  nlohmann::json room_record = nlohmann::json::object();
  std::vector<nlohmann::json> room_event_records;
  std::vector<nlohmann::json> client_report_records;
};

RoomProjectionBatch build_projection_batch(
    const RoomStateSnapshot& snapshot,
    const std::vector<RoomEvent>& events,
    const std::vector<ClientReport>& reports);
RoomProjectionBatch build_projection_batch(
    const RoomStateSnapshot& snapshot,
    const std::vector<RoomEvent>& events,
    const std::vector<ClientReport>& reports,
    std::size_t event_start_index,
    std::size_t report_start_index);

nlohmann::json project_room_record(const RoomStateSnapshot& snapshot);
nlohmann::json project_room_event_record(const std::string& room_id, const RoomEvent& event);
nlohmann::json project_client_report_record(const std::string& room_id, const ClientReport& report);

class RoomProjectionSpool : public RoomProjectionStore {
 public:
  explicit RoomProjectionSpool(std::filesystem::path root);

  void append_batch(const RoomProjectionBatch& batch) override;
  [[nodiscard]] std::vector<nlohmann::json> read_room_records() const;
  [[nodiscard]] std::vector<nlohmann::json> read_room_event_records() const;
  [[nodiscard]] std::vector<nlohmann::json> read_client_report_records() const;

 private:
  void append_jsonl(const std::filesystem::path& path, const nlohmann::json& payload);
  [[nodiscard]] std::vector<nlohmann::json> read_jsonl(const std::filesystem::path& path) const;

  std::filesystem::path root_;
};

}  // namespace sekailink_server
