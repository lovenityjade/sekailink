#include "sekailink_server/generic_generation.hpp"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

void print_usage(const char* argv0) {
  std::cerr
      << "usage: " << argv0
      << " <linkedworld_root> <output_root> [seed_id] [rng_seed] [slot_id] [user_id] [display_name] [config_version_id] [config_snapshot_json]\n";
}

int parse_int(const char* value, const char* field) {
  try {
    return std::stoi(value);
  } catch (const std::exception&) {
    throw std::runtime_error(std::string{"invalid_"} + field);
  }
}

std::int64_t parse_i64(const char* value, const char* field) {
  try {
    return std::stoll(value);
  } catch (const std::exception&) {
    throw std::runtime_error(std::string{"invalid_"} + field);
  }
}

nlohmann::json read_json_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("config_snapshot_open_failed");
  }
  nlohmann::json value;
  stream >> value;
  return value;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3 || argc > 10) {
    print_usage(argv[0]);
    return 2;
  }

  try {
    const std::filesystem::path linkedworld_root = argv[1];
    const std::filesystem::path output_root = argv[2];
    const std::string seed_id = argc >= 4 ? argv[3] : "seed-cli";
    const std::string rng_seed = argc >= 5 ? argv[4] : seed_id;
    const int slot_id = argc >= 6 ? parse_int(argv[5], "slot_id") : 1;
    const auto user_id = argc >= 7 ? parse_i64(argv[6], "user_id") : 1001;
    const std::string display_name = argc >= 8 ? argv[7] : "Jade";
    const auto config_version_id = argc >= 9 ? parse_i64(argv[8], "config_version_id") : 1;
    const auto config_snapshot = argc >= 10 ? read_json_file(argv[9]) : nlohmann::json::object();

    std::string error;
    const auto surface =
        sekailink_server::load_linkedworld_generation_surface(linkedworld_root, &error);
    if (!surface.has_value()) {
      std::cerr << "load_linkedworld_failed: " << error << "\n";
      return 3;
    }

    sekailink_server::GenerationPackageRequest request;
    request.job_id = "job-" + seed_id;
    request.room_id = "room-" + seed_id;
    request.seed_id = seed_id;
    request.rng_seed = rng_seed;
    request.output_root = output_root;
    request.slots.push_back({
        .slot_id = slot_id,
        .user_id = user_id,
        .display_name = display_name,
        .game_key = surface->game_key,
        .linkedworld_id = surface->linkedworld_id,
        .config_version_id = config_version_id,
        .config_snapshot = config_snapshot,
    });

    const auto result = sekailink_server::generate_seed_package_from_linkedworlds(request, {*surface});
    if (!result.ok) {
      std::cerr << "generate_seed_package_failed: " << result.error << "\n";
      return 4;
    }

    std::cout << "package_dir=\"" << result.package_dir.string() << "\"\n";
    std::cout << "package_hash=" << result.package_hash << "\n";
    std::cout << "slot_count=" << result.manifest.value("slot_count", 0) << "\n";
    std::cout << "linkedworld_count=" << result.manifest.value("linkedworld_count", 0) << "\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << "generic_generation_package_error: " << ex.what() << "\n";
    return 1;
  }
}
