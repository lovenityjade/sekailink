#pragma once

#include "sekailink_server/generation_service.hpp"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace sekailink_server {

struct GenerationServerConfig {
  std::uint16_t tcp_port = 0;
  std::optional<std::string> auth_token;
  GenerationServiceConfig service_config;
  std::filesystem::path state_path;
};

GenerationServerConfig load_generation_server_config(const std::filesystem::path& path);

nlohmann::json handle_generation_command(
    GenerationService& service,
    const nlohmann::json& command,
    const std::optional<std::string>& auth_token);

class GenerationTcpServer {
 public:
  explicit GenerationTcpServer(GenerationServerConfig config);
  ~GenerationTcpServer();

  GenerationTcpServer(const GenerationTcpServer&) = delete;
  GenerationTcpServer& operator=(const GenerationTcpServer&) = delete;

  bool start();
  void stop();
  [[nodiscard]] std::uint16_t port() const;
  [[nodiscard]] bool running() const;

 private:
  bool serve_one();
  bool serve_forever();
  void record_request() const;
  void record_error() const;
  void write_state_file() const;

  GenerationServerConfig config_;
  GenerationService service_;
  int listen_fd_ = -1;
  std::uint16_t bound_port_ = 0;
  std::atomic<bool> running_{false};
  std::thread server_thread_;
  mutable std::mutex state_mutex_;
  mutable std::uint64_t total_requests_ = 0;
  mutable std::uint64_t total_errors_ = 0;
};

std::string generation_tcp_send_json_line(const std::string& host, std::uint16_t port, const nlohmann::json& payload);

}  // namespace sekailink_server
