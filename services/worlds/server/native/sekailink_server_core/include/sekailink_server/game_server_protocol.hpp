#pragma once

#include "sekailink_server/game_session.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <nlohmann/json.hpp>

namespace sekailink_server {

enum class GameProtocolChannel {
  Admin,
  Core,
  Runtime,
};

struct GameProtocolEnvelope {
  GameProtocolChannel channel = GameProtocolChannel::Runtime;
  std::optional<std::string> auth_token;
  nlohmann::json command = nlohmann::json::object();
};

struct GameServerAuthPolicy {
  std::optional<std::string> admin_token;
  std::optional<std::string> core_token;
  std::optional<std::string> runtime_token;

  [[nodiscard]] bool requires_auth(GameProtocolChannel channel) const;
  [[nodiscard]] bool token_valid(GameProtocolChannel channel,
                                 const std::optional<std::string>& presented_token) const;
};

class GameSessionRegistry {
 public:
  bool insert_session(std::string session_name, GameSession session, std::string* error = nullptr);
  bool create_session_from_world_package(const std::string& session_name,
                                         const std::filesystem::path& world_package_path,
                                         std::string* error = nullptr);
  bool create_session_from_archipelago_import(const std::string& session_name,
                                              const nlohmann::json& multiserver_state,
                                              const ArchipelagoWorldImportOptions& options,
                                              std::string* error = nullptr);
  bool restore_session_from_paths(const std::string& session_name,
                                  const std::filesystem::path& world_package_path,
                                  const std::filesystem::path& save_state_path,
                                  std::string* error = nullptr);
  bool save_session_state(const std::string& session_name,
                          const std::filesystem::path& save_state_path,
                          std::string* error = nullptr) const;
  bool remove_session(const std::string& session_name);
  [[nodiscard]] bool has_session(const std::string& session_name) const;
  GameSession* find_session(const std::string& session_name);
  const GameSession* find_session(const std::string& session_name) const;
  [[nodiscard]] std::vector<std::string> list_session_names() const;

 private:
  std::unordered_map<std::string, GameSession> sessions_;
};

class GameServerProtocolService {
 public:
  explicit GameServerProtocolService(GameSessionRegistry& registry,
                                     const GameServerAuthPolicy* auth_policy = nullptr);

  [[nodiscard]] nlohmann::json handle(const GameProtocolEnvelope& envelope) const;

 private:
  [[nodiscard]] static std::string channel_to_string(GameProtocolChannel channel);
  [[nodiscard]] static bool command_allowed(GameProtocolChannel channel, const std::string& cmd);
  [[nodiscard]] static GameProtocolEnvelope parse(const nlohmann::json& envelope);
  [[nodiscard]] static nlohmann::json projected_rooms_to_json(const std::string& session_name,
                                                              const GameSession& session);

  [[nodiscard]] nlohmann::json handle_admin_command(const nlohmann::json& command) const;
  [[nodiscard]] nlohmann::json handle_core_command(const nlohmann::json& command) const;
  [[nodiscard]] nlohmann::json handle_runtime_command(const nlohmann::json& command) const;
  [[nodiscard]] nlohmann::json session_summary_json(const std::string& session_name,
                                                    const GameSession& session) const;
  [[nodiscard]] static nlohmann::json ticket_to_json(const SessionTicket& ticket);

  GameSessionRegistry& registry_;
  const GameServerAuthPolicy* auth_policy_;
};

GameProtocolEnvelope game_protocol_envelope_from_json(const nlohmann::json& envelope);
nlohmann::json handle_game_protocol_json(GameSessionRegistry& registry,
                                         const GameServerAuthPolicy* auth_policy,
                                         const nlohmann::json& envelope_json);

}  // namespace sekailink_server
