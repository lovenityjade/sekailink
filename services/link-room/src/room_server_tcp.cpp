#include "sekailink_server/room_server_tcp.hpp"

#include <cstring>
#include <iostream>
#include <stdexcept>

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

namespace sekailink_server {

namespace {

constexpr int kClientReadTimeoutMs = 3000;
constexpr int kClientSendTimeoutMs = 3000;
constexpr std::size_t kMaxRequestBytes = 64U * 1024U;

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

bool set_socket_recv_timeout(int fd, int timeout_ms) {
#if defined(_WIN32)
  const DWORD timeout = static_cast<DWORD>(timeout_ms);
  return ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0;
#else
  timeval timeout{};
  timeout.tv_sec = timeout_ms / 1000;
  timeout.tv_usec = (timeout_ms % 1000) * 1000;
  return ::setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) == 0;
#endif
}

bool set_socket_send_timeout(int fd, int timeout_ms) {
#if defined(_WIN32)
  const DWORD timeout = static_cast<DWORD>(timeout_ms);
  return ::setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&timeout), sizeof(timeout)) == 0;
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
    std::strncpy(host, "127.0.0.1", sizeof(host) - 1);
  }
  return std::string(host) + ":" + std::to_string(ntohs(client_addr.sin_port));
}

void emit_tcp_log(
    std::ostream& stream,
    const std::string& event,
    nlohmann::json payload = nlohmann::json::object()) {
  payload["service"] = "sekailink_room_server";
  payload["event"] = event;
  payload["time"] = utc_timestamp_now();
  stream << payload.dump() << "\n";
  stream.flush();
}

void set_optional_string(nlohmann::json& json, const char* key, const std::optional<std::string>& value) {
  json[key] = value.has_value() ? nlohmann::json(*value) : nlohmann::json(nullptr);
}

nlohmann::json slot_data_keys_json(const nlohmann::json& slot_data) {
  nlohmann::json keys = nlohmann::json::array();
  if (!slot_data.is_object()) {
    return keys;
  }
  for (auto it = slot_data.begin(); it != slot_data.end(); ++it) {
    keys.push_back(it.key());
  }
  return keys;
}

nlohmann::json latest_received_item_summary(const RoomStateSnapshot& snapshot) {
  if (snapshot.received_items.empty()) {
    return nullptr;
  }
  return to_json(snapshot.received_items.back());
}

nlohmann::json room_context_json(const RoomRegistry& registry, const nlohmann::json& command) {
  nlohmann::json room = nlohmann::json::object();
  if (!command.contains("room_id") || !command.at("room_id").is_string()) {
    room["room_found"] = false;
    return room;
  }
  const auto room_id = command.at("room_id").get<std::string>();
  room["room_id"] = room_id;
  const auto* session = registry.find_room(room_id);
  if (session == nullptr) {
    room["room_found"] = false;
    return room;
  }
  const auto snapshot = session->snapshot();
  room["room_found"] = true;
  room["game"] = snapshot.game;
  room["slot_id"] = snapshot.slot_id;
  room["slot_name"] = snapshot.slot_name;
  room["connection_state"] = connection_state_to_string(snapshot.connection_state);
  room["checked_count"] = snapshot.checked_locations.size();
  room["missing_count"] = snapshot.missing_locations.size();
  room["received_item_count"] = snapshot.received_items.size();
  room["next_received_item_index"] = snapshot.received_items.empty() ? 0 : snapshot.received_items.back().index + 1;
  room["tracker_connected"] = snapshot.tracker_connected;
  room["emu_connected"] = snapshot.emu_connected;
  room["slot_data_entry_count"] = snapshot.slot_data.is_object() ? snapshot.slot_data.size() : 0;
  room["slot_data_keys"] = slot_data_keys_json(snapshot.slot_data);
  room["latest_received_item"] = latest_received_item_summary(snapshot);
  set_optional_string(room, "seed_name", snapshot.seed_name);
  set_optional_string(room, "seed_id", snapshot.seed_id);
  set_optional_string(room, "seed_hash", snapshot.seed_hash);
  set_optional_string(room, "tracker_pack", snapshot.tracker_pack);
  set_optional_string(room, "tracker_variant", snapshot.tracker_variant);
  room["runtime_connected"] = snapshot.runtime_state.has_value() ? snapshot.runtime_state->connected : false;
  if (snapshot.runtime_state.has_value()) {
    set_optional_string(room, "driver_instance_id", snapshot.runtime_state->driver_instance_id);
    set_optional_string(room, "linkedworld_id", snapshot.runtime_state->linkedworld_id);
    set_optional_string(room, "core_profile", snapshot.runtime_state->core_profile);
    set_optional_string(room, "last_runtime_heartbeat", snapshot.runtime_state->last_heartbeat_at);
  }
  return room;
}

