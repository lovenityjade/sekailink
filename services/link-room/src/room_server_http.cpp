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
#include <iostream>

namespace sekailink_server {

namespace {

constexpr int kHttpReadTimeoutMs = 3000;
constexpr int kHttpSendTimeoutMs = 3000;
constexpr std::size_t kMaxHttpRequestBytes = 64U * 1024U;

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
    case 400:
      return "Bad Request";
    case 401:
      return "Unauthorized";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 413:
      return "Payload Too Large";
    default:
      return "Internal Server Error";
  }
}

bool set_socket_recv_timeout(int fd, int timeout_ms) {
#ifdef _WIN32
  (void)fd;
  (void)timeout_ms;
  return false;
#else
  timeval timeout{};
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = (timeout_ms % 1000) * 1000;
  return ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0;
#endif
}

bool set_socket_send_timeout(int fd, int timeout_ms) {
#ifdef _WIN32
  (void)fd;
  (void)timeout_ms;
  return false;
#else
  timeval timeout{};
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = (timeout_ms % 1000) * 1000;
  return ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) == 0;
#endif
}

std::string peer_address_string(const sockaddr_in& client_addr) {
  char host[INET_ADDRSTRLEN] = {0};
  if (::inet_ntop(AF_INET, &client_addr.sin_addr, host, sizeof(host)) == nullptr) {
    return "127.0.0.1:0";
  }
  return std::string(host) + ":" + std::to_string(ntohs(client_addr.sin_port));
}

void emit_http_log(
    std::ostream& stream,
    const std::string& event,
    nlohmann::json payload = nlohmann::json::object()) {
  payload["service"] = "sekailink_room_server";
  payload["event"] = event;
  payload["time"] = utc_timestamp_now();
  stream << payload.dump() << "\n";
  stream.flush();
}

nlohmann::json room_context_for_path(const RoomRegistry& registry, const std::string& path) {
  nlohmann::json room = nlohmann::json::object();
  const auto parts = split_path(path);
  if (parts.size() < 2 || parts[0] != "rooms") {
    room["room_found"] = false;
    return room;
  }
  room["room_id"] = parts[1];
  const auto* session = registry.find_room(parts[1]);
  if (session == nullptr) {
    room["room_found"] = false;
    return room;
  }
  const auto snapshot = session->snapshot();
  room["room_found"] = true;
  room["game"] = snapshot.game;
  room["slot_id"] = snapshot.slot_id;
  room["slot_name"] = snapshot.slot_name;
  room["checked_count"] = snapshot.checked_locations.size();
  room["received_item_count"] = snapshot.received_items.size();
  room["tracker_connected"] = snapshot.tracker_connected;
  room["emu_connected"] = snapshot.emu_connected;
  room["next_received_item_index"] = snapshot.received_items.empty() ? 0 : snapshot.received_items.back().index + 1;
  room["runtime_connected"] = snapshot.runtime_state.has_value() ? snapshot.runtime_state->connected : false;
  room["seed_name"] = snapshot.seed_name.has_value() ? nlohmann::json(*snapshot.seed_name) : nlohmann::json(nullptr);
  room["seed_id"] = snapshot.seed_id.has_value() ? nlohmann::json(*snapshot.seed_id) : nlohmann::json(nullptr);
  room["seed_hash"] = snapshot.seed_hash.has_value() ? nlohmann::json(*snapshot.seed_hash) : nlohmann::json(nullptr);
  return room;
}

nlohmann::json build_error_response(int status, const std::string& message) {
  return nlohmann::json{
      {"status", status},
      {"response", RoomServerHttpService::build_http_response(json_response(status, error_json(message)))},
  };
}

bool header_complete(const std::string& request) {
  return request.find("\r\n\r\n") != std::string::npos || request.find("\n\n") != std::string::npos;
}

std::string read_http_request(int client_fd) {
  std::string request;
  char buffer[4096];
  while (true) {
    const auto received = ::recv(client_fd, buffer, sizeof(buffer), 0);
    if (received <= 0) {
      break;
    }
    request.append(buffer, static_cast<std::size_t>(received));
    if (request.size() > kMaxHttpRequestBytes || header_complete(request)) {
      break;
    }
  }
  return request;
}

std::optional<std::string> extract_bearer_token(std::istringstream& request_stream) {
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
  return bearer_token;
}

void send_raw_response(int client_fd, const std::string& response) {
  if (::send(client_fd, response.data(), response.size(), 0) < 0) {
    throw std::runtime_error("http_send_failed");
  }
}

