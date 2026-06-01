#include "sekailink_server/evolution_room_query_service.hpp"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_len_t = socklen_t;
#endif

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <chrono>
#include <iomanip>

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

std::string trim(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
    value.erase(value.begin());
  }
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
    value.pop_back();
  }
  return value;
}

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::string url_decode(std::string_view value) {
  std::string decoded;
  decoded.reserve(value.size());
  for (std::size_t index = 0; index < value.size(); ++index) {
    const char current = value[index];
    if (current == '+') {
      decoded.push_back(' ');
      continue;
    }
    if (current == '%' && index + 2 < value.size()) {
      const auto hex = std::string(value.substr(index + 1, 2));
      char* end = nullptr;
      const auto parsed = std::strtol(hex.c_str(), &end, 16);
      if (end != nullptr && *end == '\0') {
        decoded.push_back(static_cast<char>(parsed));
        index += 2;
        continue;
      }
    }
    decoded.push_back(current);
  }
  return decoded;
}

std::pair<std::string, nlohmann::json> split_path_and_query(std::string_view raw_path) {
  const auto query_pos = raw_path.find('?');
  if (query_pos == std::string_view::npos) {
    return {std::string(raw_path), nlohmann::json::object()};
  }
  nlohmann::json query = nlohmann::json::object();
  const auto query_string = raw_path.substr(query_pos + 1);
  std::size_t start = 0;
  while (start <= query_string.size()) {
    const auto end = query_string.find('&', start);
    const auto segment = query_string.substr(start, end == std::string_view::npos ? query_string.size() - start : end - start);
    if (!segment.empty()) {
      const auto equals = segment.find('=');
      const auto key = url_decode(segment.substr(0, equals));
      const auto value = equals == std::string_view::npos ? std::string() : url_decode(segment.substr(equals + 1));
      if (!key.empty()) {
        query[key] = value;
      }
    }
    if (end == std::string_view::npos) {
      break;
    }
    start = end + 1;
  }
  return {std::string(raw_path.substr(0, query_pos)), std::move(query)};
}

std::optional<std::size_t> normalized_limit(const nlohmann::json& query, std::size_t max_value = 500) {
  if (!query.contains("limit") || !query.at("limit").is_string()) {
    return std::nullopt;
  }
  try {
    const auto parsed = std::stoll(trim(query.at("limit").get<std::string>()));
    if (parsed <= 0) {
      return std::nullopt;
    }
    return static_cast<std::size_t>(std::min<std::size_t>(static_cast<std::size_t>(parsed), max_value));
  } catch (...) {
    return std::nullopt;
  }
}

std::size_t normalized_offset(const nlohmann::json& query) {
  if (!query.contains("offset") || !query.at("offset").is_string()) {
    return 0;
  }
  try {
    const auto parsed = std::stoll(trim(query.at("offset").get<std::string>()));
    if (parsed <= 0) {
      return 0;
    }
    return static_cast<std::size_t>(parsed);
  } catch (...) {
    return 0;
  }
}

