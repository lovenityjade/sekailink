#include "sekailink_server/room_projection_factory.hpp"

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
    const auto root = std::filesystem::temp_directory_path() / "sekailink_room_projection_factory_smoke";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    auto jsonl_store = make_projection_store(ProjectionBackend::Jsonl, root / "jsonl");
    auto sqlite_store = make_projection_store(ProjectionBackend::Sqlite, root / "projection.sqlite3");

    require(jsonl_store != nullptr, "jsonl_store");
    require(sqlite_store != nullptr, "sqlite_store");
    require(parse_projection_backend("jsonl") == ProjectionBackend::Jsonl, "parse_jsonl");
    require(parse_projection_backend("sqlite") == ProjectionBackend::Sqlite, "parse_sqlite");

    std::cout << "factory_ok=1\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& e) {
    std::cerr << "sekailink_room_projection_factory_smoke failed: " << e.what() << "\n";
    return EXIT_FAILURE;
  }
}
