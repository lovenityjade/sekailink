#include "sekailink_server/room_session.hpp"

#include "sekailink_server/room_linkedworld.hpp"

#include <algorithm>
#include <cstdint>
#include <iomanip>
#include <sstream>
#include <set>
#include <string>
#include <utility>

namespace sekailink_server {

namespace {

constexpr std::uint64_t kFnvOffsetBasis = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

nlohmann::json slot_data_summary(const nlohmann::json& slot_data) {
  nlohmann::json keys = nlohmann::json::array();
  if (slot_data.is_object()) {
    for (auto it = slot_data.begin(); it != slot_data.end(); ++it) {
      keys.push_back(it.key());
    }
  }
  return {
      {"slot_data_shape", slot_data.type_name()},
      {"slot_data_entry_count", slot_data.is_object() ? slot_data.size() : 0},
      {"slot_data_keys", std::move(keys)},
  };
}

nlohmann::json sync_items_json(const std::vector<ReceivedItemView>& items) {
  nlohmann::json payload = nlohmann::json::array();
  for (const auto& item : items) {
    payload.push_back(to_json(item));
  }
  return payload;
}

void set_optional_string_if_missing(
    std::optional<std::string>& target,
    const nlohmann::json& source,
    const char* key) {
  if (target.has_value() || !source.contains(key) || source.at(key).is_null() || !source.at(key).is_string()) {
    return;
  }
  target = source.at(key).get<std::string>();
}

std::string json_string_or_empty(const nlohmann::json& source, const char* key) {
  if (!source.contains(key) || source.at(key).is_null() || !source.at(key).is_string()) {
    return "";
  }
  return source.at(key).get<std::string>();
}

nlohmann::json json_array_or_empty(const nlohmann::json& source, const char* key) {
  if (!source.contains(key) || source.at(key).is_null() || !source.at(key).is_array()) {
    return nlohmann::json::array();
  }
  return source.at(key);
}

nlohmann::json json_object_or_empty(const nlohmann::json& source, const char* key) {
  if (!source.contains(key) || source.at(key).is_null() || !source.at(key).is_object()) {
    return nlohmann::json::object();
  }
  return source.at(key);
}

const nlohmann::json* find_slot_contract(const nlohmann::json& seed_contract, int slot_id) {
  if (!seed_contract.contains("slots") || !seed_contract.at("slots").is_array()) {
    return nullptr;
  }
  for (const auto& slot : seed_contract.at("slots")) {
    if (slot.is_object() && slot.value("slot_id", 0) == slot_id) {
      return &slot;
    }
  }
  return nullptr;
}

const nlohmann::json* find_config_version_contract(const nlohmann::json& seed_contract, int slot_id) {
  if (!seed_contract.contains("config_versions") || !seed_contract.at("config_versions").is_array()) {
    return nullptr;
  }
  for (const auto& version : seed_contract.at("config_versions")) {
    if (version.is_object() && version.value("slot_id", 0) == slot_id) {
      return &version;
    }
  }
  return nullptr;
}

const nlohmann::json* config_snapshot_for_slot(const nlohmann::json& seed_contract, int slot_id) {
  if (const auto* slot = find_slot_contract(seed_contract, slot_id);
      slot != nullptr && slot->contains("config_snapshot") && slot->at("config_snapshot").is_object()) {
    return &slot->at("config_snapshot");
  }
  if (const auto* config_version = find_config_version_contract(seed_contract, slot_id);
      config_version != nullptr &&
      config_version->contains("config_snapshot") &&
      config_version->at("config_snapshot").is_object()) {
    return &config_version->at("config_snapshot");
  }
  return nullptr;
}

std::int64_t config_version_id_for_slot(const nlohmann::json& seed_contract, int slot_id) {
  if (const auto* slot = find_slot_contract(seed_contract, slot_id);
      slot != nullptr && slot->contains("config_version_id") && slot->at("config_version_id").is_number_integer()) {
    return slot->at("config_version_id").get<std::int64_t>();
  }
  if (const auto* config_version = find_config_version_contract(seed_contract, slot_id);
      config_version != nullptr &&
      config_version->contains("config_version_id") &&
      config_version->at("config_version_id").is_number_integer()) {
    return config_version->at("config_version_id").get<std::int64_t>();
  }
  return 0;
}

std::string fnv1a_hex(std::string_view value) {
  std::uint64_t hash = kFnvOffsetBasis;
  for (const unsigned char ch : value) {
    hash ^= static_cast<std::uint64_t>(ch);
    hash *= kFnvPrime;
  }
  std::ostringstream out;
  out << std::hex << std::setw(16) << std::setfill('0') << hash;
  return out.str();
}

std::string runtime_ticket_seed(const RoomSessionConfig& config, const RuntimeTicketRequest& request) {
  std::ostringstream seed;
  seed << config.room_id << '|'
       << request.session_name << '|'
       << request.slot_id << '|'
       << utc_timestamp_now() << '|'
       << config.game << '|'
       << request.client_kind;
  if (request.driver_instance_id.has_value()) {
    seed << '|' << *request.driver_instance_id;
  }
  if (request.linkedworld_id.has_value()) {
    seed << '|' << *request.linkedworld_id;
  }
  if (request.core_profile.has_value()) {
    seed << '|' << *request.core_profile;
  }
  return fnv1a_hex(seed.str()) + fnv1a_hex(seed.str() + "|sekailink");
}

}  // namespace

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
      .seed_name = snapshot.seed_name,
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
  session.linkedworld_surface_ = snapshot.linkedworld_surface;
  session.seed_contract_ = snapshot.seed_contract;
  session.rebuild_linkedworld_item_semantics();
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
  session.runtime_state_ = snapshot.runtime_state;
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
  if (!runtime_state_.has_value()) {
    runtime_state_.emplace();
  }
  runtime_state_->last_check_report_at = utc_timestamp_now();
  append_event("location_checked", {{"location_id", location_id}});
  return true;
}