nlohmann::json command_context_json(const nlohmann::json& command) {
  nlohmann::json details = nlohmann::json::object();
  if (!command.contains("cmd") || !command.at("cmd").is_string()) {
    return details;
  }
  const auto cmd = command.at("cmd").get<std::string>();
  if (cmd == "create_room") {
    details["game"] = command.value("game", "");
    details["room_type"] = command.value("room_type", "live");
    details["slot_id"] = command.value("slot_id", 0);
    details["slot_name"] = command.value("slot_name", "");
    set_optional_string(details, "seed_name", command.contains("seed_name") && !command.at("seed_name").is_null()
                                         ? std::optional<std::string>(command.at("seed_name").get<std::string>())
                                         : std::nullopt);
    set_optional_string(details, "seed_id", command.contains("seed_id") && !command.at("seed_id").is_null()
                                       ? std::optional<std::string>(command.at("seed_id").get<std::string>())
                                       : std::nullopt);
    set_optional_string(details, "seed_hash", command.contains("seed_hash") && !command.at("seed_hash").is_null()
                                         ? std::optional<std::string>(command.at("seed_hash").get<std::string>())
                                         : std::nullopt);
    set_optional_string(details, "tracker_pack", command.contains("tracker_pack") && !command.at("tracker_pack").is_null()
                                            ? std::optional<std::string>(command.at("tracker_pack").get<std::string>())
                                            : std::nullopt);
    set_optional_string(
        details,
        "tracker_variant",
        command.contains("tracker_variant") && !command.at("tracker_variant").is_null()
            ? std::optional<std::string>(command.at("tracker_variant").get<std::string>())
            : std::nullopt);
    return details;
  }
  if (cmd == "set_seed_metadata") {
    set_optional_string(details, "seed_name", command.contains("seed_name") && !command.at("seed_name").is_null()
                                         ? std::optional<std::string>(command.at("seed_name").get<std::string>())
                                         : std::nullopt);
    set_optional_string(details, "seed_id", command.contains("seed_id") && !command.at("seed_id").is_null()
                                       ? std::optional<std::string>(command.at("seed_id").get<std::string>())
                                       : std::nullopt);
    set_optional_string(details, "seed_hash", command.contains("seed_hash") && !command.at("seed_hash").is_null()
                                         ? std::optional<std::string>(command.at("seed_hash").get<std::string>())
                                         : std::nullopt);
    return details;
  }
  if (cmd == "set_slot_data" && command.contains("slot_data")) {
    const auto& slot_data = command.at("slot_data");
    details["slot_data_shape"] = slot_data.type_name();
    details["slot_data_entry_count"] = slot_data.is_object() ? slot_data.size() : 0;
    details["slot_data_keys"] = slot_data_keys_json(slot_data);
    return details;
  }
  if (cmd == "enqueue_received_item" && command.contains("item") && command.at("item").is_object()) {
    const auto& item = command.at("item");
    details["item_index"] = item.value("index", 0);
    details["item_id"] = item.value("item_id", 0);
    details["item_name"] = item.value("item_name", "");
    details["location_id"] = item.value("location_id", 0);
    details["sender_slot"] = item.value("sender_slot", 0);
    details["sender_alias"] = item.value("sender_alias", "");
    return details;
  }
  if (cmd == "record_check") {
    details["location_id"] = command.value("location_id", 0);
    return details;
  }
  if (cmd == "set_location_mapping") {
    details["location_id"] = command.value("location_id", 0);
    details["receiver_slot"] = command.value("receiver_slot", 0);
    details["item_id"] = command.value("item_id", 0);
    details["location_name"] = command.value("location_name", "");
    return details;
  }
  if (cmd == "runtime_heartbeat") {
    set_optional_string(details, "runtime_kind", command.contains("runtime_kind") && !command.at("runtime_kind").is_null()
                                              ? std::optional<std::string>(command.at("runtime_kind").get<std::string>())
                                              : std::nullopt);
    set_optional_string(details,
                        "runtime_session_name",
                        command.contains("runtime_session_name") && !command.at("runtime_session_name").is_null()
                            ? std::optional<std::string>(command.at("runtime_session_name").get<std::string>())
                            : std::nullopt);
    set_optional_string(details, "driver_instance_id", command.contains("driver_instance_id") && !command.at("driver_instance_id").is_null()
                                                    ? std::optional<std::string>(command.at("driver_instance_id").get<std::string>())
                                                    : std::nullopt);
    set_optional_string(details, "linkedworld_id", command.contains("linkedworld_id") && !command.at("linkedworld_id").is_null()
                                                 ? std::optional<std::string>(command.at("linkedworld_id").get<std::string>())
                                                 : std::nullopt);
    set_optional_string(details, "core_profile", command.contains("core_profile") && !command.at("core_profile").is_null()
                                                ? std::optional<std::string>(command.at("core_profile").get<std::string>())
                                                : std::nullopt);
    details["connected"] = command.contains("connected") ? nlohmann::json(command.at("connected").get<bool>()) : nlohmann::json(true);
    return details;
  }
  if (cmd == "ingest_client_report" && command.contains("report") && command.at("report").is_object()) {
    const auto& report = command.at("report");
    details["report_type"] = report.value("report_type", "");
    details["severity"] = report.value("severity", "");
    details["source"] = report.value("source", "");
    return details;
  }
  return details;
}

}  // namespace

