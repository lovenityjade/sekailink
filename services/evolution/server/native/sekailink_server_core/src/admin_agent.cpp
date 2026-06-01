#include "sekailink_server/admin_agent.hpp"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
using socket_len_t = socklen_t;
#endif

#include <fstream>
#include <iomanip>
#include <map>
#include <numeric>
#include <sstream>
#include <stdexcept>

namespace sekailink_server {

namespace {

std::vector<std::string> split_path(std::string_view path) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start < path.size()) {
    while (start < path.size() && path[start] == '/') {
      ++start;
    }
    if (start >= path.size()) {
      break;
    }
    auto end = path.find('/', start);
    if (end == std::string_view::npos) {
      end = path.size();
    }
    parts.emplace_back(path.substr(start, end - start));
    start = end;
  }
  return parts;
}

nlohmann::json read_json_if_exists(const std::filesystem::path& path) {
  if (path.empty() || !std::filesystem::exists(path)) {
    return nullptr;
  }
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("admin_agent_state_open_failed");
  }
  nlohmann::json json;
  stream >> json;
  return json;
}

std::vector<std::string> tail_lines(const std::filesystem::path& path, std::size_t max_lines) {
  if (path.empty() || !std::filesystem::exists(path)) {
    return {};
  }
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("admin_agent_log_open_failed");
  }
  stream.seekg(0, std::ios::end);
  const auto end_pos = stream.tellg();
  if (end_pos <= 0) {
    return {};
  }
  constexpr std::streamoff kChunkSize = 4096;
  std::string buffer;
  std::size_t newline_count = 0;
  for (std::streamoff pos = static_cast<std::streamoff>(end_pos); pos > 0 && newline_count <= max_lines; ) {
    const auto read_size = std::min(kChunkSize, pos);
    pos -= read_size;
    stream.seekg(pos);
    std::string chunk(static_cast<std::size_t>(read_size), '\0');
    stream.read(chunk.data(), read_size);
    newline_count += static_cast<std::size_t>(std::count(chunk.begin(), chunk.end(), '\n'));
    buffer.insert(0, chunk);
  }
  std::vector<std::string> lines;
  std::istringstream tail_stream(buffer);
  std::string line;
  while (std::getline(tail_stream, line)) {
    lines.push_back(std::move(line));
  }
  if (lines.size() > max_lines) {
    lines.erase(lines.begin(), lines.end() - static_cast<std::ptrdiff_t>(max_lines));
  }
  return lines;
}

std::string http_status_text(int status_code) {
  switch (status_code) {
    case 200:
      return "OK";
    case 401:
      return "Unauthorized";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    default:
      return "Internal Server Error";
  }
}

std::string json_http_response(int status_code, const nlohmann::json& body) {
  const auto payload = body.dump();
  std::ostringstream stream;
  stream << "HTTP/1.1 " << status_code << ' ' << http_status_text(status_code) << "\r\n";
  stream << "Content-Type: application/json\r\n";
  stream << "Content-Length: " << payload.size() << "\r\n";
  stream << "Connection: close\r\n\r\n";
  stream << payload;
  return stream.str();
}

void maybe_assign_string(const nlohmann::json& json, const char* key, std::optional<std::string>& target) {
  if (json.contains(key) && !json.at(key).is_null()) {
    target = json.at(key).get<std::string>();
  }
}

nlohmann::json read_meminfo_bytes() {
  std::ifstream stream("/proc/meminfo");
  if (!stream) {
    return nullptr;
  }
  std::map<std::string, std::uint64_t> values_kib;
  std::string key;
  std::uint64_t value = 0;
  std::string unit;
  while (stream >> key >> value >> unit) {
    if (!key.empty() && key.back() == ':') {
      key.pop_back();
    }
    values_kib.insert_or_assign(key, value);
  }
  auto kib_to_bytes = [&](const char* name) -> nlohmann::json {
    const auto found = values_kib.find(name);
    if (found == values_kib.end()) {
      return nullptr;
    }
    return static_cast<std::uint64_t>(found->second) * 1024ULL;
  };
  return {
      {"total_bytes", kib_to_bytes("MemTotal")},
      {"free_bytes", kib_to_bytes("MemFree")},
      {"available_bytes", kib_to_bytes("MemAvailable")},
      {"swap_total_bytes", kib_to_bytes("SwapTotal")},
      {"swap_free_bytes", kib_to_bytes("SwapFree")},
  };
}

