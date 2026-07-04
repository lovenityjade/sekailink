#include "sekailink_server/room_seed_package_dispatch.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
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

    const auto root = std::filesystem::temp_directory_path() / "sekailink_room_seed_package_dispatch_smoke";
    std::filesystem::remove_all(root);
    const auto package_dir = root / "seed-soh-cycle1";
    write_json(package_dir / "link_room_seed_contract.json", seed_contract_fixture());

    const RoomSeedPackageDispatchRequest dispatch_request{
        .package_dir = package_dir,
        .room_id = "room-linkedworld-soh-1",
    };
    const auto envelope = build_seed_package_dispatch_envelope(dispatch_request);
    require(envelope.at("channel") == "admin", "envelope_channel");
    require(envelope.at("command").at("cmd") == "apply_seed_contract", "envelope_cmd");
    require(envelope.at("command").at("seed_contract").at("seed_id") == "seed-soh-cycle1",
            "envelope_seed_contract");

    RoomRegistry registry;
    RoomAuditStore audit_store(root / "audit");
    RoomProjectionSpool projection_spool(root / "projection");
    require(registry.create_room({
                .room_id = "room-linkedworld-soh-1",
                .room_type = RoomType::Live,
                .game = "Ship of Harkinian",
                .slot_id = 2,
                .slot_name = "Jade SoH",
                .slot_alias = "Jade SoH",
            }),
            "create_room");

    const auto response = dispatch_seed_package_to_room(
        registry,
        &audit_store,
        &projection_spool,
        nullptr,
        dispatch_request);
    require(response.at("ok") == true, "dispatch_response");
    require(response.at("seed_contract_summary").at("seed_id") == "seed-soh-cycle1",
            "dispatch_seed_summary");
    require(response.at("slot_data").at("starting_age") == "child", "dispatch_slot_data");

    const auto snapshot = registry.find_room("room-linkedworld-soh-1")->snapshot();
    require(snapshot.seed_id == std::optional<std::string>("seed-soh-cycle1"), "snapshot_seed_id");
    require(snapshot.slot_data.at("rainbow_bridge") == "greg", "snapshot_slot_data");
    require(snapshot.seed_contract.at("slots").size() == 1, "snapshot_seed_contract");

    const auto records = projection_spool.read_room_records();
    require(!records.empty(), "projection_records");
    require(records.back().at("seed_contract_applied") == true, "projection_seed_contract_applied");

    std::cout << response.dump(2) << "\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_seed_package_dispatch_smoke failed: " << exception.what() << "\n";
    return EXIT_FAILURE;
  }
}