void RoomSession::set_missing_locations(std::vector<std::int64_t> missing_locations) {
  missing_locations_ = std::move(missing_locations);
  touch_activity();
  append_event("missing_locations_updated",
               {{"missing_location_count", missing_locations_.size()}});
}

void RoomSession::enqueue_received_item(ReceivedItemView item) {
  const auto next_index = next_received_item_index();
  if (item.index < 0 || item.index < next_index) {
    item.index = next_index;
  }
  enrich_received_item_from_linkedworld(item);
  received_items_.push_back(std::move(item));
  ++activity_summary_.items_received;
  touch_activity();
  if (!runtime_state_.has_value()) {
    runtime_state_.emplace();
  }
  runtime_state_->last_item_delivery_at = utc_timestamp_now();
  const auto& received_item = received_items_.back();
  append_event("item_received",
               {{"item_index", received_item.index},
                {"item_id", received_item.item_id},
                {"item_name", received_item.item_name},
                {"location_id", received_item.location_id},
                {"sender_slot", received_item.sender_slot},
                {"sender_alias", received_item.sender_alias},
                {"flags", received_item.flags},
                {"event_key", received_item.event_key.has_value() ? nlohmann::json(*received_item.event_key) : nlohmann::json(nullptr)},
                {"mapped_value", received_item.mapped_value.has_value() ? nlohmann::json(*received_item.mapped_value) : nlohmann::json(nullptr)},
                {"tracker_semantic_id", received_item.tracker_semantic_id.has_value()
                                            ? nlohmann::json(*received_item.tracker_semantic_id)
                                            : nlohmann::json(nullptr)},
                {"received_item_count", received_items_.size()}});
}

void RoomSession::set_seed_metadata(std::optional<std::string> seed_name,
                                    std::optional<std::string> seed_id,
                                    std::optional<std::string> seed_hash,
                                    std::optional<std::string> tracker_pack,
                                    std::optional<std::string> tracker_variant) {
  config_.seed_name = std::move(seed_name);
  config_.seed_id = std::move(seed_id);
  config_.seed_hash = std::move(seed_hash);
  config_.tracker_pack = std::move(tracker_pack);
  config_.tracker_variant = std::move(tracker_variant);
  touch_activity();
  append_event("seed_metadata_updated",
               {{"seed_name", config_.seed_name.has_value() ? nlohmann::json(*config_.seed_name) : nlohmann::json(nullptr)},
                {"seed_id", config_.seed_id.has_value() ? nlohmann::json(*config_.seed_id) : nlohmann::json(nullptr)},
                {"seed_hash", config_.seed_hash.has_value() ? nlohmann::json(*config_.seed_hash) : nlohmann::json(nullptr)},
                {"tracker_pack", config_.tracker_pack.has_value() ? nlohmann::json(*config_.tracker_pack) : nlohmann::json(nullptr)},
                {"tracker_variant", config_.tracker_variant.has_value() ? nlohmann::json(*config_.tracker_variant) : nlohmann::json(nullptr)}});
}

