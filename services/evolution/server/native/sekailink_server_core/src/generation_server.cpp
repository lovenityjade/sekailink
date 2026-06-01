#include "sekailink_server/generation_server.hpp"
#include "sekailink_server/room_state.hpp"

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
using socket_len_t = int;
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_len_t = socklen_t;
#endif

#include <algorithm>
#include <fstream>
#include <stdexcept>

namespace sekailink_server {

namespace {

std::optional<GenerationJobSortField> parse_generation_sort_field(const nlohmann::json& command) {
  if (!command.contains("sort_by") || command.at("sort_by").is_null()) {
    return std::nullopt;
  }
  const auto value = command.at("sort_by").get<std::string>();
  if (value == "job_id") {
    return GenerationJobSortField::JobId;
  }
  if (value == "requested_at") {
    return GenerationJobSortField::RequestedAt;
  }
  if (value == "status") {
    return GenerationJobSortField::Status;
  }
  throw std::runtime_error("invalid_generation_sort_by");
}

void close_socket_fd(int fd) {
  if (fd < 0) {
    return;
  }
#if defined(_WIN32)
  closesocket(fd);
#else
  close(fd);
#endif
}

void shutdown_socket_fd(int fd) {
  if (fd < 0) {
    return;
  }
#if defined(_WIN32)
  shutdown(fd, SD_BOTH);
#else
  shutdown(fd, SHUT_RDWR);
#endif
}

}  // namespace

GenerationServerConfig load_generation_server_config(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("generation_server_config_open_failed");
  }
  nlohmann::json json;
  stream >> json;

  GenerationServerConfig config;
  if (json.contains("tcp_port")) {
    config.tcp_port = json.at("tcp_port").get<std::uint16_t>();
  }
  if (json.contains("auth_token") && !json.at("auth_token").is_null()) {
    config.auth_token = json.at("auth_token").get<std::string>();
  }
  if (json.contains("command_template") && json.at("command_template").is_array()) {
    config.service_config.command_template = json.at("command_template").get<std::vector<std::string>>();
  }
  if (json.contains("state_path") && !json.at("state_path").is_null()) {
    config.state_path = json.at("state_path").get<std::string>();
  }
  return config;
}

nlohmann::json handle_generation_command(
    GenerationService& service,
    const nlohmann::json& command,
    const std::optional<std::string>& auth_token) {
  if (auth_token.has_value()) {
    const auto provided = command.value("auth_token", std::string());
    if (provided != *auth_token) {
      return {
          {"ok", false},
          {"error", "unauthorized"},
      };
    }
  }

  const auto cmd = command.at("cmd").get<std::string>();
  if (cmd == "submit_job") {
    GenerationJob job{
        .job_id = command.at("job_id").get<std::string>(),
        .requested_at = utc_timestamp_now(),
        .yaml_path = command.at("yaml_path").get<std::string>(),
        .output_dir = command.at("output_dir").get<std::string>(),
        .expected_artifact = command.contains("expected_artifact") && !command.at("expected_artifact").is_null()
                                 ? std::optional<std::filesystem::path>(command.at("expected_artifact").get<std::string>())
                                 : std::nullopt,
    };
    return {
        {"ok", service.submit_job(std::move(job))},
    };
  }
  if (cmd == "list_jobs") {
    const auto limit = command.contains("limit")
                           ? std::optional<std::size_t>(static_cast<std::size_t>(std::max(command.at("limit").get<int>(), 0)))
                           : std::nullopt;
    const auto query =
        command.contains("query") && !command.at("query").is_null() ? std::optional<std::string>(command.at("query").get<std::string>())
                                                                     : std::nullopt;
    const auto status =
        command.contains("status") && !command.at("status").is_null() ? std::optional<std::string>(command.at("status").get<std::string>())
                                                                       : std::nullopt;
    const auto requested_after =
        command.contains("requested_after") && !command.at("requested_after").is_null()
            ? std::optional<std::string>(command.at("requested_after").get<std::string>())
            : std::nullopt;
    const auto requested_before =
        command.contains("requested_before") && !command.at("requested_before").is_null()
            ? std::optional<std::string>(command.at("requested_before").get<std::string>())
            : std::nullopt;
    const auto sort_by = parse_generation_sort_field(command).value_or(GenerationJobSortField::JobId);
    const auto descending =
        command.contains("order") && !command.at("order").is_null() && command.at("order").get<std::string>() == "desc";
    const auto offset = command.contains("offset")
                            ? std::optional<std::size_t>(static_cast<std::size_t>(std::max(command.at("offset").get<int>(), 0)))
                            : std::nullopt;
    nlohmann::json jobs = nlohmann::json::array();
    for (const auto& job : service.list_jobs(limit, query, status, requested_after, requested_before, sort_by, descending, offset)) {
      jobs.push_back(to_json(job));
    }
    return {
        {"ok", true},
        {"job_ids", service.list_job_ids(limit, query, status, requested_after, requested_before, sort_by, descending, offset)},
        {"sort_by", generation_job_sort_field_name(sort_by)},
        {"order", descending ? "desc" : "asc"},
        {"jobs", std::move(jobs)},
    };
  }
  if (cmd == "job_status") {
    const auto job = service.find_job(command.at("job_id").get<std::string>());
    if (!job.has_value()) {
      return {
          {"ok", false},
          {"error", "job_not_found"},
      };
    }
    return {
        {"ok", true},
        {"job", to_json(*job)},
    };
  }
  return {
      {"ok", false},
      {"error", "unknown_command"},
  };
}

GenerationTcpServer::GenerationTcpServer(GenerationServerConfig config)
    : config_(std::move(config)), service_(config_.service_config) {}

GenerationTcpServer::~GenerationTcpServer() {
  stop();
}

