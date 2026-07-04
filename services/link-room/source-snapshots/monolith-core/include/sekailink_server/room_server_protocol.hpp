#pragma once

#include "sekailink_server/room_audit_store.hpp"
#include "sekailink_server/room_projection.hpp"
#include "sekailink_server/room_projection_store.hpp"
#include "sekailink_server/room_registry.hpp"

#include <optional>
#include <string>
#include <unordered_set>

namespace sekailink_server {

enum class ProtocolChannel {
  Admin,
  Runtime,
  ClientReport,
};

struct ProtocolEnvelope {
  ProtocolChannel channel = ProtocolChannel::Runtime;
  std::optional<std::string> auth_token;
  nlohmann::json command = nlohmann::json::object();
};

struct RoomServerAuthPolicy {
  std::optional<std::string> admin_token;
  std::optional<std::string> runtime_token;
  std::optional<std::string> client_report_token;

  [[nodiscard]] bool requires_auth(ProtocolChannel channel) const;
  [[nodiscard]] bool token_valid(ProtocolChannel channel, const std::optional<std::string>& presented_token) const;
};

class RoomServerProtocolService {
 public:
  explicit RoomServerProtocolService(
      RoomRegistry& registry,
      RoomAuditStore* audit_store = nullptr,
      RoomProjectionStore* projection_store = nullptr,
      const RoomServerAuthPolicy* auth_policy = nullptr);

  [[nodiscard]] nlohmann::json handle(const ProtocolEnvelope& envelope) const;

 private:
  [[nodiscard]] static std::string channel_to_string(ProtocolChannel channel);
  [[nodiscard]] static bool command_allowed(ProtocolChannel channel, const std::string& cmd);
  [[nodiscard]] static ProtocolEnvelope parse(const nlohmann::json& envelope);

  RoomRegistry& registry_;
  RoomAuditStore* audit_store_;
  RoomProjectionStore* projection_store_;
  const RoomServerAuthPolicy* auth_policy_;
};

ProtocolEnvelope protocol_envelope_from_json(const nlohmann::json& envelope);
nlohmann::json handle_protocol_json(
    RoomRegistry& registry,
    RoomAuditStore* audit_store,
    RoomProjectionStore* projection_store,
    const RoomServerAuthPolicy* auth_policy,
    const nlohmann::json& envelope_json);

}  // namespace sekailink_server
