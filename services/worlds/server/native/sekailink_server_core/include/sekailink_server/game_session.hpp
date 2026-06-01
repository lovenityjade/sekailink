#pragma once

#include "sekailink_server/game_save_state.hpp"
#include "sekailink_server/game_world.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace sekailink_server {

enum class GameClientKind {
  Core,
  Runtime,
  Observer,
  Admin,
};

struct SessionTicket {
  std::string session_id;
  std::string session_token;
  int slot_id = 0;
  GameClientKind client_kind = GameClientKind::Core;
  std::optional<std::string> driver_instance_id;
  std::optional<std::string> linkedworld_id;
  std::optional<std::string> core_profile;
};

struct RuntimeEvent {
  std::string session_token;
  int slot_id = 0;
  std::string driver_instance_id;
  std::string linkedworld_id;
  std::string core_profile;
  std::string event_type;
  std::int64_t canonical_id = 0;
};

struct RuntimeEventResult {
  bool accepted = false;
  bool duplicate = false;
  std::string reason;
  std::vector<int> created_delivery_ids;
};

class GameSession {
 public:
  explicit GameSession(GameWorldPackage package);
  static std::optional<GameSession> from_save(GameWorldPackage package,
                                              const GameSaveState& save_state,
                                              std::string* error = nullptr);

  std::optional<SessionTicket> issue_session_ticket(int slot_id,
                                                    GameClientKind client_kind,
                                                    std::optional<std::string> driver_instance_id = std::nullopt,
                                                    std::optional<std::string> linkedworld_id = std::nullopt,
                                                    std::optional<std::string> core_profile = std::nullopt,
                                                    std::string* error = nullptr);

  RuntimeEventResult apply_runtime_event(const RuntimeEvent& event);
  std::vector<DeliveredItemRecord> pending_items_for_slot(int slot_id, const std::string& session_token) const;
  bool acknowledge_delivery(int slot_id, int delivery_id, const std::string& session_token);

  [[nodiscard]] const GameWorldPackage& package() const;
  [[nodiscard]] GameSaveState export_save_state() const;

 private:
  const SessionTicket* find_ticket(const std::string& session_token) const;
  SessionTicket* find_ticket(const std::string& session_token);
  std::optional<int> find_delivery_index(int delivery_id) const;
  static std::string make_token();

  GameWorldPackage package_;
  std::map<int, std::set<std::int64_t>> checked_locations_by_slot_;
  std::vector<DeliveredItemRecord> delivered_items_;
  int next_delivery_id_ = 1;
  int next_session_id_ = 1;
  std::map<std::string, SessionTicket> tickets_by_token_;
};

std::string game_client_kind_to_string(GameClientKind kind);

}  // namespace sekailink_server