RoomServerTcpService::RoomServerTcpService(
    RoomRegistry& registry,
    RoomAuditStore* audit_store,
    RoomProjectionStore* projection_store,
    const RoomServerAuthPolicy* auth_policy)
    : registry_(registry), audit_store_(audit_store), projection_store_(projection_store), auth_policy_(auth_policy) {}

RoomServerTcpService::~RoomServerTcpService() {
  stop();
}

bool RoomServerTcpService::start(std::uint16_t port) {
  if (running_) {
    return false;
  }
#if defined(_WIN32)
  WSADATA wsa_data;
  if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
    return false;
  }
#endif
  listen_fd_ = static_cast<int>(::socket(AF_INET, SOCK_STREAM, 0));
  if (listen_fd_ < 0) {
    return false;
  }

  int reuse = 1;
  ::setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  addr.sin_port = htons(port);
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
  return true;
}

void RoomServerTcpService::stop() {
  running_ = false;
  shutdown_socket_fd(listen_fd_);
  close_socket_fd(listen_fd_);
  listen_fd_ = -1;
  bound_port_ = 0;
  if (server_thread_.joinable()) {
    server_thread_.join();
  }
#if defined(_WIN32)
  WSACleanup();
#endif
}

bool RoomServerTcpService::serve_one() {
  if (!running_ || listen_fd_ < 0) {
    return false;
  }
  sockaddr_in client_addr{};
  socket_len_t client_len = sizeof(client_addr);
  const int client_fd = static_cast<int>(::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &client_len));
  if (client_fd < 0) {
    return false;
  }

  set_socket_recv_timeout(client_fd, kClientReadTimeoutMs);
  set_socket_send_timeout(client_fd, kClientSendTimeoutMs);
  const auto peer = peer_address_string(client_addr);

  std::string payload;
  char buffer[4096];
  while (true) {
    const auto read_count = ::recv(client_fd, buffer, sizeof(buffer), 0);
    if (read_count <= 0) {
      break;
    }
    payload.append(buffer, static_cast<std::size_t>(read_count));
    if (payload.size() > kMaxRequestBytes) {
      const auto response = nlohmann::json({{"ok", false}, {"error", "request_too_large"}}).dump() + "\n";
      ::send(client_fd, response.data(), static_cast<int>(response.size()), 0);
      emit_tcp_log(std::cerr, "room_server_tcp_request_too_large",
                   {
                       {"peer", peer},
                       {"request_bytes", payload.size()},
                       {"max_request_bytes", kMaxRequestBytes},
                   });
      close_socket_fd(client_fd);
      return true;
    }
    if (payload.find('\n') != std::string::npos) {
      break;
    }
  }

  const auto newline = payload.find('\n');
  if (newline == std::string::npos) {
    const auto response = nlohmann::json({{"ok", false}, {"error", "incomplete_request"}}).dump() + "\n";
    ::send(client_fd, response.data(), static_cast<int>(response.size()), 0);
    emit_tcp_log(std::cerr, "room_server_tcp_incomplete_request",
                 {
                     {"peer", peer},
                     {"request_bytes", payload.size()},
                 });
    close_socket_fd(client_fd);
    return true;
  }
  payload.resize(newline);

  nlohmann::json envelope_json;
  nlohmann::json response;
  std::string channel = "unknown";
  std::string cmd;
  try {
    envelope_json = nlohmann::json::parse(payload);
    if (envelope_json.contains("channel") && envelope_json.at("channel").is_string()) {
      channel = envelope_json.at("channel").get<std::string>();
    }
    if (envelope_json.contains("command") && envelope_json.at("command").is_object() &&
        envelope_json.at("command").contains("cmd") && envelope_json.at("command").at("cmd").is_string()) {
      cmd = envelope_json.at("command").at("cmd").get<std::string>();
    }
    response = handle_protocol_json(registry_, audit_store_, projection_store_, auth_policy_, envelope_json);
  } catch (const std::exception& e) {
    response = {{"ok", false}, {"error", e.what()}};
  }
  const auto serialized = response.dump() + "\n";
  ::send(client_fd, serialized.data(), static_cast<int>(serialized.size()), 0);
  emit_tcp_log(std::cout, "room_server_tcp_command",
               {
                   {"peer", peer},
                   {"channel", channel},
                   {"cmd", cmd.empty() ? nlohmann::json(nullptr) : nlohmann::json(cmd)},
                   {"ok", response.value("ok", false)},
                   {"error", response.contains("error") ? response.at("error") : nlohmann::json(nullptr)},
                   {"request_bytes", payload.size()},
                   {"command", envelope_json.contains("command") && envelope_json.at("command").is_object()
                                   ? command_context_json(envelope_json.at("command"))
                                   : nlohmann::json::object()},
                   {"room", envelope_json.contains("command") && envelope_json.at("command").is_object()
                                ? room_context_json(registry_, envelope_json.at("command"))
                                : nlohmann::json::object()},
               });
  close_socket_fd(client_fd);
  return true;
}

bool RoomServerTcpService::serve_forever() {
  if (!running_ || listen_fd_ < 0) {
    return false;
  }
  while (running_) {
    if (!serve_one()) {
      if (!running_) {
        break;
      }
    }
  }
  return true;
}

bool RoomServerTcpService::start_background(std::uint16_t port) {
  if (!start(port)) {
    return false;
  }
  server_thread_ = std::thread([this]() {
    serve_forever();
  });
  return true;
}

void RoomServerTcpService::join() {
  if (server_thread_.joinable()) {
    server_thread_.join();
  }
}

std::uint16_t RoomServerTcpService::port() const {
  return bound_port_;
}

bool RoomServerTcpService::running() const {
  return running_;
}

std::string tcp_send_json_line(const std::string& host, std::uint16_t port, const nlohmann::json& payload) {
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