template <typename T>
void apply_offset_limit(std::vector<T>& values, const std::optional<std::size_t>& limit, std::size_t offset) {
  if (offset > 0) {
    if (offset >= values.size()) {
      values.clear();
      return;
    }
    values.erase(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(offset));
  }
  if (limit.has_value() && *limit < values.size()) {
    values.resize(*limit);
  }
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

std::string env_or_default(const char* name, const std::string& fallback) {
  const auto* value = std::getenv(name);
  if (value != nullptr && *value != '\0') {
    return value;
  }
  return fallback;
}

std::uint32_t env_or_default_u32(const char* name, std::uint32_t fallback) {
  const auto* value = std::getenv(name);
  if (value != nullptr && *value != '\0') {
    return static_cast<std::uint32_t>(std::stoul(value));
  }
  return fallback;
}

nlohmann::json summarize_snapshot(const RoomStateSnapshot& snapshot) {
  return {
      {"room_id", snapshot.room_id},
      {"room_type", room_type_to_string(snapshot.room_type)},
      {"game", snapshot.game},
      {"slot_name", snapshot.slot_name},
      {"slot_alias", snapshot.slot_alias},
      {"generated_at", snapshot.generated_at},
      {"checked_count", snapshot.checked_locations.size()},
      {"missing_count", snapshot.missing_locations.size()},
      {"received_item_count", snapshot.received_items.size()},
      {"tracker_connected", snapshot.tracker_connected},
      {"emu_connected", snapshot.emu_connected},
  };
}

nlohmann::json summarize_event(const RoomEvent& event) {
  const auto source = event.payload.contains("source") && event.payload.at("source").is_string()
                          ? nlohmann::json(event.payload.at("source").get<std::string>())
                          : nlohmann::json(nullptr);
  const auto severity = event.payload.contains("severity") && event.payload.at("severity").is_string()
                            ? nlohmann::json(event.payload.at("severity").get<std::string>())
                            : nlohmann::json(nullptr);
  return {
      {"event_type", event.event_type},
      {"timestamp", event.timestamp},
      {"source", source},
      {"severity", severity},
      {"payload", event.payload},
  };
}

nlohmann::json summarize_client_report(const ClientReport& report) {
  nlohmann::json json = {
      {"report_type", report.report_type},
      {"source", report.source},
      {"severity", report.severity},
      {"message", report.message},
      {"timestamp", report.timestamp},
      {"details", report.details},
  };
  json["request_id"] = report.request_id.has_value() ? nlohmann::json(*report.request_id) : nlohmann::json(nullptr);
  json["session_id"] = report.session_id.has_value() ? nlohmann::json(*report.session_id) : nlohmann::json(nullptr);
  json["user_id"] = report.user_id.has_value() ? nlohmann::json(*report.user_id) : nlohmann::json(nullptr);
  json["room_id"] = report.room_id.has_value() ? nlohmann::json(*report.room_id) : nlohmann::json(nullptr);
  json["lobby_id"] = report.lobby_id.has_value() ? nlohmann::json(*report.lobby_id) : nlohmann::json(nullptr);
  json["game"] = report.game.has_value() ? nlohmann::json(*report.game) : nlohmann::json(nullptr);
  json["runtime"] = report.runtime.has_value() ? nlohmann::json(*report.runtime) : nlohmann::json(nullptr);
  return json;
}

}  // namespace

EvolutionRoomQueryConfig load_evolution_room_query_config(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("evolution_room_query_config_open_failed");
  }

  nlohmann::json json;
  stream >> json;

  EvolutionRoomQueryConfig config;
  if (json.contains("http_port")) {
    config.http_port = json.at("http_port").get<std::uint16_t>();
  }
  if (json.contains("listen_host") && !json.at("listen_host").is_null()) {
    config.listen_host = json.at("listen_host").get<std::string>();
  }
  maybe_assign_string(json, "auth_token", config.auth_token);
  if (json.contains("projection_backend")) {
    config.projection_backend = parse_projection_backend(json.at("projection_backend").get<std::string>());
  }
  if (json.contains("projection_target") && !json.at("projection_target").is_null()) {
    config.projection_target = json.at("projection_target").get<std::string>();
  }
  if (json.contains("state_path") && !json.at("state_path").is_null()) {
    config.state_path = json.at("state_path").get<std::string>();
  }
  if (json.contains("mysql") && json.at("mysql").is_object()) {
    const auto& mysql = json.at("mysql");
    config.mysql_config.host = mysql.value("host", env_or_default("SEKAILINK_MYSQL_HOST", "127.0.0.1"));
    config.mysql_config.user = mysql.value("user", env_or_default("SEKAILINK_MYSQL_USER", ""));
    config.mysql_config.password = mysql.value("password", env_or_default("SEKAILINK_MYSQL_PASSWORD", ""));
    config.mysql_config.database = mysql.value("database", config.projection_target.string());
    config.mysql_config.port = mysql.value("port", env_or_default_u32("SEKAILINK_MYSQL_PORT", 3306));
    config.mysql_config.unix_socket = mysql.value("unix_socket", env_or_default("SEKAILINK_MYSQL_SOCKET", ""));
  } else {
    config.mysql_config.host = env_or_default("SEKAILINK_MYSQL_HOST", "127.0.0.1");
    config.mysql_config.user = env_or_default("SEKAILINK_MYSQL_USER", "");
    config.mysql_config.password = env_or_default("SEKAILINK_MYSQL_PASSWORD", "");
    config.mysql_config.database = config.projection_target.string();
    config.mysql_config.port = env_or_default_u32("SEKAILINK_MYSQL_PORT", 3306);
    config.mysql_config.unix_socket = env_or_default("SEKAILINK_MYSQL_SOCKET", "");
  }
  return config;
}