bool GenerationTcpServer::start() {
  if (running_) {
    return false;
  }
#if defined(_WIN32)
  WSADATA wsa_data;
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    return false;
  }
#endif
  if (!service_.start()) {
    return false;
  }
  listen_fd_ = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
  if (listen_fd_ < 0) {
    service_.stop();
    return false;
  }

  int reuse = 1;
  ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(config_.tcp_port);
  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    stop();
    return false;
  }
  if (::listen(listen_fd_, 8) != 0) {
    stop();
    return false;
  }

  socket_len_t len = sizeof(addr);
  if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len) != 0) {
    stop();
    return false;
  }

  bound_port_ = ntohs(addr.sin_port);
  running_ = true;
  write_state_file();
  server_thread_ = std::thread([this]() {
    serve_forever();
  });
  return true;
}

void GenerationTcpServer::stop() {
  running_ = false;
  shutdown_socket_fd(listen_fd_);
  close_socket_fd(listen_fd_);
  listen_fd_ = -1;
  bound_port_ = 0;
  if (server_thread_.joinable()) {
    server_thread_.join();
  }
  service_.stop();
  write_state_file();
#if defined(_WIN32)
  WSACleanup();
#endif
}

std::uint16_t GenerationTcpServer::port() const {
  return bound_port_;
}

bool GenerationTcpServer::running() const {
  return running_;
}

bool GenerationTcpServer::serve_one() {
  if (!running_ || listen_fd_ < 0) {
    return false;
  }
  sockaddr_in client_addr{};
  socket_len_t client_len = sizeof(client_addr);
  const int client_fd = static_cast<int>(::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len));
  if (client_fd < 0) {
    return false;
  }

  std::string payload;
  char buffer[4096];
  while (true) {
    const auto read_count = ::recv(client_fd, buffer, sizeof(buffer), 0);
    if (read_count <= 0) {
      break;
    }
    payload.append(buffer, static_cast<std::size_t>(read_count));
    if (payload.find('\n') != std::string::npos) {
      break;
    }
  }
  const auto newline = payload.find('\n');
  if (newline != std::string::npos) {
    payload.resize(newline);
  }

  nlohmann::json response;
  try {
    record_request();
    response = handle_generation_command(service_, nlohmann::json::parse(payload), config_.auth_token);
  } catch (const std::exception& exception) {
    record_error();
    response = {
        {"ok", false},
        {"error", exception.what()},
    };
  }

  const auto serialized = response.dump() + "\n";
  ::send(client_fd, serialized.data(), static_cast<int>(serialized.size()), 0);
  close_socket_fd(client_fd);
  return true;
}

bool GenerationTcpServer::serve_forever() {
  while (running_) {
    if (!serve_one() && !running_) {
      break;
    }
  }
  return true;
}

void GenerationTcpServer::record_request() const {
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    ++total_requests_;
  }
  write_state_file();
}

void GenerationTcpServer::record_error() const {
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    ++total_errors_;
  }
  write_state_file();
}

void GenerationTcpServer::write_state_file() const {
  if (config_.state_path.empty()) {
    return;
  }
  std::filesystem::create_directories(config_.state_path.parent_path());
  std::size_t queued = 0;
  std::size_t running = 0;
  std::size_t succeeded = 0;
  std::size_t failed = 0;
  for (const auto& job_id : service_.list_job_ids()) {
    const auto job = service_.find_job(job_id);
    if (!job.has_value()) {
      continue;
    }
    switch (job->status) {
      case GenerationJobStatus::Queued:
        ++queued;
        break;
      case GenerationJobStatus::Running:
        ++running;
        break;
      case GenerationJobStatus::Succeeded:
        ++succeeded;
        break;
      case GenerationJobStatus::Failed:
        ++failed;
        break;
    }
  }
  nlohmann::json state;
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    state = {
        {"ok", true},
        {"service", "sekailink_generation_server"},
        {"tcp_port", bound_port_},
        {"state_path", config_.state_path.string()},
        {"running", running_.load()},
        {"total_requests", total_requests_},
        {"total_errors", total_errors_},
        {"job_counts",
         {
             {"queued", queued},
             {"running", running},
             {"succeeded", succeeded},
             {"failed", failed},
         }},
        {"updated_at", utc_timestamp_now()},
    };
  }
  std::ofstream stream(config_.state_path);
  stream << state.dump(2) << "\n";
}

std::string generation_tcp_send_json_line(const std::string& host, std::uint16_t port, const nlohmann::json& payload) {
#if defined(_WIN32)
  WSADATA wsa_data;
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    throw std::runtime_error("wsa_startup_failed");
  }
#endif
  const int fd = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
  if (fd < 0) {
    throw std::runtime_error("socket_create_failed");
  }

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (::inet_pton(AF_INET, host.c_str(), &addr.sin_addr) != 1) {
    close_socket_fd(fd);
    throw std::runtime_error("inet_pton_failed");
  }
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    close_socket_fd(fd);
    throw std::runtime_error("connect_failed");
  }

  const auto data = payload.dump() + "\n";
  ::send(fd, data.data(), static_cast<int>(data.size()), 0);

  std::string response;
  char buffer[4096];
  while (true) {
    const auto read_count = ::recv(fd, buffer, sizeof(buffer), 0);
    if (read_count <= 0) {
      break;
    }
    response.append(buffer, static_cast<std::size_t>(read_count));
    if (response.find('\n') != std::string::npos) {
      break;
    }
  }
  close_socket_fd(fd);
#if defined(_WIN32)
  WSACleanup();
#endif
  const auto newline = response.find('\n');
  if (newline != std::string::npos) {
    response.resize(newline);
  }
  return response;
}

}  // namespace sekailink_server
