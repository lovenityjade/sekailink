#pragma once

#include "sekailink_server/room_state.hpp"

#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <string>
#include <string_view>
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
  std::optional<std::string> seed_name;
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

struct RuntimeHeartbeatUpdate {
  std::optional<std::string> runtime_kind;
  std::optional<std::string> runtime_session_name;
  std::optional<std::string> driver_instance_id;
  std::optional<std::string> linkedworld_id;
  std::optional<std::string> core_profile;
  std::optional<std::string> client_name;
  std::optional<std::string> client_version;
  std::optional<bool> connected;
};

struct RuntimeTicketRequest {
  std::string session_name;
  int slot_id = 0;
  std::string client_kind = "runtime";
  std::optional<std::string> driver_instance_id;
  std::optional<std::string> linkedworld_id;
  std::optional<std::string> core_profile;
};

struct RuntimeEventRequest {
  int slot_id = 0;
  std::string session_token;
  std::string event_type;
  std::int64_t canonical_id = 0;
  std::optional<std::string> driver_instance_id;
  std::optional<std::string> linkedworld_id;
  std::optional<std::string> core_profile;
};

struct PendingDeliveryView {
  std::int64_t delivery_id = 0;
  std::int64_t item_id = 0;
  std::string item_name;
  std::int64_t location_id = 0;
  int sender_slot = 0;
  std::string sender_alias;
  int flags = 0;
  std::optional<std::string> event_key;
  std::optional<std::string> mapped_value;
  std::optional<std::string> tracker_semantic_id;
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
  void set_seed_metadata(
      std::optional<std::string> seed_name,
      std::optional<std::string> seed_id,
      std::optional<std::string> seed_hash,
      std::optional<std::string> tracker_pack,
      std::optional<std::string> tracker_variant);
  void set_stored_data(std::string key, nlohmann::json value);
  void set_slot_data(nlohmann::json slot_data);
  void apply_linkedworld_surface(const nlohmann::json& linkedworld_payload);
  [[nodiscard]] bool apply_seed_contract(const nlohmann::json& seed_contract, std::string* reason = nullptr);
  void set_location_mapping(std::int64_t location_id, int receiver_slot, int item_id, std::string location_name);
  void heartbeat_runtime(RuntimeHeartbeatUpdate update);
  void disconnect_runtime(std::optional<std::string> reason = std::nullopt);
  [[nodiscard]] std::optional<std::string> issue_runtime_ticket(RuntimeTicketRequest request, std::string* reason = nullptr);
  [[nodiscard]] std::optional<bool> record_runtime_event(RuntimeEventRequest request, std::string* reason = nullptr);
  [[nodiscard]] std::optional<std::vector<PendingDeliveryView>> pending_deliveries(
      int slot_id,
      std::string_view session_token,
      std::string* reason = nullptr) const;
  [[nodiscard]] std::optional<bool> acknowledge_delivery(
      int slot_id,
      std::int64_t delivery_id,
      std::string_view session_token,
      std::string* reason = nullptr);
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
  [[nodiscard]] int next_received_item_index() const;
  [[nodiscard]] std::size_t pending_delivery_count() const;
  [[nodiscard]] std::vector<ReceivedItemView> received_items_from_index(int index) const;
  [[nodiscard]] nlohmann::json sync_payload(int from_item_index = 0) const;
  [[nodiscard]] RoomStateSnapshot snapshot() const;

 private:
  void touch_activity();
  void append_event(std::string event_type, nlohmann::json payload = nlohmann::json::object());
  void ensure_runtime_state();
  void rebuild_linkedworld_item_semantics();
  void enrich_received_item_from_linkedworld(ReceivedItemView& item) const;
  bool runtime_request_authorized(int slot_id, std::string_view session_token, std::string* reason) const;
  PlayerRoomView* find_player(int slot);
  const PlayerRoomView* find_player(int slot) const;

  RoomSessionConfig config_;
  std::vector<PlayerRoomView> players_;
  std::set<std::int64_t> checked_locations_;
  std::vector<std::int64_t> missing_locations_;
  std::vector<ReceivedItemView> received_items_;
  std::map<std::string, nlohmann::json> stored_data_;
  nlohmann::json slot_data_ = nlohmann::json::object();
  nlohmann::json linkedworld_surface_ = nlohmann::json::object();
  nlohmann::json seed_contract_ = nlohmann::json::object();
  std::map<std::int64_t, nlohmann::json> linkedworld_item_semantics_by_id_;
  std::map<std::string, nlohmann::json> linkedworld_item_semantics_by_name_;
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
  std::optional<RuntimeBridgeState> runtime_state_;
  AsyncState async_state_;
};

}  // namespace sekailink_server
