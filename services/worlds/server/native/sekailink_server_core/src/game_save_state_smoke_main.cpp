#include "sekailink_server/game_save_state.hpp"

#include <cstdlib>
#include <filesystem>
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
    GameSaveState state;
    state.world_id = "alttp-world-1";
    state.seed_id = "seed-123";
    state.checked_locations_by_slot[1] = {1001, 1002};
    state.delivered_items.push_back(DeliveredItemRecord{
        .delivery_id = 1,
        .receiver_slot = 2,
        .item_id = 2001,
        .item_name = "Bow",
        .source_location_id = 1001,
        .sender_slot = 1,
        .acknowledged = true,
    });
    state.next_delivery_id = 2;

    const auto temp_dir = std::filesystem::temp_directory_path() / "sekailink-game-save-smoke";
    std::filesystem::remove_all(temp_dir);
    std::filesystem::create_directories(temp_dir);
    const auto save_path = temp_dir / "save.json";

    std::string error;
    require(save_game_save_state(state, save_path, &error), "save_state");
    const auto loaded = load_game_save_state(save_path, &error);
    require(loaded.has_value(), "load_state");
    require(loaded->checked_locations_by_slot.at(1).size() == 2, "checked_locations");
    require(loaded->delivered_items.front().acknowledged, "delivery_acknowledged");

    GameSaveState invalid = state;
    invalid.world_id.clear();
    require(!save_game_save_state(invalid, temp_dir / "invalid.json", &error), "invalid_save_rejected");

    std::cout << "sekailink_game_save_state_smoke_ok\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "sekailink_game_save_state_smoke failed: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
