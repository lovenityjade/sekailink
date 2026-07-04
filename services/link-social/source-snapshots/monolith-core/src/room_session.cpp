#include "sekailink_server/room_session.hpp"

#include <algorithm>
#include <utility>

namespace sekailink_server {

RoomSession::RoomSession(RoomSessionConfig config) : config_(std::move(config)) {
  async_state_.expires_at = config_.expires_at;
  async_state_.last_player_activity = utc_timestamp_now();
}

RoomSession RoomSession::from_snapshot(
    const RoomStateSnapshot& snapshot,
    std::vector<RoomEvent> events,
    std::vector<ClientReport> client_reports) {
  RoomSession session(RoomSessionConfig{
      .room_id = snapshot.room_id,
      .room_type = snapshot.room_type,
      .game = snapshot.game,
      .team_id = snapshot.team_id,
      .slot_id = snapshot.slot_id,
      .slot_name = snapshot.slot_name,
      .slot_alias = snapshot.slot_alias,
      .seed_id = snapshot.seed_id,
      .seed_hash = snapshot.seed_hash,
      .patch_url = snapshot.patch_url,
      .tracker_pack = snapshot.tracker_pack,
      .tracker_variant = snapshot.tracker_variant,
      .sni_required = snapshot.sni_required,
      .memory_backend_state = snapshot.memory_backend_state,
      .expires_at = snapshot.async_state.has_value() ? snapshot.async_state->expires_at : std::nullopt,
  });
  session.players_ = snapshot.players;
  session.checked_locations_ = std::set<std::int64_t>(snapshot.checked_locations.begin(), snapshot.checked_locations.end());
  session.missing_locations_ = snapshot.missing_locations;
  session.received_items_ = snapshot.received_items;
  session.stored_data_ = snapshot.stored_data;
  session.slot_data_ = snapshot.slot_data;
  session.location_to_item_ = snapshot.location_to_item;
  session.location_to_item_id_ = snapshot.location_to_item_id;
  session.location_names_ = snapshot.location_names;
  session.player_aliases_ = snapshot.player_aliases;
  session.milestones_ = snapshot.milestones;
  session.notifications_ = snapshot.notifications;
  session.hints_ = snapshot.hints;
  session.er_hint_data_ = snapshot.er_hint_data;
  session.game_options_ = snapshot.game_options;
  session.hint_points_ = snapshot.hint_points;
  session.hints_used_ = snapshot.hints_used;
  session.events_ = std::move(events);
  session.client_reports_ = std::move(client_reports);
  session.tracker_connected_ = snapshot.tracker_connected;
  session.emu_connected_ = snapshot.emu_connected;
  if (snapshot.async_state.has_value()) {
    session.async_state_ = *snapshot.async_state;
  }
  return session;
}

void RoomSession::upsert_player(PlayerRoomView player) {
  if (auto* existing = find_player(player.slot)) {
    *existing = std::move(player);
  } else {
    players_.push_back(std::move(player));
  }
  player_aliases_.clear();
  for (const auto& entry : players_) {
    player_aliases_[entry.slot] = entry.alias;
  }
}

void RoomSession::connect_player(int slot) {
  if (auto* player = find_player(slot); player != nullptr && !player->connected) {
    player->connected = true;
    ++activity_summary_.player_connections;
    touch_activity();
    append_event("player_connected", {{"slot", slot}});
  }
}

void RoomSession::disconnect_player(int slot) {
  if (auto* player = find_player(slot); player != nullptr && player->connected) {
    player->connected = false;
    ++activity_summary_.player_disconnections;
    touch_activity();
    append_event("player_disconnected", {{"slot", slot}});
  }
}

void RoomSession::mark_tracker_connected(bool connected) {
  tracker_connected_ = connected;
  if (connected) {
    ++activity_summary_.tracker_connections;
  }
  touch_activity();
  append_event("tracker_connection_state", {{"connected", connected}});
}

void RoomSession::mark_emu_connected(bool connected) {
  emu_connected_ = connected;
  if (connected) {
    ++activity_summary_.emu_connections;
  }
  touch_activity();
  append_event("emu_connection_state", {{"connected", connected}});
}

bool RoomSession::record_check(std::int64_t location_id) {
  const auto inserted = checked_locations_.insert(location_id).second;
  if (!inserted) {
    return false;
  }
  ++activity_summary_.checks_recorded;
  missing_locations_.erase(
      std::remove(missing_locations_.begin(), missing_locations_.end(), location_id),
      missing_locations_.end());
  touch_activity();
  append_event("location_checked", {{"location_id", location_id}});
  return true;
}

void RoomSession::set_missing_locations(std::vector<std::int64_t> missing_locations) {
  missing_locations_ = std::move(missing_locations);
}

void RoomSession::enqueue_received_item(ReceivedItemView item) {
  received_items_.push_back(std::move(item));
  ++activity_summary_.items_received;
  touch_activity();
  append_event("item_received", {{"item_index", received_items_.back().index},
                                  {"item_id", received_items_.back().item_id}});
}

void RoomSession::set_stored_data(std::string key, nlohmann::json value) {
  stored_data_[std::move(key)] = std::move(value);
  touch_activity();
}

void RoomSession::set_slot_data(nlohmann::json slot_data) {
  slot_data_ = std::move(slot_data);
}

void RoomSession::set_location_mapping(
    std::int64_t location_id, int receiver_slot, int item_id, std::string location_name) {
  location_to_item_[location_id] = receiver_slot;
  location_to_item_id_[location_id] = item_id;
  location_names_[location_id] = std::move(location_name);
}

void RoomSession::set_allowed_players(std::vector<int> allowed_players) {
  async_state_.allowed_players = std::move(allowed_players);
}

void RoomSession::set_expires_at(std::optional<std::string> expires_at) {
  async_state_.expires_at = std::move(expires_at);
}

void RoomSession::set_async_notification_state(std::optional<std::string> state) {
  async_state_.async_notification_state = std::move(state);
}

void RoomSession::set_daily_summary_state(std::optional<std::string> state) {
  async_state_.daily_summary_state = std::move(state);
}

void RoomSession::set_suspend_state(std::optional<std::string> state) {
  async_state_.suspend_state = std::move(state);
}

void RoomSession::set_milestones(std::vector<std::string> milestones) {
  milestones_ = std::move(milestones);
}

void RoomSession::set_notifications(std::vector<std::string> notifications) {
  notifications_ = std::move(notifications);
}

void RoomSession::set_hints(nlohmann::json hints) {
  hints_ = std::move(hints);
}

void RoomSession::set_er_hint_data(nlohmann::json er_hint_data) {
  er_hint_data_ = std::move(er_hint_data);
}

void RoomSession::set_game_options(nlohmann::json game_options) {
  game_options_ = std::move(game_options);
}

void RoomSession::set_hint_points(int hint_points) {
  hint_points_ = hint_points;
}

void RoomSession::set_hints_used(int hints_used) {
  hints_used_ = hints_used;
}

void RoomSession::ingest_client_report(ClientReport report) {
  if (report.timestamp.empty()) {
    report.timestamp = utc_timestamp_now();
  }
  client_reports_.push_back(std::move(report));
  touch_activity();
  append_event("client_report_ingested", {{"severity", client_reports_.back().severity},
                                          {"report_type", client_reports_.back().report_type},
                                          {"source", client_reports_.back().source}});
}

bool RoomSession::is_expired_at(const std::string& now_utc) const {
  if (!async_state_.expires_at.has_value()) {
    return false;
  }
  return now_utc >= *async_state_.expires_at;
}

RoomActivitySummary RoomSession::activity_summary() const {
  return activity_summary_;
}

const std::vector<RoomEvent>& RoomSession::events() const {
  return events_;
}

const std::vector<ClientReport>& RoomSession::client_reports() const {
  return client_reports_;
}

RoomStateSnapshot RoomSession::snapshot() const {
  RoomStateSnapshot snapshot;
  snapshot.room_id = config_.room_id;
  snapshot.room_type = config_.room_type;
  snapshot.connection_state = emu_connected_ || tracker_connected_ ? ConnectionState::Online
                                                                   : ConnectionState::Offline;
  snapshot.game = config_.game;
  snapshot.team_id = config_.team_id;
  snapshot.slot_id = config_.slot_id;
  snapshot.slot_name = config_.slot_name;
  snapshot.slot_alias = config_.slot_alias;
  snapshot.players = players_;
  snapshot.checked_locations.assign(checked_locations_.begin(), checked_locations_.end());
  snapshot.missing_locations = missing_locations_;
  snapshot.received_items = received_items_;
  snapshot.stored_data = stored_data_;
  snapshot.milestones = milestones_;
  snapshot.notifications = notifications_;
  snapshot.hints = hints_;
  snapshot.er_hint_data = er_hint_data_;
  snapshot.game_options = game_options_;
  snapshot.hint_points = hint_points_;
  snapshot.hints_used = hints_used_;
  snapshot.patch_url = config_.patch_url;
  snapshot.tracker_pack = config_.tracker_pack;
  snapshot.tracker_variant = config_.tracker_variant;
  snapshot.seed_id = config_.seed_id;
  snapshot.seed_hash = config_.seed_hash;
  snapshot.slot_data = slot_data_;
  snapshot.location_to_item = location_to_item_;
  snapshot.location_to_item_id = location_to_item_id_;
  snapshot.location_names = location_names_;
  snapshot.player_aliases = player_aliases_;
  snapshot.emu_connected = emu_connected_;
  snapshot.tracker_connected = tracker_connected_;
  snapshot.sni_required = config_.sni_required;
  snapshot.memory_backend_state = config_.memory_backend_state;
  snapshot.capabilities.supports_async = config_.room_type == RoomType::Async;
  snapshot.capabilities.supports_tracker_state = true;
  snapshot.capabilities.supports_mobile_summary = config_.room_type == RoomType::Async;
  snapshot.capabilities.supports_sni_local = config_.sni_required;
  if (config_.room_type == RoomType::Async) {
    snapshot.async_state = async_state_;
  }
  return snapshot;
}

void RoomSession::touch_activity() {
  async_state_.last_player_activity = utc_timestamp_now();
}

void RoomSession::append_event(std::string event_type, nlohmann::json payload) {
  events_.push_back(RoomEvent{
      .event_type = std::move(event_type),
      .timestamp = utc_timestamp_now(),
      .payload = std::move(payload),
  });
}

PlayerRoomView* RoomSession::find_player(int slot) {
  const auto it = std::find_if(players_.begin(), players_.end(), [slot](const auto& player) {
    return player.slot == slot;
  });
  return it == players_.end() ? nullptr : &(*it);
}

const PlayerRoomView* RoomSession::find_player(int slot) const {
  const auto it = std::find_if(players_.begin(), players_.end(), [slot](const auto& player) {
    return player.slot == slot;
  });
  return it == players_.end() ? nullptr : &(*it);
}

}  // namespace sekailink_server