void RoomSession::set_stored_data(std::string key, nlohmann::json value) {
  const auto stored_key = key;
  stored_data_[std::move(key)] = std::move(value);
  touch_activity();
  append_event("stored_data_updated",
               {{"key", stored_key},
                {"stored_data_entry_count", stored_data_.size()}});
}

void RoomSession::set_slot_data(nlohmann::json slot_data) {
  slot_data_ = std::move(slot_data);
  touch_activity();
  append_event("slot_data_updated", slot_data_summary(slot_data_));
}

void RoomSession::apply_linkedworld_surface(const nlohmann::json& linkedworld_payload) {
  linkedworld_surface_ = build_linkedworld_room_surface(linkedworld_payload);
  rebuild_linkedworld_item_semantics();
  if (const auto tracker_pack = json_string_or_empty(linkedworld_surface_, "tracker_pack"); !tracker_pack.empty()) {
    config_.tracker_pack = tracker_pack;
  }
  if (const auto tracker_variant = json_string_or_empty(linkedworld_surface_, "tracker_variant"); !tracker_variant.empty()) {
    config_.tracker_variant = tracker_variant;
  }
  if (!runtime_state_.has_value() && linkedworld_surface_.contains("linkedworld_id") &&
      !linkedworld_surface_.at("linkedworld_id").is_null()) {
    runtime_state_.emplace();
    runtime_state_->linkedworld_id = linkedworld_surface_.at("linkedworld_id").get<std::string>();
  }
  touch_activity();
  append_event("linkedworld_surface_applied",
               {{"linkedworld_id", json_string_or_empty(linkedworld_surface_, "linkedworld_id")},
                {"display_name", json_string_or_empty(linkedworld_surface_, "display_name")},
                {"module_id", json_string_or_empty(linkedworld_surface_, "module_id")},
                {"tracker_pack", json_string_or_empty(linkedworld_surface_, "tracker_pack")},
                {"tracker_variant", json_string_or_empty(linkedworld_surface_, "tracker_variant")},
                {"room_metadata_required_keys", json_array_or_empty(linkedworld_surface_, "room_metadata_required_keys")},
                {"slot_data_visible_fields", json_array_or_empty(linkedworld_surface_, "slot_data_visible_fields")},
                {"item_semantic_count", json_array_or_empty(linkedworld_surface_, "item_semantics").size()},
                {"bridge", json_object_or_empty(linkedworld_surface_, "bridge")}});
}

bool RoomSession::apply_seed_contract(const nlohmann::json& seed_contract, std::string* reason) {
  if (!seed_contract.is_object()) {
    if (reason != nullptr) {
      *reason = "invalid_seed_contract";
    }
    return false;
  }
  if (seed_contract.value("schema_version", std::string{}) != "sekailink-link-room-seed-contract-v1") {
    if (reason != nullptr) {
      *reason = "seed_contract_schema_mismatch";
    }
    return false;
  }
  if (seed_contract.value("generation_scope", std::string{}) != "multiworld") {
    if (reason != nullptr) {
      *reason = "seed_contract_scope_mismatch";
    }
    return false;
  }
  const auto* slot = find_slot_contract(seed_contract, config_.slot_id);
  if (slot == nullptr) {
    if (reason != nullptr) {
      *reason = "seed_contract_slot_not_found";
    }
    return false;
  }

  seed_contract_ = seed_contract;
  if (const auto seed_id = json_string_or_empty(seed_contract_, "seed_id"); !seed_id.empty()) {
    config_.seed_id = seed_id;
  }

  const auto linkedworld_id = json_string_or_empty(*slot, "linkedworld_id");
  if (!linkedworld_id.empty()) {
    ensure_runtime_state();
    runtime_state_->linkedworld_id = linkedworld_id;
  }

  if (const auto* snapshot = config_snapshot_for_slot(seed_contract_, config_.slot_id); snapshot != nullptr) {
    if (snapshot->contains("values") && snapshot->at("values").is_object()) {
      slot_data_ = snapshot->at("values");
    } else {
      slot_data_ = *snapshot;
    }
    game_options_ = slot_data_;
  }

  touch_activity();
  append_event("seed_contract_applied",
               {{"seed_id", json_string_or_empty(seed_contract_, "seed_id")},
                {"contract_room_id", json_string_or_empty(seed_contract_, "room_id")},
                {"slot_id", config_.slot_id},
                {"linkedworld_id", linkedworld_id},
                {"config_version_id", config_version_id_for_slot(seed_contract_, config_.slot_id)},
                {"slot_data_entry_count", slot_data_.is_object() ? slot_data_.size() : 0},
                {"seed_contract_summary", seed_contract_summary(seed_contract_)}});
  return true;
}

