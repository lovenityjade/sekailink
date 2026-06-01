#pragma once

#include "sekailink_server/room_session.hpp"

namespace sekailink_server {

struct MultiServerImportOptions {
  std::string room_id;
  RoomType room_type = RoomType::Live;
  int team_id = 0;
  int slot_id = 0;
  std::optional<std::string> seed_id;
  std::optional<std::string> seed_hash;
  std::optional<std::string> patch_url;
  std::optional<std::string> tracker_pack;
  std::optional<std::string> tracker_variant;
  bool sni_required = false;
  std::optional<std::string> memory_backend_state;
  std::optional<std::string> expires_at;
};

RoomSession import_multiserver_semantics(
    const nlohmann::json& multiserver_state,
    const MultiServerImportOptions& options);

}  // namespace sekailink_server
