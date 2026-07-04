#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace sekailink_server {

enum class RoomType {
  Live,
  Async,
};

enum class ConnectionState {
  Offline,
  Online,
};

struct CapabilitySet {
  bool supports_async = false;
  bool supports_tracker_state = true;
  bool supports_mobile_summary = false;
  bool supports_sni_local = false;
  bool supports_achievements = false;
  bool supports_shop = false;
  bool supports_2fa_mobile_approve = false;
};

struct PlayerRoomView {
  int team = 0;
  int slot = 0;
  std::string name;
  std::string alias;
  std::string game;
  bool connected = false;
};

struct ReceivedItemView {
  int index = 0;
  std::int64_t item_id = 0;
  std::string item_name;
  std::int64_t location_id = 0;
  int sender_slot = 0;
  std::string sender_alias;
  int flags = 0;
};

struct AsyncState {
  std::optional<std::string> expires_at;
  std::optional<std::string> last_player_activity;
  std::vector<int> allowed_players;
  std::optional<std::string> daily_summary_state;
  std::optional<std::string> async_notification_state;
  std::optional<std::string> suspend_state;
};

struct LogEventContext {
  std::string service_name;
  std::string server_name;
  std::string severity;
  std::string event_type;
  std::optional<std::string> request_id;
  std::optional<std::string> session_id;
  std::optional<std::string> user_id;
  std::optional<std::string> room_id;
  std::optional<std::string> lobby_id;
};

struct ClientReport {
  std::string report_type;
  std::string source;
  std::string severity;
  std::string message;
  std::string timestamp;
  std::optional<std::string> request_id;
  std::optional<std::string> session_id;
  std::optional<std::string> user_id;
  std::optional<std::string> room_id;
  std::optional<std::string> lobby_id;
  std::optional<std::string> game;
  std::optional<std::string> runtime;
  nlohmann::json details = nlohmann::json::object();
};

struct RoomStateSnapshot {
  std::string room_id;
  RoomType room_type = RoomType::Live;
  ConnectionState connection_state = ConnectionState::Offline;
  std::string game;
  int team_id = 0;
  int slot_id = 0;
  std::string slot_name;
  std::string slot_alias;
  std::vector<PlayerRoomView> players;
  std::vector<std::int64_t> checked_locations;
  std::vector<std::int64_t> missing_locations;
  std::vector<ReceivedItemView> received_items;
  std::map<std::string, nlohmann::json> stored_data;
  std::vector<std::string> milestones;
  std::vector<std::string> notifications;
  nlohmann::json hints = nlohmann::json::array();
  nlohmann::json er_hint_data = nlohmann::json::object();
  nlohmann::json game_options = nlohmann::json::object();
  int hint_points = 0;
  int hints_used = 0;
  std::optional<std::string> patch_url;
  std::optional<std::string> tracker_pack;
  std::optional<std::string> tracker_variant;
  std::optional<std::string> seed_id;
  std::optional<std::string> seed_hash;
  nlohmann::json slot_data = nlohmann::json::object();
  std::map<std::int64_t, int> location_to_item;
  std::map<std::int64_t, int> location_to_item_id;
  std::map<std::int64_t, std::string> location_names;
  std::map<int, std::string> player_aliases;
  bool emu_connected = false;
  bool tracker_connected = false;
  bool sni_required = false;
  std::optional<std::string> memory_backend_state;
  CapabilitySet capabilities;
  std::optional<AsyncState> async_state;
  std::string generated_at;
};

std::string room_type_to_string(RoomType room_type);
std::string connection_state_to_string(ConnectionState connection_state);
std::string utc_timestamp_now();
nlohmann::json to_json(const CapabilitySet& capabilities);
nlohmann::json to_json(const PlayerRoomView& player);
nlohmann::json to_json(const ReceivedItemView& item);
nlohmann::json to_json(const AsyncState& async_state);
nlohmann::json to_json(const LogEventContext& ctx);
nlohmann::json to_json(const ClientReport& report);
nlohmann::json to_json(const RoomStateSnapshot& snapshot);

}  // namespace sekailink_server