void RoomSession::set_location_mapping(
    std::int64_t location_id, int receiver_slot, int item_id, std::string location_name) {
  location_to_item_[location_id] = receiver_slot;
  location_to_item_id_[location_id] = item_id;
  location_names_[location_id] = std::move(location_name);
  touch_activity();
  append_event("location_mapping_updated",
               {{"location_id", location_id},
                {"receiver_slot", receiver_slot},
                {"item_id", item_id},
                {"location_name", location_names_[location_id]},
                {"location_mapping_count", location_to_item_.size()}});
}

void RoomSession::heartbeat_runtime(RuntimeHeartbeatUpdate update) {
  ensure_runtime_state();
  auto& runtime = *runtime_state_;
  if (update.runtime_kind.has_value()) runtime.runtime_kind = std::move(update.runtime_kind);
  if (update.runtime_session_name.has_value()) runtime.runtime_session_name = std::move(update.runtime_session_name);
  if (update.driver_instance_id.has_value()) runtime.driver_instance_id = std::move(update.driver_instance_id);
  if (update.linkedworld_id.has_value()) runtime.linkedworld_id = std::move(update.linkedworld_id);
  if (update.core_profile.has_value()) runtime.core_profile = std::move(update.core_profile);
  if (update.client_name.has_value()) runtime.client_name = std::move(update.client_name);
  if (update.client_version.has_value()) runtime.client_version = std::move(update.client_version);
  runtime.connected = update.connected.value_or(true);
  runtime.last_heartbeat_at = utc_timestamp_now();
  runtime.last_disconnect_reason.reset();
  ++runtime.heartbeat_count;
  touch_activity();
  append_event("runtime_heartbeat",
               {{"runtime_kind", runtime.runtime_kind.has_value() ? nlohmann::json(*runtime.runtime_kind) : nlohmann::json(nullptr)},
                {"runtime_session_name", runtime.runtime_session_name.has_value() ? nlohmann::json(*runtime.runtime_session_name) : nlohmann::json(nullptr)},
                {"driver_instance_id", runtime.driver_instance_id.has_value() ? nlohmann::json(*runtime.driver_instance_id) : nlohmann::json(nullptr)},
                {"linkedworld_id", runtime.linkedworld_id.has_value() ? nlohmann::json(*runtime.linkedworld_id) : nlohmann::json(nullptr)},
                {"core_profile", runtime.core_profile.has_value() ? nlohmann::json(*runtime.core_profile) : nlohmann::json(nullptr)},
                {"client_name", runtime.client_name.has_value() ? nlohmann::json(*runtime.client_name) : nlohmann::json(nullptr)},
                {"client_version", runtime.client_version.has_value() ? nlohmann::json(*runtime.client_version) : nlohmann::json(nullptr)},
                {"connected", runtime.connected},
                {"heartbeat_count", runtime.heartbeat_count}});
}

void RoomSession::disconnect_runtime(std::optional<std::string> reason) {
  ensure_runtime_state();
  runtime_state_->connected = false;
  runtime_state_->last_disconnect_reason = std::move(reason);
  touch_activity();
  append_event("runtime_disconnected",
               {{"reason", runtime_state_->last_disconnect_reason.has_value()
                               ? nlohmann::json(*runtime_state_->last_disconnect_reason)
                               : nlohmann::json(nullptr)}});
}

