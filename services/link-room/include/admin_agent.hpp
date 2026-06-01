#pragma once

#include "nlohmann/json.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace sekailink_server {

struct AdminAgentServiceDescriptor {
  std::string name;
  std::filesystem::path state_file;
  std::filesystem::path log_file;
  std::optional<std::string> systemd_unit;
};

struct AdminAgentConfig {
  std::uint16_t http_port = 19091;
  std::string listen_host = "127.0.0.1";
  std::optional<std::string> admin_token;
  std::string systemctl_path = "/usr/bin/systemctl";
  bool systemctl_use_sudo = false;
  std::vector<AdminAgentServiceDescriptor> services;
};

AdminAgentConfig load_admin_agent_config(const std::filesystem::path& path);
nlohmann::json to_json(const AdminAgentServiceDescriptor& descriptor);
nlohmann::json to_json(const AdminAgentConfig& config);

class AdminAgentService {
 public:
  explicit AdminAgentService(AdminAgentConfig config);

  [[nodiscard]] nlohmann::json handle_get(
      const std::string& path,
      const std::optional<std::string>& bearer_token) const;
  [[nodiscard]] nlohmann::json handle_post(
      const std::string& path,
      const std::optional<std::string>& bearer_token) const;

 private:
  [[nodiscard]] bool authorized(const std::optional<std::string>& bearer_token) const;
  [[nodiscard]] const AdminAgentServiceDescriptor* find_service(const std::string& name) const;
  [[nodiscard]] nlohmann::json system_state() const;
  [[nodiscard]] nlohmann::json control_service(const AdminAgentServiceDescriptor& service, const std::string& action) const;

  AdminAgentConfig config_;
};

class AdminAgentHttpServer {
 public:
  explicit AdminAgentHttpServer(AdminAgentConfig config);
  ~AdminAgentHttpServer();

  AdminAgentHttpServer(const AdminAgentHttpServer&) = delete;
  AdminAgentHttpServer& operator=(const AdminAgentHttpServer&) = delete;

  [[nodiscard]] bool start();
  void stop();
  [[nodiscard]] std::uint16_t port() const;
  void serve_one() const;

 private:
  int listen_fd_ = -1;
  std::uint16_t bound_port_ = 0;
  AdminAgentService service_;
  AdminAgentConfig config_;
};

}  // namespace sekailink_server
