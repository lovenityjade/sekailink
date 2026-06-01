#pragma once

#include "sekailink_server/room_state.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace sekailink_server {

struct RoomSessionConfig {
  std::string room_id;
  RoomType room_type = RoomType::Live;
  std::string game;
  int team_id = 0;
  int slot_id = 0;
  std::string slot_name;
  std::string slot_alias;
  std::optional<std::string> seed_id;
  std::optional<std::string> seed_hash;
  std::optional<std::string> patch_url;
  std::optional<std::string> tracker_pack;
  std::optional<std::string> tracker_variant;
  bool sni_required = false;
  std::optional<std::string> memory_backend_state;
  std::optional<std::string> expires_at;
};

struct RoomEvent {
  std::string event_type;
  std::string timestamp;
  nlohmann::json payload = nlohmann::json::object();
};

struct RoomActivitySummary {
  int player_connections = 0;
  int player_disconnections = 0;
  int checks_recorded = 0;
  int items_received = 0;
  int tracker_connections = 0;
  int emu_connections = 0;
};

class RoomSession {
 public:
  explicit RoomSession(RoomSessionConfig config);
  static RoomSession from_snapshot(
      const RoomStateSnapshot& snapshot,
      std::vector<RoomEvent> events = {},
      std::vector<ClientReport> client_reports = {});

  void upsert_player(PlayerRoomView player);
  void connect_player(int slot);
  void disconnect_player(int slot);
  void mark_tracker_connected(bool connected);
  void mark_emu_connected(bool connected);
  bool record_check(std::int64_t location_id);
  void set_missing_locations(std::vector<std::int64_t> missing_locations);
  void enqueue_received_item(ReceivedItemView item);
  void set_stored_data(std::string key, nlohmann::json value);
  void set_slot_data(nlohmann::json slot_data);
  void set_location_mapping(std::int64_t location_id, int receiver_slot, int item_id, std::string location_name);
  void set_allowed_players(std::vector<int> allowed_players);
  void set_expires_at(std::optional<std::string> expires_at);
  void set_async_notification_state(std::optional<std::string> state);
  void set_daily_summary_state(std::optional<std::string> state);
  void set_suspend_state(std::optional<std::string> state);
  void set_milestones(std::vector<std::string> milestones);
  void set_notifications(std::vector<std::string> notifications);
  void set_hints(nlohmann::json hints);
  void set_er_hint_data(nlohmann::json er_hint_data);
  void set_game_options(nlohmann::json game_options);
  void set_hint_points(int hint_points);
  void set_hints_used(int hints_used);
  void ingest_client_report(ClientReport report);

  [[nodiscard]] bool is_expired_at(const std::string& now_utc) const;
  [[nodiscard]] RoomActivitySummary activity_summary() const;
  [[nodiscard]] const std::vector<RoomEvent>& events() const;
  [[nodiscard]] const std::vector<ClientReport>& client_reports() const;
  [[nodiscard]] RoomStateSnapshot snapshot() const;

 private:
  void touch_activity();
  void append_event(std::string event_type, nlohmann::json payload = nlohmann::json::object());
  PlayerRoomView* find_player(int slot);
  const PlayerRoomView* find_player(int slot) const;

  RoomSessionConfig config_;
  std::vector<PlayerRoomView> players_;
  std::set<std::int64_t> checked_locations_;
  std::vector<std::int64_t> missing_locations_;
  std::vector<ReceivedItemView> received_items_;
  std::map<std::string, nlohmann::json> stored_data_;
  nlohmann::json slot_data_ = nlohmann::json::object();
  std::map<std::int64_t, int> location_to_item_;
  std::map<std::int64_t, int> location_to_item_id_;
  std::map<std::int64_t, std::string> location_names_;
  std::map<int, std::string> player_aliases_;
  std::vector<std::string> milestones_;
  std::vector<std::string> notifications_;
  nlohmann::json hints_ = nlohmann::json::array();
  nlohmann::json er_hint_data_ = nlohmann::json::object();
  nlohmann::json game_options_ = nlohmann::json::object();
  int hint_points_ = 0;
  int hints_used_ = 0;
  std::vector<RoomEvent> events_;
  std::vector<ClientReport> client_reports_;
  RoomActivitySummary activity_summary_;
  bool tracker_connected_ = false;
  bool emu_connected_ = false;
  AsyncState async_state_;
};

}  // namespace sekailink_server