std::optional<std::string> RoomSession::issue_runtime_ticket(RuntimeTicketRequest request, std::string* reason) {
  if (request.slot_id != config_.slot_id) {
    if (reason != nullptr) {
      *reason = "slot_mismatch";
    }
    return std::nullopt;
  }
  if (request.client_kind != "runtime") {
    if (reason != nullptr) {
      *reason = "unsupported_client_kind";
    }
    return std::nullopt;
  }

  ensure_runtime_state();
  auto& runtime = *runtime_state_;
  const auto token = runtime_ticket_seed(config_, request);
  const auto issued_at = utc_timestamp_now();
  runtime.runtime_kind = request.client_kind;
  runtime.runtime_session_name = request.session_name;
  runtime.driver_instance_id = std::move(request.driver_instance_id);
  runtime.linkedworld_id = std::move(request.linkedworld_id);
  runtime.core_profile = std::move(request.core_profile);
  runtime.session_token_hash = fnv1a_hex(token);
  runtime.ticket_issued_at = issued_at;
  runtime.connected = true;
  runtime.last_disconnect_reason.reset();
  touch_activity();
  append_event("runtime_ticket_issued",
               {{"session_name", request.session_name},
                {"slot_id", request.slot_id},
                {"client_kind", request.client_kind},
                {"driver_instance_id", runtime.driver_instance_id.has_value()
                                           ? nlohmann::json(*runtime.driver_instance_id)
                                           : nlohmann::json(nullptr)},
                {"linkedworld_id", runtime.linkedworld_id.has_value()
                                       ? nlohmann::json(*runtime.linkedworld_id)
                                       : nlohmann::json(nullptr)},
                {"core_profile", runtime.core_profile.has_value()
                                     ? nlohmann::json(*runtime.core_profile)
                                     : nlohmann::json(nullptr)},
                {"ticket_issued_at", issued_at}});
  return token;
}

std::optional<bool> RoomSession::record_runtime_event(RuntimeEventRequest request, std::string* reason) {
  if (!runtime_request_authorized(request.slot_id, request.session_token, reason)) {
    return std::nullopt;
  }
  if (request.event_type != "location_checked") {
    if (reason != nullptr) {
      *reason = "unsupported_runtime_event";
    }
    return std::nullopt;
  }

  ensure_runtime_state();
  auto& runtime = *runtime_state_;
  if (request.driver_instance_id.has_value()) runtime.driver_instance_id = std::move(request.driver_instance_id);
  if (request.linkedworld_id.has_value()) runtime.linkedworld_id = std::move(request.linkedworld_id);
  if (request.core_profile.has_value()) runtime.core_profile = std::move(request.core_profile);
  runtime.connected = true;
  runtime.last_disconnect_reason.reset();
  return record_check(request.canonical_id);
}

std::optional<std::vector<PendingDeliveryView>> RoomSession::pending_deliveries(
    int slot_id,
    std::string_view session_token,
    std::string* reason) const {
  if (!runtime_request_authorized(slot_id, session_token, reason)) {
    return std::nullopt;
  }

  std::set<std::int64_t> acknowledged;
  if (runtime_state_.has_value()) {
    acknowledged.insert(
        runtime_state_->acknowledged_delivery_ids.begin(),
        runtime_state_->acknowledged_delivery_ids.end());
  }

  std::vector<PendingDeliveryView> pending;
  pending.reserve(received_items_.size());
  for (const auto& item : received_items_) {
    const auto delivery_id = static_cast<std::int64_t>(item.index);
    if (acknowledged.contains(delivery_id)) {
      continue;
    }
    pending.push_back(PendingDeliveryView{
        .delivery_id = delivery_id,
        .item_id = item.item_id,
        .item_name = item.item_name,
        .location_id = item.location_id,
        .sender_slot = item.sender_slot,
        .sender_alias = item.sender_alias,
        .flags = item.flags,
        .event_key = item.event_key,
        .mapped_value = item.mapped_value,
        .tracker_semantic_id = item.tracker_semantic_id,
    });
  }
  return pending;
}