nlohmann::json read_loadavg() {
  std::ifstream stream("/proc/loadavg");
  if (!stream) {
    return nullptr;
  }
  double one = 0.0;
  double five = 0.0;
  double fifteen = 0.0;
  if (!(stream >> one >> five >> fifteen)) {
    return nullptr;
  }
  return {
      {"one", one},
      {"five", five},
      {"fifteen", fifteen},
  };
}

nlohmann::json read_uptime_seconds() {
  std::ifstream stream("/proc/uptime");
  if (!stream) {
    return nullptr;
  }
  double uptime = 0.0;
  if (!(stream >> uptime)) {
    return nullptr;
  }
  return uptime;
}

nlohmann::json filesystem_state(const std::filesystem::path& path) {
  std::error_code ec;
  const auto canonical = std::filesystem::exists(path, ec) ? std::filesystem::weakly_canonical(path, ec) : path;
  const auto space = std::filesystem::space(path, ec);
  if (ec) {
    return {
        {"path", path.string()},
        {"error", ec.message()},
    };
  }
  return {
      {"path", canonical.string()},
      {"capacity_bytes", static_cast<std::uint64_t>(space.capacity)},
      {"free_bytes", static_cast<std::uint64_t>(space.free)},
      {"available_bytes", static_cast<std::uint64_t>(space.available)},
  };
}

std::string host_name() {
  char buffer[256] = {};
  if (::gethostname(buffer, sizeof(buffer) - 1) != 0) {
    return {};
  }
  return std::string(buffer);
}

}  // namespace

AdminAgentConfig load_admin_agent_config(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("admin_agent_config_open_failed");
  }
  nlohmann::json json;
  stream >> json;

  AdminAgentConfig config;
  if (json.contains("http_port")) {
    config.http_port = json.at("http_port").get<std::uint16_t>();
  }
  if (json.contains("listen_host") && !json.at("listen_host").is_null()) {
    config.listen_host = json.at("listen_host").get<std::string>();
  }
  maybe_assign_string(json, "admin_token", config.admin_token);
  if (json.contains("systemctl_path") && !json.at("systemctl_path").is_null()) {
    config.systemctl_path = json.at("systemctl_path").get<std::string>();
  }
  if (json.contains("systemctl_use_sudo")) {
    config.systemctl_use_sudo = json.at("systemctl_use_sudo").get<bool>();
  }
  if (json.contains("services") && json.at("services").is_array()) {
    for (const auto& item : json.at("services")) {
      AdminAgentServiceDescriptor descriptor{
          .name = item.at("name").get<std::string>(),
          .state_file = item.value("state_file", ""),
          .log_file = item.value("log_file", ""),
      };
      if (item.contains("systemd_unit") && !item.at("systemd_unit").is_null()) {
        descriptor.systemd_unit = item.at("systemd_unit").get<std::string>();
      }
      config.services.push_back(std::move(descriptor));
    }
  }
  return config;
}

nlohmann::json to_json(const AdminAgentServiceDescriptor& descriptor) {
  return {
      {"name", descriptor.name},
      {"state_file", descriptor.state_file.empty() ? nlohmann::json(nullptr) : nlohmann::json(descriptor.state_file.string())},
      {"log_file", descriptor.log_file.empty() ? nlohmann::json(nullptr) : nlohmann::json(descriptor.log_file.string())},
      {"systemd_unit", descriptor.systemd_unit.has_value() ? nlohmann::json(*descriptor.systemd_unit) : nlohmann::json(nullptr)},
  };
}

nlohmann::json to_json(const AdminAgentConfig& config) {
  nlohmann::json services = nlohmann::json::array();
  for (const auto& service : config.services) {
    services.push_back(to_json(service));
  }
  return {
      {"http_port", config.http_port},
      {"listen_host", config.listen_host},
      {"admin_token", config.admin_token.has_value() ? nlohmann::json("<redacted>") : nlohmann::json(nullptr)},
      {"systemctl_path", config.systemctl_path},
      {"systemctl_use_sudo", config.systemctl_use_sudo},
      {"services", std::move(services)},
  };
}

AdminAgentService::AdminAgentService(AdminAgentConfig config)
    : config_(std::move(config)) {}

