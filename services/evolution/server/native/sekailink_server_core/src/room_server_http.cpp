#include "sekailink_server/room_server_http.hpp"

#include "sekailink_server/room_state.hpp"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_len_t = socklen_t;
#endif

#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

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

nlohmann::json error_json(const std::string& message) {
  return {
      {"ok", false},
      {"error", message},
  };
}

HttpResponse json_response(int status, const nlohmann::json& body) {
  return HttpResponse{
      .status_code = status,
      .content_type = "application/json",
      .body = body.dump(),
  };
}

std::string http_status_text(int status_code) {
  switch (status_code) {
    case 200:
      return "OK";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    default:
      return "Internal Server Error";
  }
}

}  // namespace

RoomServerHttpService::RoomServerHttpService(const RoomRegistry& registry, const RoomServerAuthPolicy* auth_policy)
    : registry_(registry), auth_policy_(auth_policy) {}

HttpResponse RoomServerHttpService::handle_request(
    const std::string& method,
    const std::string& path,
    const std::optional<std::string>& bearer_token) const {
  if (method != "GET") {
    return json_response(405, error_json("method_not_allowed"));
  }
  return handle_get(path, bearer_token);
}

std::string RoomServerHttpService::build_http_response(const HttpResponse& response) {
  std::ostringstream stream;
  stream << "HTTP/1.1 " << response.status_code << ' ' << http_status_text(response.status_code) << "\r\n";
  stream << "Content-Type: " << response.content_type << "\r\n";
  stream << "Content-Length: " << response.body.size() << "\r\n";
  stream << "Connection: close\r\n\r\n";
  stream << response.body;
  return stream.str();
}

HttpResponse RoomServerHttpService::handle_get(
    const std::string& path,
    const std::optional<std::string>& bearer_token) const {
  if (path == "/health") {
    return json_response(200, {
        {"ok", true},
        {"service", "sekailink_room_server"},
        {"time", utc_timestamp_now()},
    });
  }
  if (auth_policy_ != nullptr && auth_policy_->admin_token.has_value()) {
    if (!bearer_token.has_value() || *bearer_token != *auth_policy_->admin_token) {
      return json_response(401, error_json("unauthorized"));
    }
  }
  if (path == "/rooms") {
    return json_response(200, {
        {"ok", true},
        {"room_ids", registry_.list_room_ids()},
    });
  }

  const auto parts = split_path(path);
  if (parts.size() >= 3 && parts[0] == "rooms") {
    const auto& room_id = parts[1];
    const auto* room = registry_.find_room(room_id);
    if (room == nullptr) {
      return json_response(404, error_json("room_not_found"));
    }

    if (parts[2] == "snapshot") {
      return json_response(200, {
          {"ok", true},
          {"snapshot", to_json(room->snapshot())},
      });
    }
    if (parts[2] == "summary") {
      const auto summary = room->activity_summary();
      return json_response(200, {
          {"ok", true},
          {"summary",
           {
               {"player_connections", summary.player_connections},
               {"player_disconnections", summary.player_disconnections},
               {"checks_recorded", summary.checks_recorded},
               {"items_received", summary.items_received},
               {"tracker_connections", summary.tracker_connections},
               {"emu_connections", summary.emu_connections},
           }},
      });
    }
    if (parts[2] == "events") {
      nlohmann::json events = nlohmann::json::array();
      for (const auto& event : room->events()) {
        events.push_back(nlohmann::json{
            {"event_type", event.event_type},
            {"timestamp", event.timestamp},
            {"payload", event.payload},
        });
      }
      return json_response(200, {
          {"ok", true},
          {"events", std::move(events)},
      });
    }
    if (parts[2] == "client-reports") {
      nlohmann::json reports = nlohmann::json::array();
      for (const auto& report : room->client_reports()) {
        reports.push_back(to_json(report));
      }
      return json_response(200, {
          {"ok", true},
          {"client_reports", std::move(reports)},
      });
    }
  }

  return json_response(404, error_json("route_not_found"));
}

RoomServerHttpTcpServer::RoomServerHttpTcpServer(
    const RoomRegistry& registry,
    std::uint16_t port,
    const RoomServerAuthPolicy* auth_policy)
    : service_(registry, auth_policy) {
#ifdef _WIN32
  throw std::runtime_error("room_server_http_tcp_not_supported_on_windows_yet");
#else
  listen_fd_ = ::socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) {
    throw std::runtime_error("http_socket_create_failed");
  }

  int reuse = 1;
  if (::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) != 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    throw std::runtime_error("http_setsockopt_failed");
  }

  sockaddr_in address{};
  address.sin_family = AF_INET;
  address.sin_port = htons(port);
  address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

  if (::bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    throw std::runtime_error("http_bind_failed");
  }
  if (::listen(listen_fd_, 16) != 0) {
    ::close(listen_fd_);
    listen_fd_ = -1;
    throw std::runtime_error("http_listen_failed");
  }
  socket_len_t len = sizeof(address);
  if (::getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&address), &len) == 0) {
    bound_port_ = ntohs(address.sin_port);
  }
#endif
}

RoomServerHttpTcpServer::~RoomServerHttpTcpServer() {
  stop();
}

void RoomServerHttpTcpServer::stop() {
#ifndef _WIN32
  if (listen_fd_ >= 0) {
    ::shutdown(listen_fd_, SHUT_RDWR);
    ::close(listen_fd_);
    listen_fd_ = -1;
    bound_port_ = 0;
  }
#endif
}

std::uint16_t RoomServerHttpTcpServer::port() const {
  return bound_port_;
}

void RoomServerHttpTcpServer::serve_one() const {
#ifndef _WIN32
  sockaddr_in client_address{};
  socklen_t client_length = sizeof(client_address);
  const int client_fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_address), &client_length);
  if (client_fd < 0) {
    throw std::runtime_error("http_accept_failed");
  }

  char buffer[4096];
  const auto received = ::recv(client_fd, buffer, sizeof(buffer), 0);
  if (received <= 0) {
    ::close(client_fd);
    throw std::runtime_error("http_recv_failed");
  }

  const std::string request(buffer, static_cast<std::size_t>(received));
  std::istringstream request_stream(request);
  std::string method;
  std::string path;
  std::string version;
  request_stream >> method >> path >> version;
  std::optional<std::string> bearer_token;
  std::string header_line;
  std::getline(request_stream, header_line);  // consume remainder of request line
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

  const auto response = service_.handle_request(method, path, bearer_token);
  const auto payload = RoomServerHttpService::build_http_response(response);
  if (::send(client_fd, payload.data(), payload.size(), 0) < 0) {
    ::close(client_fd);
    throw std::runtime_error("http_send_failed");
  }
  ::close(client_fd);
#else
  throw std::runtime_error("room_server_http_tcp_not_supported_on_windows_yet");
#endif
}

}  // namespace sekailink_server
