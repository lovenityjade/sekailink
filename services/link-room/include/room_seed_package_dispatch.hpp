#pragma once

#include "sekailink_server/room_server_protocol.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace sekailink_server {

struct RoomSeedPackageDispatchRequest {
  std::filesystem::path package_dir;
  std::string room_id;
  std::optional<std::string> auth_token;
};

nlohmann::json load_link_room_seed_contract(const std::filesystem::path& package_dir);
nlohmann::json build_apply_seed_contract_command(
    const std::string& room_id,
    const nlohmann::json& seed_contract);
nlohmann::json build_seed_package_dispatch_envelope(const RoomSeedPackageDispatchRequest& request);
nlohmann::json dispatch_seed_package_to_room(
    RoomRegistry& registry,
    RoomAuditStore* audit_store,
    RoomProjectionStore* projection_store,
    const RoomServerAuthPolicy* auth_policy,
    const RoomSeedPackageDispatchRequest& request);
std::string dispatch_seed_package_to_room_tcp(
    const RoomSeedPackageDispatchRequest& request,
    const std::string& host,
    std::uint16_t port);

}  // namespace sekailink_server