EvolutionRoomQueryStore::EvolutionRoomQueryStore(EvolutionRoomQueryConfig config)
    : config_(std::move(config)) {
  switch (config_.projection_backend) {
    case ProjectionBackend::Sqlite:
      store_.emplace<RoomProjectionSqliteStore>(config_.projection_target);
      break;
    case ProjectionBackend::Mysql: {
      auto mysql_config = config_.mysql_config;
      if (mysql_config.database.empty()) {
        mysql_config.database = config_.projection_target.string();
      }
      store_.emplace<RoomProjectionMysqlStore>(std::move(mysql_config));
      break;
    }
    case ProjectionBackend::Jsonl:
      throw std::runtime_error("evolution_room_query_jsonl_not_supported");
  }
}

std::vector<RoomStateSnapshot> EvolutionRoomQueryStore::list_latest_snapshots() const {
  if (const auto* sqlite_store = std::get_if<RoomProjectionSqliteStore>(&store_); sqlite_store != nullptr) {
    return latest_room_snapshots(*sqlite_store);
  }
  if (const auto* mysql_store = std::get_if<RoomProjectionMysqlStore>(&store_); mysql_store != nullptr) {
    return latest_room_snapshots(*mysql_store);
  }
  return {};
}

std::optional<RoomStateSnapshot> EvolutionRoomQueryStore::latest_snapshot(const std::string& room_id) const {
  if (const auto* sqlite_store = std::get_if<RoomProjectionSqliteStore>(&store_); sqlite_store != nullptr) {
    return latest_room_snapshot(*sqlite_store, room_id);
  }
  if (const auto* mysql_store = std::get_if<RoomProjectionMysqlStore>(&store_); mysql_store != nullptr) {
    return latest_room_snapshot(*mysql_store, room_id);
  }
  return std::nullopt;
}

std::vector<RoomEvent> EvolutionRoomQueryStore::events(const std::string& room_id) const {
  if (const auto* sqlite_store = std::get_if<RoomProjectionSqliteStore>(&store_); sqlite_store != nullptr) {
    return room_events(*sqlite_store, room_id);
  }
  if (const auto* mysql_store = std::get_if<RoomProjectionMysqlStore>(&store_); mysql_store != nullptr) {
    return room_events(*mysql_store, room_id);
  }
  return {};
}

std::vector<ClientReport> EvolutionRoomQueryStore::reports(const std::string& room_id) const {
  if (const auto* sqlite_store = std::get_if<RoomProjectionSqliteStore>(&store_); sqlite_store != nullptr) {
    return client_reports(*sqlite_store, room_id);
  }
  if (const auto* mysql_store = std::get_if<RoomProjectionMysqlStore>(&store_); mysql_store != nullptr) {
    return client_reports(*mysql_store, room_id);
  }
  return {};
}

EvolutionRoomQueryHttpService::EvolutionRoomQueryHttpService(EvolutionRoomQueryConfig config)
    : config_(config), store_(std::move(config)) {
  write_state_file();
}

