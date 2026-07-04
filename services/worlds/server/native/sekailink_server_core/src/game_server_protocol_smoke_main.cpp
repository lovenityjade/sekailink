#include "sekailink_server/game_server_protocol.hpp"
#include "sekailink_server/game_world.hpp"

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <stdexcept>

using namespace sekailink_server;

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

}  // namespace

int main() {
  try {
    const auto root = std::filesystem::temp_directory_path() / "sekailink_game_server_protocol_smoke";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    GameSessionRegistry registry;
    GameServerAuthPolicy auth_policy{
        .admin_token = std::string("admin-secret"),
        .core_token = std::string("core-secret"),
        .runtime_token = std::string("runtime-secret"),
    };

    const nlohmann::json imported_ap = {
        {"slot_info",
         {
             {"1", {{"name", "Jade"}, {"game", "EarthBound"}, {"type", "player"}}},
             {"2", {{"name", "Ness"}, {"game", "EarthBound"}, {"type", "player"}}},
         }},
        {"locations",
         {
             {"1",
              {
                  {"1001",
                   {
                       {"receiver_slot", 2},
                       {"item_id", 5001},
                       {"item_name", "Sound Stone"},
                       {"location_name", "Onett Drugstore"},
                       {"flags", 0},
                   }},
              }},
             {"2",
              {
                  {"2001",
                   {
                       {"receiver_slot", 1},
                       {"item_id", 6001},
                       {"item_name", "Mr. Baseball Cap"},
                       {"location_name", "Twoson Burglin Park"},
                       {"flags", 0},
                   }},
              }},
         }},
        {"game_options", {{"release_mode", "enabled"}}},
    };

    auto response = handle_game_protocol_json(
        registry,
        &auth_policy,
        {
            {"channel", "admin"},
            {"command", {{"cmd", "list_sessions"}}},
        });
    require(response.at("ok").get<bool>() == false, "admin_auth_required");
    require(response.at("error").get<std::string>() == "unauthorized", "admin_auth_error");

    response = handle_game_protocol_json(
        registry,
        &auth_policy,
        {
            {"channel", "admin"},
            {"auth_token", "admin-secret"},
            {"command",
             {
                 {"cmd", "create_session_from_ap_import"},
                 {"session_name", "offline-earthbound"},
                 {"world_id", "world-eb-1"},
                 {"world_version", "1.0"},
                 {"seed_id", "seed-eb-1"},
                 {"seed_hash", "ABC123"},
                 {"linkedworld_id", "earthbound-linkedworld"},
                 {"archipelago", imported_ap},
             }},
        });
    require(response.at("ok").get<bool>(), "create_session_from_ap_import");

    response = handle_game_protocol_json(
        registry,
        &auth_policy,
        {
            {"channel", "admin"},
            {"auth_token", "admin-secret"},
            {"command", {{"cmd", "session_summary"}, {"session_name", "offline-earthbound"}}},
        });
    require(response.at("ok").get<bool>(), "session_summary");
    require(response.at("world").at("location_count").get<int>() == 2, "location_count");

    const auto save_path = root / "session-save.json";
    response = handle_game_protocol_json(
        registry,
        &auth_policy,
        {
            {"channel", "core"},
            {"auth_token", "core-secret"},
            {"command",
             {
                 {"cmd", "issue_ticket"},
                 {"session_name", "offline-earthbound"},
                 {"slot_id", 2},
                 {"client_kind", "core"},
             }},
        });
    require(response.at("ok").get<bool>(), "issue_core_ticket");
    const auto core_token = response.at("ticket").at("session_token").get<std::string>();

    response = handle_game_protocol_json(
        registry,
        &auth_policy,
        {
            {"channel", "admin"},
            {"auth_token", "admin-secret"},
            {"command",
             {
                 {"cmd", "issue_ticket"},
                 {"session_name", "offline-earthbound"},
                 {"slot_id", 1},
                 {"client_kind", "runtime"},
                 {"driver_instance_id", "driver-eb-1"},
                 {"linkedworld_id", "earthbound-linkedworld"},
                 {"core_profile", "bsnes"},
             }},
        });
    require(response.at("ok").get<bool>(), "issue_runtime_ticket");
    const auto runtime_token = response.at("ticket").at("session_token").get<std::string>();

    response = handle_game_protocol_json(
        registry,
        &auth_policy,
        {
            {"channel", "runtime"},
            {"auth_token", "runtime-secret"},
            {"command",
             {
                 {"cmd", "runtime_event"},
                 {"session_name", "offline-earthbound"},
                 {"session_token", runtime_token},
                 {"slot_id", 1},
                 {"driver_instance_id", "driver-eb-1"},
                 {"linkedworld_id", "earthbound-linkedworld"},
                 {"core_profile", "bsnes"},
                 {"event_type", "location_checked"},
                 {"canonical_id", 1001},
             }},
        });
    require(response.at("accepted").get<bool>(), "runtime_event_accepted");
    require(response.at("created_delivery_ids").size() == 1, "delivery_created");
    const auto delivery_id = response.at("created_delivery_ids").at(0).get<int>();

    response = handle_game_protocol_json(
        registry,
        &auth_policy,
        {
            {"channel", "runtime"},
            {"auth_token", "runtime-secret"},
            {"command",
             {
                 {"cmd", "runtime_event"},
                 {"session_name", "offline-earthbound"},
                 {"session_token", runtime_token},
                 {"slot_id", 1},
                 {"driver_instance_id", "driver-eb-1"},
                 {"linkedworld_id", "earthbound-linkedworld"},
                 {"core_profile", "bsnes"},
                 {"event_type", "location_checked"},
                 {"canonical_id", 1001},
             }},
        });
    require(response.at("ok").get<bool>() == false, "runtime_duplicate_rejected");
    require(response.at("duplicate").get<bool>(), "runtime_duplicate_flag");

    response = handle_game_protocol_json(
        registry,
        &auth_policy,
        {
            {"channel", "core"},
            {"auth_token", "core-secret"},
            {"command",
             {
                 {"cmd", "pending_items"},
                 {"session_name", "offline-earthbound"},
                 {"slot_id", 2},
                 {"session_token", core_token},
             }},
        });
    require(response.at("ok").get<bool>(), "pending_items");
    require(response.at("pending_items").size() == 1, "pending_items_size");

    response = handle_game_protocol_json(
        registry,
        &auth_policy,
        {
            {"channel", "core"},
            {"auth_token", "core-secret"},
            {"command",
             {
                 {"cmd", "acknowledge_delivery"},
                 {"session_name", "offline-earthbound"},
                 {"slot_id", 2},
                 {"delivery_id", delivery_id},
                 {"session_token", core_token},
             }},
        });
    require(response.at("ok").get<bool>(), "ack_delivery");

    response = handle_game_protocol_json(
        registry,
        &auth_policy,
        {
            {"channel", "admin"},
            {"auth_token", "admin-secret"},
            {"command",
             {
                 {"cmd", "save_session_state"},
                 {"session_name", "offline-earthbound"},
                 {"save_state_path", save_path.string()},
             }},
        });
    require(response.at("ok").get<bool>(), "save_session_state");

    const auto world_path = root / "world-package.json";
    std::string error;
    require(save_game_world_package(registry.find_session("offline-earthbound")->package(), world_path, &error),
            std::string("save_world_package_failed: ") + error);

    response = handle_game_protocol_json(
        registry,
        &auth_policy,
        {
            {"channel", "admin"},
            {"auth_token", "admin-secret"},
            {"command",
             {
                 {"cmd", "restore_session_from_paths"},
                 {"session_name", "offline-earthbound-restored"},
                 {"world_package_path", world_path.string()},
                 {"save_state_path", save_path.string()},
             }},
        });
    require(response.at("ok").get<bool>(), "restore_session");

    response = handle_game_protocol_json(
        registry,
        &auth_policy,
        {
            {"channel", "admin"},
            {"auth_token", "admin-secret"},
            {"command", {{"cmd", "list_sessions"}}},
        });
    require(response.at("ok").get<bool>(), "list_sessions");
    require(response.at("sessions").size() == 2, "list_sessions_size");

    response = handle_game_protocol_json(
        registry,
        &auth_policy,
        {
            {"channel", "admin"},
            {"auth_token", "admin-secret"},
            {"command", {{"cmd", "project_session_rooms"}, {"session_name", "offline-earthbound"}}},
        });
    require(response.at("ok").get<bool>(), "project_session_rooms");
    require(response.at("rooms").size() == 2, "project_session_rooms_size");

    std::cout << "sekailink_game_server_protocol_smoke_ok\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_game_server_protocol_smoke failed: " << exception.what() << "\n";
    return EXIT_FAILURE;
  }
}