nlohmann::json AdminAgentService::handle_get(
    const std::string& path,
    const std::optional<std::string>& bearer_token) const {
  if (path == "/health") {
    return {
        {"ok", true},
        {"service", "sekailink_admin_agent"},
    };
  }
  if (!authorized(bearer_token)) {
    return {
        {"ok", false},
        {"status", 401},
        {"error", "unauthorized"},
    };
  }
  if (path == "/services") {
    nlohmann::json services = nlohmann::json::array();
    for (const auto& service : config_.services) {
      services.push_back(to_json(service));
    }
    return {
        {"ok", true},
        {"services", std::move(services)},
    };
  }
  if (path == "/system") {
    return {
        {"ok", true},
        {"system", system_state()},
    };
  }

  const auto parts = split_path(path);
  if (parts.size() >= 2 && parts[0] == "services") {
    const auto* service = find_service(parts[1]);
    if (service == nullptr) {
      return {
          {"ok", false},
          {"status", 404},
          {"error", "service_not_found"},
      };
    }
    if (parts.size() == 2) {
      return {
          {"ok", true},
          {"service", to_json(*service)},
          {"state", read_json_if_exists(service->state_file)},
      };
    }
    if (parts.size() == 3 && parts[2] == "logs") {
      return {
          {"ok", true},
          {"service", service->name},
          {"lines", tail_lines(service->log_file, 50)},
      };
    }
  }

  return {
      {"ok", false},
      {"status", 404},
      {"error", "route_not_found"},
  };
}

nlohmann::json AdminAgentService::handle_post(
    const std::string& path,
    const std::optional<std::string>& bearer_token) const {
  if (!authorized(bearer_token)) {
    return {
        {"ok", false},
        {"status", 401},
        {"error", "unauthorized"},
    };
  }

  const auto parts = split_path(path);
  if (parts.size() == 3 && parts[0] == "services" &&
      (parts[2] == "restart" || parts[2] == "start" || parts[2] == "stop")) {
    const auto* service = find_service(parts[1]);
    if (service == nullptr) {
      return {
          {"ok", false},
          {"status", 404},
          {"error", "service_not_found"},
      };
    }
    return control_service(*service, parts[2]);
  }

  return {
      {"ok", false},
      {"status", 404},
      {"error", "route_not_found"},
  };
}

bool AdminAgentService::authorized(const std::optional<std::string>& bearer_token) const {
  if (!config_.admin_token.has_value()) {
    return true;
  }
  return bearer_token.has_value() && *bearer_token == *config_.admin_token;
}

const AdminAgentServiceDescriptor* AdminAgentService::find_service(const std::string& name) const {
  for (const auto& service : config_.services) {
    if (service.name == name) {
      return &service;
    }
  }
  return nullptr;
}

nlohmann::json AdminAgentService::system_state() const {
  nlohmann::json filesystems = nlohmann::json::array();
  filesystems.push_back(filesystem_state("/"));
  for (const auto& extra : {std::filesystem::path("/opt/sekailink"), std::filesystem::path("/srv/nexus-data")}) {
    if (std::filesystem::exists(extra)) {
      filesystems.push_back(filesystem_state(extra));
    }
  }
  return {
      {"hostname", host_name()},
      {"listen_host", config_.listen_host},
      {"http_port", config_.http_port},
      {"uptime_seconds", read_uptime_seconds()},
      {"loadavg", read_loadavg()},
      {"memory", read_meminfo_bytes()},
      {"filesystems", std::move(filesystems)},
  };
}

nlohmann::json AdminAgentService::control_service(
    const AdminAgentServiceDescriptor& service,
    const std::string& action) const {
#ifdef _WIN32
  (void) service;
  (void) action;
  throw std::runtime_error("admin_agent_service_control_not_supported_on_windows_yet");
#else
  if (!service.systemd_unit.has_value()) {
    return {
        {"ok", false},
        {"status", 400},
        {"error", "service_restart_not_configured"},
    };
  }
  const pid_t pid = ::fork();
  if (pid < 0) {
    throw std::runtime_error("admin_agent_restart_fork_failed");
  }
  if (pid == 0) {
    if (config_.systemctl_use_sudo) {
      ::execl(
          "/usr/bin/sudo",
          "/usr/bin/sudo",
          "-n",
          config_.systemctl_path.c_str(),
          action.c_str(),
          service.systemd_unit->c_str(),
          static_cast<char*>(nullptr));
    } else {
      ::execl(
          config_.systemctl_path.c_str(),
          config_.systemctl_path.c_str(),
          action.c_str(),
          service.systemd_unit->c_str(),
          static_cast<char*>(nullptr));
    }
    _exit(127);
  }
  int status = 0;
  if (::waitpid(pid, &status, 0) < 0) {
    throw std::runtime_error("admin_agent_restart_waitpid_failed");
  }
  if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
    return {
        {"ok", false},
        {"status", 500},
        {"error", "service_control_failed"},
        {"action", action},
        {"service", service.name},
        {"systemd_unit", *service.systemd_unit},
    };
  }
  return {
      {"ok", true},
      {"action", action},
      {"service", service.name},
      {"systemd_unit", *service.systemd_unit},
  };
