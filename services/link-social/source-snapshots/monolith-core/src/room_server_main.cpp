#include "sekailink_server/room_registry.hpp"
#include "sekailink_server/room_audit_store.hpp"
#include "sekailink_server/room_projection_factory.hpp"
#include "sekailink_server/room_projection.hpp"
#include "sekailink_server/room_projection_restore.hpp"
#include "sekailink_server/room_server_protocol.hpp"
#include "sekailink_server/room_restore.hpp"
#include "sekailink_server/room_retention.hpp"

#include <filesystem>
#include <cstdlib>
#include <iostream>
#include <string>

using namespace sekailink_server;

int main(int argc, char** argv) {
  RoomRegistry registry;
  std::filesystem::path audit_root;
  std::filesystem::path projection_root;
  RoomAuditStore* audit_store_ptr = nullptr;
  RoomProjectionStore* projection_store_ptr = nullptr;
  std::optional<RoomAuditStore> audit_store;
  std::unique_ptr<RoomProjectionStore> projection_store;
  RoomServerAuthPolicy auth_policy;
  auto projection_backend = ProjectionBackend::Jsonl;
  bool restore_from_audit = false;
  bool restore_from_projection = false;
  bool purge_expired_before_each_command = false;

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];
    if (arg == "--audit-root" && i + 1 < argc) {
      audit_root = argv[++i];
    } else if (arg == "--projection-root" && i + 1 < argc) {
      projection_root = argv[++i];
    } else if (arg == "--projection-backend" && i + 1 < argc) {
      projection_backend = parse_projection_backend(argv[++i]);
    } else if (arg == "--restore-from-audit") {
      restore_from_audit = true;
    } else if (arg == "--restore-from-projection") {
      restore_from_projection = true;
    } else if (arg == "--purge-expired-before-each-command") {
      purge_expired_before_each_command = true;
    }
  }
  if (!audit_root.empty()) {
    audit_store.emplace(audit_root);
    audit_store_ptr = &(*audit_store);
  }
  if (!projection_root.empty()) {
    projection_store = make_projection_store(projection_backend, projection_root);
    projection_store_ptr = projection_store.get();
  }
  if (const auto* token = std::getenv("SEKAILINK_ROOM_SERVER_ADMIN_TOKEN"); token != nullptr && *token != '\0') {
    auth_policy.admin_token = token;
  }
  if (const auto* token = std::getenv("SEKAILINK_ROOM_SERVER_RUNTIME_TOKEN"); token != nullptr && *token != '\0') {
    auth_policy.runtime_token = token;
  }
  if (const auto* token = std::getenv("SEKAILINK_ROOM_SERVER_CLIENT_REPORT_TOKEN"); token != nullptr && *token != '\0') {
    auth_policy.client_report_token = token;
  }
  if (restore_from_audit && audit_store_ptr != nullptr) {
    restore_all_rooms_from_audit(registry, *audit_store_ptr);
  }
  if (restore_from_projection && projection_store != nullptr) {
    if (auto* sqlite_store = dynamic_cast<RoomProjectionSqliteStore*>(projection_store.get()); sqlite_store != nullptr) {
      restore_rooms_from_projection_store(registry, *sqlite_store);
    } else if (auto* mysql_store = dynamic_cast<RoomProjectionMysqlStore*>(projection_store.get()); mysql_store != nullptr) {
      restore_rooms_from_projection_store(registry, *mysql_store);
    }
  }

  std::string line;
  while (std::getline(std::cin, line)) {
    if (line.empty()) {
      continue;
    }
    try {
      if (purge_expired_before_each_command) {
        purge_expired_rooms(registry, audit_store_ptr, utc_timestamp_now());
      }
      const auto command = nlohmann::json::parse(line);
      const auto response = handle_protocol_json(
          registry,
          audit_store_ptr,
          projection_store_ptr,
          &auth_policy,
          {
              {"channel", "admin"},
              {"auth_token", auth_policy.admin_token.has_value() ? nlohmann::json(*auth_policy.admin_token) : nlohmann::json(nullptr)},
              {"command", command},
          });
      std::cout << response.dump() << std::endl;
    } catch (const std::exception& e) {
      std::cout << nlohmann::json{{"ok", false}, {"error", e.what()}}.dump() << std::endl;
    }
  }
  return EXIT_SUCCESS;
}