nlohmann::json EvolutionRoomQueryHttpService::handle_get(
    const std::string& path,
    const std::optional<std::string>& bearer_token) const {
  record_request();
  const auto [normalized_path, query] = split_path_and_query(path);
  if (normalized_path == "/health") {
    return {
        {"ok", true},
        {"service", "sekailink_evolution_room_query"},
        {"projection_backend", projection_backend_name(config_.projection_backend)},
    };
  }
  if (!authorized(bearer_token)) {
    return {
        {"ok", false},
        {"status", 401},
        {"error", "unauthorized"},
    };
  }
  if (normalized_path == "/rooms") {
    const auto limit = normalized_limit(query);
    const auto offset = normalized_offset(query);
    auto snapshots = store_.list_latest_snapshots();
    apply_offset_limit(snapshots, limit, offset);
    nlohmann::json rooms = nlohmann::json::array();
    for (const auto& snapshot : snapshots) {
      rooms.push_back(summarize_snapshot(snapshot));
    }
    return {
        {"ok", true},
        {"limit", limit.has_value() ? nlohmann::json(*limit) : nlohmann::json(nullptr)},
        {"offset", offset},
        {"rooms", std::move(rooms)},
    };
  }

  const auto parts = split_path(normalized_path);
  if (parts.size() == 2 && parts[0] == "rooms") {
    const auto snapshot = store_.latest_snapshot(parts[1]);
    if (!snapshot.has_value()) {
      return {
          {"ok", false},
          {"status", 404},
          {"error", "room_not_found"},
      };
    }
    return {
        {"ok", true},
        {"snapshot", to_json(*snapshot)},
    };
  }
  if (parts.size() == 3 && parts[0] == "rooms" && parts[2] == "events") {
    const auto limit = normalized_limit(query);
    const auto offset = normalized_offset(query);
    auto room_events_list = store_.events(parts[1]);
    const auto requested_event_type = query.contains("event_type") && query.at("event_type").is_string() &&
                                              !trim(query.at("event_type").get<std::string>()).empty()
                                          ? std::optional<std::string>(lower(trim(query.at("event_type").get<std::string>())))
                                          : std::nullopt;
    const auto requested_severity = query.contains("severity") && query.at("severity").is_string() &&
                                            !trim(query.at("severity").get<std::string>()).empty()
                                        ? std::optional<std::string>(lower(trim(query.at("severity").get<std::string>())))
                                        : std::nullopt;
    const auto requested_source = query.contains("source") && query.at("source").is_string() &&
                                          !trim(query.at("source").get<std::string>()).empty()
                                      ? std::optional<std::string>(lower(trim(query.at("source").get<std::string>())))
                                      : std::nullopt;
    room_events_list.erase(
        std::remove_if(
            room_events_list.begin(),
            room_events_list.end(),
            [&](const RoomEvent& event) {
              if (requested_event_type.has_value() && lower(event.event_type) != *requested_event_type) {
                return true;
              }
              const auto payload_severity = event.payload.contains("severity") && event.payload.at("severity").is_string()
                                                ? std::optional<std::string>(lower(event.payload.at("severity").get<std::string>()))
                                                : std::nullopt;
              if (requested_severity.has_value() && payload_severity != requested_severity) {
                return true;
              }
              const auto payload_source = event.payload.contains("source") && event.payload.at("source").is_string()
                                              ? std::optional<std::string>(lower(event.payload.at("source").get<std::string>()))
                                              : std::nullopt;
              return requested_source.has_value() && payload_source != requested_source;
            }),
        room_events_list.end());
    apply_offset_limit(room_events_list, limit, offset);
    nlohmann::json events = nlohmann::json::array();
    for (const auto& event : room_events_list) {
      events.push_back(summarize_event(event));
    }
    return {
        {"ok", true},
        {"room_id", parts[1]},
        {"limit", limit.has_value() ? nlohmann::json(*limit) : nlohmann::json(nullptr)},
        {"offset", offset},
        {"events", std::move(events)},
    };
  }
  if (parts.size() == 3 && parts[0] == "rooms" && parts[2] == "client-reports") {
    const auto limit = normalized_limit(query);
    const auto offset = normalized_offset(query);
    auto report_list = store_.reports(parts[1]);
    apply_offset_limit(report_list, limit, offset);
    nlohmann::json reports = nlohmann::json::array();
    for (const auto& report : report_list) {
      reports.push_back(summarize_client_report(report));
    }
    return {
        {"ok", true},
        {"room_id", parts[1]},
        {"limit", limit.has_value() ? nlohmann::json(*limit) : nlohmann::json(nullptr)},
        {"offset", offset},
        {"client_reports", std::move(reports)},
    };
  }
  if (parts.size() == 3 && parts[0] == "rooms" && parts[2] == "diagnostics") {
    const auto snapshot = store_.latest_snapshot(parts[1]);
    const auto events = store_.events(parts[1]);
    const auto reports = store_.reports(parts[1]);
    nlohmann::json response = {
        {"ok", true},
        {"room_id", parts[1]},
        {"event_count", events.size()},
        {"client_report_count", reports.size()},
    };
    response["snapshot_summary"] = snapshot.has_value() ? summarize_snapshot(*snapshot) : nlohmann::json(nullptr);
    response["latest_event"] = !events.empty() ? summarize_event(events.back()) : nlohmann::json(nullptr);
    response["latest_client_report"] =
        !reports.empty() ? summarize_client_report(reports.back()) : nlohmann::json(nullptr);
    return response;
  }

  return {
      {"ok", false},
      {"status", 404},
      {"error", "route_not_found"},
  };
}