bool auth_enabled(const std::optional<std::string>& token) {
  return token.has_value() && !token->empty();
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

const RoomRegistry& RoomServerHttpService::registry() const {
  return registry_;
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
        {"loopback_only", true},
        {"http_bind_host", "127.0.0.1"},
        {"room_count", registry_.list_room_ids().size()},
        {"admin_auth_enabled", auth_policy_ != nullptr && auth_enabled(auth_policy_->admin_token)},
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
      const auto snapshot = room->snapshot();
      return json_response(200, {
          {"ok", true},
          {"summary",
           {
               {"room_id", snapshot.room_id},
               {"game", snapshot.game},
               {"slot_id", snapshot.slot_id},
               {"slot_name", snapshot.slot_name},
               {"slot_alias", snapshot.slot_alias},
               {"seed_name", snapshot.seed_name.has_value() ? nlohmann::json(*snapshot.seed_name) : nlohmann::json(nullptr)},
               {"seed_id", snapshot.seed_id.has_value() ? nlohmann::json(*snapshot.seed_id) : nlohmann::json(nullptr)},
               {"seed_hash", snapshot.seed_hash.has_value() ? nlohmann::json(*snapshot.seed_hash) : nlohmann::json(nullptr)},
               {"tracker_pack", snapshot.tracker_pack.has_value() ? nlohmann::json(*snapshot.tracker_pack) : nlohmann::json(nullptr)},
               {"tracker_variant", snapshot.tracker_variant.has_value() ? nlohmann::json(*snapshot.tracker_variant) : nlohmann::json(nullptr)},
               {"player_connections", summary.player_connections},
               {"player_disconnections", summary.player_disconnections},
               {"checks_recorded", summary.checks_recorded},
               {"items_received", summary.items_received},
               {"tracker_connections", summary.tracker_connections},
               {"emu_connections", summary.emu_connections},
               {"checked_count", snapshot.checked_locations.size()},
               {"missing_count", snapshot.missing_locations.size()},
               {"received_item_count", snapshot.received_items.size()},
               {"next_received_item_index", room->next_received_item_index()},
               {"runtime_connected", snapshot.runtime_state.has_value() ? nlohmann::json(snapshot.runtime_state->connected) : nlohmann::json(false)},
               {"last_runtime_heartbeat",
                snapshot.runtime_state.has_value() && snapshot.runtime_state->last_heartbeat_at.has_value()
                    ? nlohmann::json(*snapshot.runtime_state->last_heartbeat_at)
                    : nlohmann::json(nullptr)},
               {"driver_instance_id",
                snapshot.runtime_state.has_value() && snapshot.runtime_state->driver_instance_id.has_value()
                    ? nlohmann::json(*snapshot.runtime_state->driver_instance_id)
                    : nlohmann::json(nullptr)},
               {"linkedworld_id",
                snapshot.runtime_state.has_value() && snapshot.runtime_state->linkedworld_id.has_value()
                    ? nlohmann::json(*snapshot.runtime_state->linkedworld_id)
                    : nlohmann::json(nullptr)},
               {"core_profile",
                snapshot.runtime_state.has_value() && snapshot.runtime_state->core_profile.has_value()
                    ? nlohmann::json(*snapshot.runtime_state->core_profile)
                    : nlohmann::json(nullptr)},
           }},
      });
    }
    if (parts[2] == "sync") {
      return json_response(200, {
          {"ok", true},
          {"sync", room->sync_payload(0)},
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
  set_socket_recv_timeout(client_fd, kHttpReadTimeoutMs);
  set_socket_send_timeout(client_fd, kHttpSendTimeoutMs);
  const auto peer = peer_address_string(client_address);

  const auto request = read_http_request(client_fd);
  if (request.empty()) {
    ::close(client_fd);
    throw std::runtime_error("http_recv_failed");
  }
  if (request.size() > kMaxHttpRequestBytes) {
    const auto error = build_error_response(413, "request_too_large");
    send_raw_response(client_fd, error.at("response").get<std::string>());
    emit_http_log(std::cerr, "room_server_http_request_too_large",
                  {
                      {"peer", peer},
                      {"request_bytes", request.size()},
                      {"max_request_bytes", kMaxHttpRequestBytes},
                      {"status", error.at("status")},
                  });
    ::close(client_fd);
    return;
  }
  if (!header_complete(request)) {
    const auto error = build_error_response(400, "incomplete_request");
    send_raw_response(client_fd, error.at("response").get<std::string>());
    emit_http_log(std::cerr, "room_server_http_incomplete_request",
                  {
                      {"peer", peer},
                      {"request_bytes", request.size()},
                      {"status", error.at("status")},
                  });
    ::close(client_fd);
    return;
  }

  std::istringstream request_stream(request);
  std::string method;
  std::string path;
  std::string version;
  request_stream >> method >> path >> version;
  if (method.empty() || path.empty() || version.empty()) {
    const auto error = build_error_response(400, "invalid_request_line");
    send_raw_response(client_fd, error.at("response").get<std::string>());
    emit_http_log(std::cerr, "room_server_http_invalid_request_line",
                  {
                      {"peer", peer},
                      {"request_bytes", request.size()},
                      {"status", error.at("status")},
                  });
    ::close(client_fd);
    return;
  }
  const auto bearer_token = extract_bearer_token(request_stream);

  const auto response = service_.handle_request(method, path, bearer_token);
  const auto payload = RoomServerHttpService::build_http_response(response);
  send_raw_response(client_fd, payload);
  emit_http_log(std::cout, "room_server_http_request",
                {
                    {"peer", peer},
                    {"method", method},
                    {"path", path},
                    {"status", response.status_code},
                    {"auth_present", bearer_token.has_value()},
                    {"room", room_context_for_path(service_.registry(), path)},
                });
  ::close(client_fd);
#else
  throw std::runtime_error("room_server_http_tcp_not_supported_on_windows_yet");
#endif
}

}  // namespace sekailink_server
