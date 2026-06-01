#include "sekailink_server/multiserver_semantics.hpp"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

using namespace sekailink_server;

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  try {
    const nlohmann::json multiserver_state = {
        {"slot_info",
         {
             {"1", {{"name", "Jade"}, {"game", "A Link to the Past"}}},
             {"2", {{"name", "Cloud"}, {"game", "A Link to the Past"}}},
         }},
        {"player_names",
         {
             {"0:1", "Jade"},
             {"0:2", "Cloud"},
         }},
        {"name_aliases",
         {
             {"0:1", "Sekai Jade"},
         }},
        {"client_connections",
         {
             {"1", 1},
             {"2", 0},
         }},
        {"checked_locations",
         {
             {"0:1", {1001}},
         }},
        {"missing_locations",
         {
             {"0:1", {1002, 1003}},
         }},
        {"received_items",
         {
             {"0:1",
              {{
                  {"item", 2001},
                  {"item_name", "Bow"},
                  {"location", 1001},
                  {"player", 2},
                  {"sender_name", "Cloud"},
                  {"flags", 0},
              }}},
         }},
        {"stored_data",
         {
             {"lttp_big_key_eastern", true},
         }},
        {"hints",
         {
             {"0:1",
              {{
                  {"receiving_player", 1},
                  {"finding_player", 2},
                  {"location", 1002},
                  {"item", 2002},
                  {"found", false},
                  {"status", "priority"},
              }}},
         }},
        {"hints_used",
         {
             {"0:1", 3},
         }},
        {"game_options",
         {
             {"hint_cost", 20},
             {"location_check_points", 5},
             {"hint_points", 77},
         }},
        {"er_hint_data",
         {
             {"1", {{"1002", "Eastern Lobby"}}},
         }},
        {"slot_data",
         {
             {"1", {{"mode", "open"}, {"shuffle", "none"}}},
         }},
        {"locations",
         {
             {"1",
              {
                  {"1002",
                   {
                       {"receiver_slot", 2},
                       {"item_id", 2002},
                       {"location_name", "Eastern Palace - Big Chest"},
                   }},
              }},
         }},
        {"tracker_connected", true},
        {"emu_connected", true},
    };

    auto session = import_multiserver_semantics(
        multiserver_state,
        MultiServerImportOptions{
            .room_id = "room-import-1",
            .room_type = RoomType::Async,
            .team_id = 0,
            .slot_id = 1,
            .seed_id = "seed-123",
            .seed_hash = "hash-abc",
            .patch_url = "https://cdn.sekailink.com/patch.apz",
            .tracker_pack = "alttp-pack",
            .tracker_variant = "Map Tracker - AP",
            .sni_required = true,
            .memory_backend_state = "required",
            .expires_at = "2026-04-01T00:00:00Z",
        });

    const auto payload = to_json(session.snapshot());
    require(payload["room_id"] == "room-import-1", "room_id");
    require(payload["room_type"] == "async", "room_type");
    require(payload["slot_alias"] == "Sekai Jade", "slot_alias");
    require(payload["players"].size() == 2, "players");
    require(payload["checked_locations"][0] == 1001, "checked_locations");
    require(payload["received_items"][0]["item_name"] == "Bow", "received_items");
    require(payload["stored_data"]["lttp_big_key_eastern"] == true, "stored_data");
    require(payload["hints"].size() == 1, "hints");
    require(payload["hints_used"] == 3, "hints_used");
    require(payload["game_options"]["hint_cost"] == 20, "game_options");
    require(payload["hint_points"] == 77, "hint_points");
    require(payload["er_hint_data"]["1002"] == "Eastern Lobby", "er_hint_data");
    require(payload["location_names"][0][1] == "Eastern Palace - Big Chest", "location_name");
    require(payload["tracker_connected"] == true, "tracker_connected");
    require(payload["emu_connected"] == true, "emu_connected");

    std::cout << payload.dump(2) << "\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "sekailink_multiserver_semantics_smoke failed: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
