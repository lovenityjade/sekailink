#include "sekailink_server/room_seed_package_dispatch.hpp"
#include "sekailink_server/room_server_tcp.hpp"

#include <fstream>
#include <stdexcept>

namespace sekailink_server {

namespace {

nlohmann::json read_json_file(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    throw std::runtime_error("seed_contract_open_failed:" + path.string());
  }
  nlohmann::json value;
  stream >> value;
  return value;
}

}  // namespace

nlohmann::json load_link_room_seed_contract(const std::filesystem::path& package_dir) {
  const auto contract_path = package_dir / "link_room_seed_contract.json";
  auto contract = read_json_file(contract_path);
  if (!contract.is_object() ||
      contract.value("schema_version", std::string{}) != "sekailink-link-room-seed-contract-v1") {
    throw std::runtime_error("seed_contract_schema_mismatch:" + contract_path.string());
  }
  return contract;
}

nlohmann::json build_apply_seed_contract_command(
    const std::string& room_id,
    const nlohmann::json& seed_contract) {
  return {
      {"cmd", "apply_seed_contract"},
      {"room_id", room_id},
      {"seed_contract", seed_contract},
  };
}

nlohmann::json build_seed_package_dispatch_envelope(const RoomSeedPackageDispatchRequest& request) {
  nlohmann::json envelope = {
      {"channel", "admin"},
      {"command", build_apply_seed_contract_command(request.room_id, load_link_room_seed_contract(request.package_dir))},
  };
  if (request.auth_token.has_value()) {
    envelope["auth_token"] = *request.auth_token;
  }
  return envelope;
}

nlohmann::json dispatch_seed_package_to_room(
    RoomRegistry& registry,
    RoomAuditStore* audit_store,
    RoomProjectionStore* projection_store,
    const RoomServerAuthPolicy* auth_policy,
    const RoomSeedPackageDispatchRequest& request) {
  return handle_protocol_json(
      registry,
      audit_store,
      projection_store,
      auth_policy,
      build_seed_package_dispatch_envelope(request));
}

std::string dispatch_seed_package_to_room_tcp(
    const RoomSeedPackageDispatchRequest& request,
    const std::string& host,
    std::uint16_t port) {
  return tcp_send_json_line(host, port, build_seed_package_dispatch_envelope(request));
}

}  // namespace sekailink_server
