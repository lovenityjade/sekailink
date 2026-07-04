#include "sekailink_server/game_world.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
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
             {"1", {{"name", "Jade"}, {"game", "A Link to the Past"}, {"type", "player"}}},
             {"2", {{"name", "Cloud"}, {"game", "A Link to the Past"}, {"type", "player"}}},
         }},
        {"locations",
         {
             {"1",
              {
                  {"1001",
                   {
                       {"receiver_slot", 2},
                       {"item_id", 2001},
                       {"item_name", "Bow"},
                       {"location_name", "Link's House"},
                       {"flags", 1},
                   }},
              }},
         }},
        {"game_options",
         {
             {"hint_cost", 20},
             {"release_mode", "enabled"},
         }},
    };

    const auto package = import_archipelago_world_package(
        multiserver_state,
        ArchipelagoWorldImportOptions{
            .world_id = "alttp-world-1",
            .world_version = "1.0",
            .seed_id = "seed-123",
            .seed_hash = "hash-abc",
            .linkedworld_id = "alttp",
        });

    require(package.world_id == "alttp-world-1", "world_id");
    require(package.slots.size() == 2, "slots");
    require(package.locations.size() == 1, "locations");
    require(package.locations.at(1001).receiver_slot == 2, "receiver_slot");
    require(package.locations.at(1001).item_name == "Bow", "item_name");
    require(package.server_rules.at("hint_cost") == 20, "server_rules");

    const auto temp_dir = std::filesystem::temp_directory_path() / "sekailink-game-world-smoke";
    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);
    const auto package_path = temp_dir / "world.json";

    std::string error;
    require(save_game_world_package(package, package_path, &error), "save_package");
    const auto loaded = load_game_world_package(package_path, &error);
    require(loaded.has_value(), "load_package");
    require(loaded->locations.at(1001).item_id == 2001, "loaded_location");

    const auto invalid_path = temp_dir / "invalid-world.json";
    {
      std::ofstream output(invalid_path, std::ios::trunc);
      output << R"({
  "world_id": "bad-world",
  "world_version": "1.0",
  "seed_id": "seed-bad",
  "seed_hash": "hash-bad",
  "linkedworld_id": "alttp",
  "server_rules": {},
  "slots": [
    {"slot_id": 1, "name": "Jade", "game": "A Link to the Past"}
  ],
  "locations": [
    {
      "location_id": 1001,
      "owner_slot": 1,
      "receiver_slot": 1,
      "item_id": 2001,
      "item_name": "Bow",
      "location_name": "Link's House",
      "script": "return true;"
    }
  ]
})";
    }
    error.clear();
    const auto invalid = load_game_world_package(invalid_path, &error);
    require(!invalid.has_value(), "invalid_package_rejected");
    require(error == "world_package_contains_forbidden_programming_fields", "invalid_error");

    std::cout << "sekailink_game_world_package_smoke_ok\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "sekailink_game_world_package_smoke failed: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