std::optional<bool> RoomSession::acknowledge_delivery(
    int slot_id,
    std::int64_t delivery_id,
    std::string_view session_token,
    std::string* reason) {
  if (!runtime_request_authorized(slot_id, session_token, reason)) {
    return std::nullopt;
  }

  ensure_runtime_state();
  auto& runtime = *runtime_state_;
  const auto already_acked = std::find(
                                 runtime.acknowledged_delivery_ids.begin(),
                                 runtime.acknowledged_delivery_ids.end(),
                                 delivery_id) != runtime.acknowledged_delivery_ids.end();
  const auto delivery_exists = std::find_if(received_items_.begin(), received_items_.end(), [delivery_id](const auto& item) {
                               return static_cast<std::int64_t>(item.index) == delivery_id;
                             }) != received_items_.end();
  if (already_acked) {
    runtime.connected = true;
    runtime.last_disconnect_reason.reset();
    return true;
  }
  if (!delivery_exists) {
    if (reason != nullptr) {
      *reason = "delivery_not_found";
    }
    return false;
  }

  runtime.acknowledged_delivery_ids.push_back(delivery_id);
  runtime.last_delivery_ack_id = delivery_id;
  runtime.last_delivery_ack_at = utc_timestamp_now();
  runtime.connected = true;
  runtime.last_disconnect_reason.reset();
  touch_activity();
  append_event("delivery_acknowledged",
               {{"delivery_id", delivery_id},
                {"acknowledged_delivery_count", runtime.acknowledged_delivery_ids.size()}});
  return true;
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

int RoomSession::next_received_item_index() const {
  if (received_items_.empty()) {
    return 0;
  }
  return received_items_.back().index + 1;
}

std::size_t RoomSession::pending_delivery_count() const {
  if (!runtime_state_.has_value()) {
    return received_items_.size();
  }
  std::set<std::int64_t> acknowledged(
      runtime_state_->acknowledged_delivery_ids.begin(),
      runtime_state_->acknowledged_delivery_ids.end());
  std::size_t count = 0;
  for (const auto& item : received_items_) {
    if (!acknowledged.contains(static_cast<std::int64_t>(item.index))) {
      ++count;
    }
  }
  return count;
}

std::vector<ReceivedItemView> RoomSession::received_items_from_index(int index) const {
  std::vector<ReceivedItemView> items;
  for (const auto& item : received_items_) {
    if (item.index >= index) {
      items.push_back(item);
    }
  }
  return items;
}

nlohmann::json RoomSession::sync_payload(int from_item_index) const {
  const auto snapshot = this->snapshot();
  const auto items = received_items_from_index(from_item_index);
  return {
      {"room",
       {
           {"room_id", snapshot.room_id},
           {"room_type", room_type_to_string(snapshot.room_type)},
           {"game", snapshot.game},
           {"team_id", snapshot.team_id},
           {"slot_id", snapshot.slot_id},
           {"slot_name", snapshot.slot_name},
           {"slot_alias", snapshot.slot_alias},
           {"seed_name", snapshot.seed_name.has_value() ? nlohmann::json(*snapshot.seed_name) : nlohmann::json(nullptr)},
           {"seed_id", snapshot.seed_id.has_value() ? nlohmann::json(*snapshot.seed_id) : nlohmann::json(nullptr)},
           {"seed_hash", snapshot.seed_hash.has_value() ? nlohmann::json(*snapshot.seed_hash) : nlohmann::json(nullptr)},
           {"tracker_pack", snapshot.tracker_pack.has_value() ? nlohmann::json(*snapshot.tracker_pack) : nlohmann::json(nullptr)},
           {"tracker_variant", snapshot.tracker_variant.has_value() ? nlohmann::json(*snapshot.tracker_variant) : nlohmann::json(nullptr)},
           {"slot_data", snapshot.slot_data},
           {"linkedworld_surface", snapshot.linkedworld_surface},
           {"seed_contract", snapshot.seed_contract},
           {"seed_contract_summary", seed_contract_summary(snapshot.seed_contract)},
           {"runtime_state", snapshot.runtime_state.has_value() ? to_json(*snapshot.runtime_state) : nlohmann::json(nullptr)},
       }},
      {"connections",
       {
           {"connection_state", connection_state_to_string(snapshot.connection_state)},
           {"tracker_connected", snapshot.tracker_connected},
           {"emu_connected", snapshot.emu_connected},
       }},
      {"checks",
       {
           {"checked_locations", snapshot.checked_locations},
           {"missing_locations", snapshot.missing_locations},
           {"checked_count", snapshot.checked_locations.size()},
           {"missing_count", snapshot.missing_locations.size()},
       }},
      {"items",
       {
           {"from_index", from_item_index},
           {"next_index", next_received_item_index()},
           {"received_count", snapshot.received_items.size()},
           {"items", sync_items_json(items)},
       }},
  };
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
  snapshot.seed_name = config_.seed_name;
  snapshot.patch_url = config_.patch_url;
  snapshot.tracker_pack = config_.tracker_pack;
  snapshot.tracker_variant = config_.tracker_variant;
  snapshot.seed_id = config_.seed_id;
  snapshot.seed_hash = config_.seed_hash;
  snapshot.slot_data = slot_data_;
  snapshot.linkedworld_surface = linkedworld_surface_;
  snapshot.seed_contract = seed_contract_;
  snapshot.location_to_item = location_to_item_;
  snapshot.location_to_item_id = location_to_item_id_;
  snapshot.location_names = location_names_;
  snapshot.player_aliases = player_aliases_;
  snapshot.emu_connected = emu_connected_;
  snapshot.tracker_connected = tracker_connected_;
  snapshot.sni_required = config_.sni_required;
  snapshot.memory_backend_state = config_.memory_backend_state;
  snapshot.runtime_state = runtime_state_;
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

void RoomSession::ensure_runtime_state() {
  if (!runtime_state_.has_value()) {
    runtime_state_.emplace();
  }
}

void RoomSession::rebuild_linkedworld_item_semantics() {
  linkedworld_item_semantics_by_id_.clear();
  linkedworld_item_semantics_by_name_.clear();
  if (!linkedworld_surface_.contains("item_semantics") || !linkedworld_surface_.at("item_semantics").is_array()) {
    return;
  }
  for (const auto& semantic : linkedworld_surface_.at("item_semantics")) {
    if (semantic.contains("item_id") && semantic.at("item_id").is_number_integer()) {
      linkedworld_item_semantics_by_id_[semantic.at("item_id").get<std::int64_t>()] = semantic;
    }
    if (semantic.contains("item_name") && semantic.at("item_name").is_string()) {
      linkedworld_item_semantics_by_name_[semantic.at("item_name").get<std::string>()] = semantic;
    }
  }
}

void RoomSession::enrich_received_item_from_linkedworld(ReceivedItemView& item) const {
  const nlohmann::json* semantic = nullptr;
  if (const auto by_id = linkedworld_item_semantics_by_id_.find(item.item_id);
      by_id != linkedworld_item_semantics_by_id_.end()) {
    semantic = &by_id->second;
  } else if (const auto by_name = linkedworld_item_semantics_by_name_.find(item.item_name);
             by_name != linkedworld_item_semantics_by_name_.end()) {
    semantic = &by_name->second;
  }
  if (semantic == nullptr) {
    return;
  }
  set_optional_string_if_missing(item.event_key, *semantic, "event_key");
  set_optional_string_if_missing(item.mapped_value, *semantic, "mapped_value");
  set_optional_string_if_missing(item.tracker_semantic_id, *semantic, "tracker_semantic_id");
  if (!item.tracker_semantic_id.has_value() && item.event_key.has_value()) {
    item.tracker_semantic_id = item.event_key;
  }
}

bool RoomSession::runtime_request_authorized(int slot_id, std::string_view session_token, std::string* reason) const {
  if (slot_id != config_.slot_id) {
    if (reason != nullptr) {
      *reason = "slot_mismatch";
    }
    return false;
  }
  if (!runtime_state_.has_value() || !runtime_state_->session_token_hash.has_value()) {
    if (reason != nullptr) {
      *reason = "missing_runtime_ticket";
    }
    return false;
  }
  if (session_token.empty() || fnv1a_hex(session_token) != *runtime_state_->session_token_hash) {
    if (reason != nullptr) {
      *reason = "invalid_session_token";
    }
    return false;
  }
  return true;
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
