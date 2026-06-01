#include "sekailink_server/game_session.hpp"

#include <openssl/rand.h>

#include <iomanip>
#include <sstream>

namespace sekailink_server {

namespace {

std::string hex_encode(const unsigned char* data, std::size_t size) {
  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (std::size_t index = 0; index < size; ++index) {
    output << std::setw(2) << static_cast<unsigned int>(data[index]);
  }
  return output.str();
}

bool is_runtime_event_type_allowed(const std::string& event_type) {
  return event_type == "location_checked";
}

}  // namespace

GameSession::GameSession(GameWorldPackage package) : package_(std::move(package)) {}

std::optional<GameSession> GameSession::from_save(GameWorldPackage package,
                                                  const GameSaveState& save_state,
                                                  std::string* error) {
  std::string validation_error;
  if (!validate_game_world_package(package, &validation_error)) {
    if (error) *error = validation_error;
    return std::nullopt;
  }
  if (!validate_game_save_state(save_state, &validation_error)) {
    if (error) *error = validation_error;
    return std::nullopt;
  }
  if (package.world_id != save_state.world_id) {
    if (error) *error = "game_save_world_id_mismatch";
    return std::nullopt;
  }
  if (package.seed_id != save_state.seed_id) {
    if (error) *error = "game_save_seed_id_mismatch";
    return std::nullopt;
  }

  GameSession session(std::move(package));
  for (const auto& [slot_id, locations] : save_state.checked_locations_by_slot) {
    session.checked_locations_by_slot_[slot_id] = std::set<std::int64_t>(locations.begin(), locations.end());
  }
  session.delivered_items_ = save_state.delivered_items;
  session.next_delivery_id_ = save_state.next_delivery_id;
  return session;
}

std::optional<SessionTicket> GameSession::issue_session_ticket(int slot_id,
                                                               GameClientKind client_kind,
                                                               std::optional<std::string> driver_instance_id,
                                                               std::optional<std::string> linkedworld_id,
                                                               std::optional<std::string> core_profile,
                                                               std::string* error) {
  if (!package_.slots.contains(slot_id)) {
    if (error) *error = "game_session_unknown_slot";
    return std::nullopt;
  }
  if (client_kind == GameClientKind::Runtime) {
    if (!driver_instance_id.has_value() || driver_instance_id->empty() ||
        !linkedworld_id.has_value() || linkedworld_id->empty()) {
      if (error) *error = "game_session_runtime_ticket_requires_binding";
      return std::nullopt;
    }
  }

  SessionTicket ticket{
      .session_id = "session-" + std::to_string(next_session_id_++),
      .session_token = make_token(),
      .slot_id = slot_id,
      .client_kind = client_kind,
      .driver_instance_id = std::move(driver_instance_id),
      .linkedworld_id = std::move(linkedworld_id),
      .core_profile = std::move(core_profile),
  };
  tickets_by_token_[ticket.session_token] = ticket;
  return ticket;
}

RuntimeEventResult GameSession::apply_runtime_event(const RuntimeEvent& event) {
  RuntimeEventResult result;
  const auto* ticket = find_ticket(event.session_token);
  if (ticket == nullptr) {
    result.reason = "invalid_session_token";
    return result;
  }
  if (ticket->client_kind != GameClientKind::Runtime) {
    result.reason = "session_not_runtime";
    return result;
  }
  if (ticket->slot_id != event.slot_id) {
    result.reason = "runtime_slot_mismatch";
    return result;
  }
  if (!ticket->driver_instance_id.has_value() || *ticket->driver_instance_id != event.driver_instance_id) {
    result.reason = "runtime_driver_instance_mismatch";
    return result;
  }
  if (!ticket->linkedworld_id.has_value() || *ticket->linkedworld_id != event.linkedworld_id ||
      package_.linkedworld_id != event.linkedworld_id) {
    result.reason = "runtime_linkedworld_mismatch";
    return result;
  }
  if (!is_runtime_event_type_allowed(event.event_type)) {
    result.reason = "runtime_event_type_forbidden";
    return result;
  }
  if (event.canonical_id <= 0) {
    result.reason = "runtime_invalid_canonical_id";
    return result;
  }
  const auto location_it = package_.locations.find(event.canonical_id);
  if (location_it == package_.locations.end()) {
    result.reason = "runtime_unknown_location";
    return result;
  }
  if (location_it->second.owner_slot != event.slot_id) {
    result.reason = "runtime_location_owner_mismatch";
    return result;
  }

  auto& checked = checked_locations_by_slot_[event.slot_id];
  if (!checked.insert(event.canonical_id).second) {
    result.duplicate = true;
    result.reason = "runtime_duplicate_check";
    return result;
  }

  const auto& location = location_it->second;
  delivered_items_.push_back(DeliveredItemRecord{
      .delivery_id = next_delivery_id_++,
      .receiver_slot = location.receiver_slot,
      .item_id = location.item_id,
      .item_name = location.item_name,
      .source_location_id = location.location_id,
      .sender_slot = location.owner_slot,
      .acknowledged = false,
  });

  result.accepted = true;
  result.reason = "accepted";
  result.created_delivery_ids.push_back(delivered_items_.back().delivery_id);
  return result;
}

std::vector<DeliveredItemRecord> GameSession::pending_items_for_slot(int slot_id, const std::string& session_token) const {
  const auto* ticket = find_ticket(session_token);
  if (ticket == nullptr || ticket->slot_id != slot_id) {
    return {};
  }
  std::vector<DeliveredItemRecord> pending;
  for (const auto& delivery : delivered_items_) {
    if (delivery.receiver_slot == slot_id && !delivery.acknowledged) {
      pending.push_back(delivery);
    }
  }
  return pending;
}

bool GameSession::acknowledge_delivery(int slot_id, int delivery_id, const std::string& session_token) {
  const auto* ticket = find_ticket(session_token);
  if (ticket == nullptr || ticket->slot_id != slot_id) {
    return false;
  }
  const auto delivery_index = find_delivery_index(delivery_id);
  if (!delivery_index.has_value()) {
    return false;
  }
  auto& delivery = delivered_items_.at(static_cast<std::size_t>(*delivery_index));
  if (delivery.receiver_slot != slot_id || delivery.acknowledged) {
    return false;
  }
  delivery.acknowledged = true;
  return true;
}

const GameWorldPackage& GameSession::package() const {
  return package_;
}

GameSaveState GameSession::export_save_state() const {
  GameSaveState state;
  state.world_id = package_.world_id;
  state.seed_id = package_.seed_id;
  state.next_delivery_id = next_delivery_id_;
  state.delivered_items = delivered_items_;
  for (const auto& [slot_id, locations] : checked_locations_by_slot_) {
    state.checked_locations_by_slot[slot_id] = std::vector<std::int64_t>(locations.begin(), locations.end());
  }
  return state;
}

const SessionTicket* GameSession::find_ticket(const std::string& session_token) const {
  const auto it = tickets_by_token_.find(session_token);
  return it == tickets_by_token_.end() ? nullptr : &it->second;
}

SessionTicket* GameSession::find_ticket(const std::string& session_token) {
  const auto it = tickets_by_token_.find(session_token);
  return it == tickets_by_token_.end() ? nullptr : &it->second;
}

std::optional<int> GameSession::find_delivery_index(int delivery_id) const {
  for (std::size_t index = 0; index < delivered_items_.size(); ++index) {
    if (delivered_items_[index].delivery_id == delivery_id) {
      return static_cast<int>(index);
    }
  }
  return std::nullopt;
}

std::string GameSession::make_token() {
  unsigned char buffer[16];
  if (RAND_bytes(buffer, sizeof(buffer)) != 1) {
    throw std::runtime_error("session_token_generation_failed");
  }
  return hex_encode(buffer, sizeof(buffer));
}

std::string game_client_kind_to_string(GameClientKind kind) {
  switch (kind) {
    case GameClientKind::Core: return "core";
    case GameClientKind::Runtime: return "runtime";
    case GameClientKind::Observer: return "observer";
    case GameClientKind::Admin: return "admin";
  }
  return "core";
}

}  // namespace sekailink_server