#endif
}

AdminAgentHttpServer::AdminAgentHttpServer(AdminAgentConfig config)
    : service_(config), config_(std::move(config)) {}

AdminAgentHttpServer::~AdminAgentHttpServer() {
  stop();
}

bool AdminAgentHttpServer::start() {
#ifdef _WIN32
  throw std::runtime_error("admin_agent_http_not_supported_on_windows_yet");
#else
  if (listen_fd_ >= 0) {
    return false;
  }
  listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    return false;
  }

  int reuse = 1;
  if (::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
    stop();
    return false;
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(config_.http_port);
  if (::inet_pton(AF_INET, config_.listen_host.c_str(), &address.sin_addr) != 1) {
    stop();
    return false;
  }

  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    stop();
    return false;
  }
  if (::listen(listen_fd_, 16) != 0) {
    stop();
    return false;
  }
  socket_len_t len = sizeof(address);
  if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&address), &len) == 0) {
    bound_port_ = ntohs(address.sin_port);
  }
  return true;
#endif
}

void AdminAgentHttpServer::stop() {
#ifndef _WIN32
  if (listen_fd_ >= 0) {
    ::shutdown(listen_fd_, SHUT_RDWR);
    ::close(listen_fd_);
    listen_fd_ = -1;
    bound_port_ = 0;
  }
#endif
}

std::uint16_t AdminAgentHttpServer::port() const {
  return bound_port_;
}

void AdminAgentHttpServer::serve_one() const {
#ifdef _WIN32
  throw std::runtime_error("admin_agent_http_not_supported_on_windows_yet");
#else
  sockaddr_in client_address{};
  socklen_t client_length = sizeof(client_address);
  const int client_fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_address), &client_length);
  if (client_fd < 0) {
    throw std::runtime_error("admin_agent_accept_failed");
  }

  char buffer[4096];
  const auto received = ::recv(client_fd, buffer, sizeof(buffer), 0);
  if (received <= 0) {
    ::close(client_fd);
    throw std::runtime_error("admin_agent_recv_failed");
  }

  const std::string request(buffer, static_cast<std::size_t>(received));
  std::istringstream request_stream(request);
  std::string method;
  std::string path;
  std::string version;
  request_stream >> method >> path >> version;

  std::optional<std::string> bearer_token;
  std::string header_line;
  std::getline(request_stream, header_line);
  while (std::getline(request_stream, header_line)) {
    if (header_line == "\r" || header_line.empty()) {
      break;
    }
    if (header_line.rfind("Authorization: Bearer ", 0) == 0) {
      auto token = header_line.substr(std::string("Authorization: Bearer ").size());
      if (!token.empty() && token.back() == '\r') {
        token.pop_back();
      }
      bearer_token = std::move(token);
    }
  }

  nlohmann::json body;
  int status_code = 200;
  if (method == "GET") {
    body = service_.handle_get(path, bearer_token);
  } else if (method == "POST") {
    body = service_.handle_post(path, bearer_token);
  } else {
    status_code = 405;
    body = {
        {"ok", false},
        {"error", "method_not_allowed"},
    };
  }
  if (body.contains("status")) {
    status_code = body.at("status").get<int>();
    body.erase("status");
  }

  const auto response = json_http_response(status_code, body);
  if (::send(client_fd, response.data(), response.size(), 0) < 0) {
    ::close(client_fd);
    throw std::runtime_error("admin_agent_send_failed");
  }
  ::close(client_fd);
#endif
}

}  // namespace sekailink_server
