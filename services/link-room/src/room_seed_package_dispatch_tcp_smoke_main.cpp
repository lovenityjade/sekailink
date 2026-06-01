#include "sekailink_server/room_seed_package_dispatch.hpp"
#include "sekailink_server/room_server_tcp.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

void write_json(const std::filesystem::path& path, const nlohmann::json& value) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream stream(path, std::ios::binary | std::ios::trunc);
  if (!stream) {
    throw std::runtime_error("write_json_failed:" + path.string());
  }
  stream << value.dump(2) << "\n";
}

nlohmann::json seed_contract_fixture() {
  const nlohmann::json config_snapshot = {
      {"schema_version", "sekailink-config-snapshot-v1"},
      {"game_key", "oot_soh"},
      {"source_game_key", "ship_of_harkinian"},
      {"linkedworld_id", "oot_soh"},
      {"config_name", "Jade-SoH"},
      {"values",
       {
           {"starting_age", "child"},
           {"rainbow_bridge", "greg"},
           {"shuffle_songs", "anywhere"},
       }},
  };
  return {
      {"schema_version", "sekailink-link-room-seed-contract-v1"},
      {"generation_scope", "multiworld"},
      {"room_id", "room-seed-soh-cycle1"},
      {"seed_id", "seed-soh-cycle1"},
      {"checks_ref", "checks.json"},
      {"items_ref", "items.json"},
      {"placements_ref", "placements.json"},
      {"slots",
       nlohmann::json::array({
           {
               {"slot_id", 2},
               {"user_id", 1001},
               {"display_name", "Jade SoH"},
               {"game_key", "oot_soh"},
               {"linkedworld_id", "oot_soh"},
               {"config_version_id", 7001},
               {"config_snapshot", config_snapshot},
           },
       })},
      {"config_versions",
       nlohmann::json::array({
           {
               {"slot_id", 2},
               {"user_id", 1001},
               {"game_key", "oot_soh"},
               {"linkedworld_id", "oot_soh"},
               {"config_version_id", 7001},
               {"source", "nexus.config_version_id"},
               {"config_snapshot", config_snapshot},
           },
       })},
  };
}

}  // namespace

int main() {
  try {
    using namespace sekailink_server;

    const auto root = std::filesystem::temp_directory_path() / "sekailink_room_seed_package_dispatch_tcp_smoke";
    std::filesystem::remove_all(root);
    const auto package_dir = root / "seed-soh-cycle1";
    write_json(package_dir / "link_room_seed_contract.json", seed_contract_fixture());

    RoomRegistry registry;
    RoomAuditStore audit_store(root / "audit");
    RoomProjectionSpool projection_spool(root / "projection");
    RoomServerTcpService service(registry, &audit_store, &projection_spool);
    require(service.start_background(0), "service_start");
    const auto port = service.port();
    require(port != 0, "bound_port");

    auto response = nlohmann::json::parse(tcp_send_json_line(
        "127.0.0.1",
        port,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "create_room"},
                 {"room_id", "room-linkedworld-soh-1"},
                 {"room_type", "live"},
                 {"game", "Ship of Harkinian"},
                 {"team_id", 0},
                 {"slot_id", 2},
                 {"slot_name", "Jade SoH"},
                 {"slot_alias", "Jade SoH"},
             }},
        }));
    require(response.at("ok") == true, "create_room");

    response = nlohmann::json::parse(dispatch_seed_package_to_room_tcp(
        {
            .package_dir = package_dir,
            .room_id = "room-linkedworld-soh-1",
        },
        "127.0.0.1",
        port));
    require(response.at("ok") == true, "dispatch_response");
    require(response.at("seed_contract_summary").at("applied") == true, "dispatch_seed_applied");
    require(response.at("seed_contract_summary").at("seed_id") == "seed-soh-cycle1",
            "dispatch_seed_summary");
    require(response.at("slot_data").at("starting_age") == "child", "dispatch_slot_data_starting_age");
    require(response.at("slot_data").at("rainbow_bridge") == "greg", "dispatch_slot_data_rainbow_bridge");

    response = nlohmann::json::parse(tcp_send_json_line(
        "127.0.0.1",
        port,
        {
            {"channel", "admin"},
            {"command",
             {
                 {"cmd", "sync_room"},
                 {"room_id", "room-linkedworld-soh-1"},
                 {"from_item_index", 0},
             }},
        }));
    require(response.at("ok") == true, "sync_room");
    require(response.at("sync").at("room").at("seed_contract_summary").at("applied") == true,
            "sync_seed_contract_applied");
    require(response.at("sync").at("room").at("seed_contract_summary").at("seed_id") == "seed-soh-cycle1",
            "sync_seed_contract_seed_id");
    require(response.at("sync").at("room").at("slot_data").at("shuffle_songs") == "anywhere",
            "sync_slot_data");

    service.stop();

    const auto* room = registry.find_room("room-linkedworld-soh-1");
    require(room != nullptr, "room_exists");
    const auto snapshot = room->snapshot();
    require(snapshot.seed_id == std::optional<std::string>("seed-soh-cycle1"), "snapshot_seed_id");
    require(snapshot.seed_contract.at("slots").size() == 1, "snapshot_seed_contract");

    const auto records = projection_spool.read_room_records();
    require(!records.empty(), "projection_records");
    require(records.back().at("seed_contract_applied") == true, "projection_seed_contract_applied");

    std::cout << response.dump(2) << "\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_seed_package_dispatch_tcp_smoke failed: " << exception.what() << "\n";
    return EXIT_FAILURE;
  }
}