bool EvolutionRoomQueryHttpService::authorized(const std::optional<std::string>& bearer_token) const {
  if (!config_.auth_token.has_value()) {
    return true;
  }
  return bearer_token.has_value() && *bearer_token == *config_.auth_token;
}

void EvolutionRoomQueryHttpService::record_request() const {
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    ++total_requests_;
  }
  write_state_file();
}

void EvolutionRoomQueryHttpService::record_error() const {
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    ++total_errors_;
  }
  write_state_file();
}

void EvolutionRoomQueryHttpService::write_state_file() const {
  if (config_.state_path.empty()) {
    return;
  }
  std::lock_guard<std::mutex> lock(state_mutex_);
  std::filesystem::create_directories(config_.state_path.parent_path());
  nlohmann::json state = {
      {"ok", true},
      {"service", "sekailink_evolution_room_query"},
      {"listen_host", config_.listen_host},
      {"http_port", config_.http_port},
      {"projection_backend", projection_backend_name(config_.projection_backend)},
      {"projection_target", config_.projection_target.empty() ? nlohmann::json(nullptr) : nlohmann::json(config_.projection_target.string())},
      {"state_path", config_.state_path.string()},
      {"total_requests", total_requests_},
      {"total_errors", total_errors_},
      {"updated_at", utc_timestamp_now()},
  };
  if (config_.projection_backend == ProjectionBackend::Mysql) {
    state["mysql"] = {
        {"host", config_.mysql_config.host},
        {"database", config_.mysql_config.database},
        {"port", config_.mysql_config.port},
        {"unix_socket", config_.mysql_config.unix_socket.empty() ? nlohmann::json(nullptr) : nlohmann::json(config_.mysql_config.unix_socket)},
    };
  }
  std::ofstream stream(config_.state_path);
  stream << state.dump(2) << "\n";
}

EvolutionRoomQueryHttpServer::EvolutionRoomQueryHttpServer(EvolutionRoomQueryConfig config)
    : service_(config), config_(std::move(config)) {}

EvolutionRoomQueryHttpServer::~EvolutionRoomQueryHttpServer() {
  stop();
}

bool EvolutionRoomQueryHttpServer::start() {
#ifdef _WIN32
  throw std::runtime_error("evolution_room_query_http_not_supported_on_windows_yet");
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

void EvolutionRoomQueryHttpServer::stop() {
#ifndef _WIN32
  if (listen_fd_ >= 0) {
    ::shutdown(listen_fd_, SHUT_RDWR);
    ::close(listen_fd_);
    listen_fd_ = -1;
    bound_port_ = 0;
  }
#endif
}

std::uint16_t EvolutionRoomQueryHttpServer::port() const {
  return bound_port_;
}

void EvolutionRoomQueryHttpServer::serve_one() const {
#ifdef _WIN32
  throw std::runtime_error("evolution_room_query_http_not_supported_on_windows_yet");
#else
  sockaddr_in client_address{};
  socklen_t client_length = sizeof(client_address);
  const int client_fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_address), &client_length);
  if (client_fd < 0) {
    throw std::runtime_error("evolution_room_query_accept_failed");
  }

  char buffer[4096];
  const auto received = ::recv(client_fd, buffer, sizeof(buffer), 0);
  if (received <= 0) {
    ::close(client_fd);
    throw std::runtime_error("evolution_room_query_recv_failed");
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
  if (method != "GET") {
    status_code = 405;
    body = {
        {"ok", false},
        {"error", "method_not_allowed"},
    };
  } else {
    try {
      body = service_.handle_get(path, bearer_token);
      if (body.contains("status")) {
        status_code = body.at("status").get<int>();
        body.erase("status");
      }
    } catch (const std::exception& exception) {
      status_code = 500;
      body = {
          {"ok", false},
          {"error", "internal_error"},
          {"message", exception.what()},
      };
    }
  }

  const auto response = json_http_response(status_code, body);
  if (::send(client_fd, response.data(), response.size(), 0) < 0) {
    ::close(client_fd);
    throw std::runtime_error("evolution_room_query_send_failed");
  }
  ::close(client_fd);
#endif
}

}  // namespace sekailink_server
