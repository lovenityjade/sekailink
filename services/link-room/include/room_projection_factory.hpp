#pragma once

#include "sekailink_server/room_projection.hpp"
#include "sekailink_server/room_projection_mysql.hpp"
#include "sekailink_server/room_projection_sqlite.hpp"

#include <filesystem>
#include <memory>
#include <string>

namespace sekailink_server {

enum class ProjectionBackend {
  Jsonl,
  Sqlite,
  Mysql,
};

ProjectionBackend parse_projection_backend(const std::string& value);
std::string projection_backend_name(ProjectionBackend backend);
std::unique_ptr<RoomProjectionStore> make_projection_store(
    ProjectionBackend backend,
    const std::filesystem::path& target);

}  // namespace sekailink_server
