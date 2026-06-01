#pragma once

#include "sekailink_server/game_session.hpp"
#include "sekailink_server/room_registry.hpp"

#include <string>
#include <utility>
#include <vector>

namespace sekailink_server {

class GameSessionRegistry;

struct GameRoomProjectionOptions {
  std::string room_id_prefix = "game";
  RoomType room_type = RoomType::Async;
  bool include_acknowledged_items = true;
};

[[nodiscard]] std::string projected_room_id_for_session_slot(const std::string& session_name,
                                                             int slot_id,
                                                             const GameRoomProjectionOptions& options = {});
[[nodiscard]] RoomSession project_game_session_slot_to_room_session(const std::string& session_name,
                                                                    const GameSession& session,
                                                                    int slot_id,
                                                                    const GameRoomProjectionOptions& options = {});
[[nodiscard]] std::vector<std::pair<std::string, RoomSession>> project_game_session_to_rooms(
    const std::string& session_name,
    const GameSession& session,
    const GameRoomProjectionOptions& options = {});

void sync_game_session_projection(const std::string& session_name,
                                  const GameSession& session,
                                  RoomRegistry& room_registry,
                                  const GameRoomProjectionOptions& options = {});
void sync_all_game_session_projections(const GameSessionRegistry& game_registry,
                                       RoomRegistry& room_registry,
                                       const GameRoomProjectionOptions& options = {});

}  // namespace sekailink_server
