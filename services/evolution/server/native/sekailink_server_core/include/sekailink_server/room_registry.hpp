#pragma once

#include "sekailink_server/room_session.hpp"

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace sekailink_server {

class RoomAuditStore;

class RoomRegistry {
 public:
  bool create_room(RoomSessionConfig config);
  bool insert_room(std::string room_id, RoomSession session);
  bool remove_room(const std::string& room_id);
  bool has_room(const std::string& room_id) const;
  RoomSession* find_room(const std::string& room_id);
  const RoomSession* find_room(const std::string& room_id) const;
  std::vector<std::string> list_room_ids(
      std::optional<std::size_t> limit = std::nullopt,
      std::size_t offset = 0,
      const std::optional<std::string>& query = std::nullopt,
      const std::optional<std::string>& room_type = std::nullopt,
      const std::optional<std::string>& connection_state = std::nullopt) const;
  std::vector<std::string> expired_room_ids(const std::string& now_utc) const;
  bool remove_expired_rooms(const std::string& now_utc);

 private:
  std::unordered_map<std::string, RoomSession> rooms_;
};

std::optional<RoomSessionConfig> room_session_config_from_json(const nlohmann::json& input);
nlohmann::json handle_room_server_command(RoomRegistry& registry, const nlohmann::json& command);
nlohmann::json handle_room_server_command_with_audit(
    RoomRegistry& registry,
    const nlohmann::json& command,
    RoomAuditStore* audit_store);

}  // namespace sekailink_server
