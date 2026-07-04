#include "sekailink_server/room_projection_factory.hpp"

#include <cstdlib>
#include <cstdint>
#include <stdexcept>

namespace sekailink_server {

ProjectionBackend parse_projection_backend(const std::string& value) {
  if (value == "jsonl") {
    return ProjectionBackend::Jsonl;
  }
  if (value == "sqlite") {
    return ProjectionBackend::Sqlite;
  }
  if (value == "mysql") {
    return ProjectionBackend::Mysql;
  }
  throw std::runtime_error("invalid_projection_backend");
}

std::string projection_backend_name(ProjectionBackend backend) {
  switch (backend) {
    case ProjectionBackend::Jsonl:
      return "jsonl";
    case ProjectionBackend::Sqlite:
      return "sqlite";
    case ProjectionBackend::Mysql:
      return "mysql";
  }
  throw std::runtime_error("invalid_projection_backend");
}

std::unique_ptr<RoomProjectionStore> make_projection_store(
    ProjectionBackend backend,
    const std::filesystem::path& target) {
  switch (backend) {
    case ProjectionBackend::Jsonl:
      return std::make_unique<RoomProjectionSpool>(target);
    case ProjectionBackend::Sqlite:
      return std::make_unique<RoomProjectionSqliteStore>(target);
    case ProjectionBackend::Mysql:
      return std::make_unique<RoomProjectionMysqlStore>(MysqlConnectionConfig{
          .host = std::getenv("SEKAILINK_MYSQL_HOST") != nullptr ? std::getenv("SEKAILINK_MYSQL_HOST") : "127.0.0.1",
          .user = std::getenv("SEKAILINK_MYSQL_USER") != nullptr ? std::getenv("SEKAILINK_MYSQL_USER") : "",
          .password = std::getenv("SEKAILINK_MYSQL_PASSWORD") != nullptr ? std::getenv("SEKAILINK_MYSQL_PASSWORD") : "",
          .database = target.string(),
          .port = std::getenv("SEKAILINK_MYSQL_PORT") != nullptr
                      ? static_cast<std::uint32_t>(std::stoul(std::getenv("SEKAILINK_MYSQL_PORT")))
                      : 3306,
          .unix_socket = std::getenv("SEKAILINK_MYSQL_SOCKET") != nullptr ? std::getenv("SEKAILINK_MYSQL_SOCKET") : "",
      });
  }
  throw std::runtime_error("invalid_projection_backend");
}

}  // namespace sekailink_server
