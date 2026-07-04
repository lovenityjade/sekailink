#include "sekailink_server/chat_gateway_service.hpp"

#ifndef _WIN32
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_len_t = socklen_t;
#endif

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cerrno>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <map>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

namespace sekailink_server {

namespace {

std::string trim(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) value.erase(value.begin());
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) value.pop_back();
  return value;
}

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::vector<std::string> split_path(std::string_view path) {
  std::vector<std::string> parts;
  std::size_t start = 0;
  while (start < path.size()) {
    while (start < path.size() && path[start] == '/') ++start;
    if (start >= path.size()) break;
    auto end = path.find('/', start);
    if (end == std::string_view::npos) end = path.size();
    parts.emplace_back(path.substr(start, end - start));
    start = end;
  }
  return parts;
}

std::string url_decode(std::string_view value) {
  std::string decoded;
  decoded.reserve(value.size());
  for (std::size_t i = 0; i < value.size(); ++i) {
    if (value[i] == '+') {
      decoded.push_back(' ');
      continue;
    }
    if (value[i] == '%' && i + 2 < value.size()) {
      const auto hex = std::string(value.substr(i + 1, 2));
      char* end = nullptr;
      const auto parsed = std::strtol(hex.c_str(), &end, 16);
      if (end && *end == '\0') {
        decoded.push_back(static_cast<char>(parsed));
        i += 2;
        continue;
      }
    }
    decoded.push_back(value[i]);
  }
  return decoded;
}

std::string path_without_query(std::string_view raw_path) {
  const auto q = raw_path.find('?');
  return std::string(raw_path.substr(0, q == std::string_view::npos ? raw_path.size() : q));
}

std::string http_status_text(int status_code) {
  switch (status_code) {
    case 200: return "OK";
    case 400: return "Bad Request";
    case 401: return "Unauthorized";
    case 404: return "Not Found";
    case 502: return "Bad Gateway";
    default: return "Internal Server Error";
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

bool is_safe_channel_id(const std::string& value) {
  if (value.empty() || value.size() > 96) return false;
  return std::all_of(value.begin(), value.end(), [](unsigned char c) {
    return std::isalnum(c) != 0 || c == ':' || c == '_' || c == '-';
  });
}

std::string irc_channel_name(std::string channel_id) {
  std::replace(channel_id.begin(), channel_id.end(), ':', '-');
  if (channel_id.empty() || channel_id.front() != '#') channel_id.insert(channel_id.begin(), '#');
  return channel_id;
}

std::string sanitize_irc_text(std::string value, std::size_t max_len) {
  value.erase(std::remove_if(value.begin(), value.end(), [](unsigned char c) {
    return c == '\r' || c == '\n' || c == '\0';
  }), value.end());
  value = trim(value);
  if (value.size() > max_len) value.resize(max_len);
  return value;
}

std::string sanitize_nick(std::string value) {
  value = sanitize_irc_text(value, 24);
  std::string out;
  for (unsigned char c : value) {
    if (std::isalnum(c) != 0 || c == '_' || c == '-') out.push_back(static_cast<char>(c));
  }
  if (out.empty()) out = "SekaiLink";
  if (std::isdigit(static_cast<unsigned char>(out.front())) != 0) out.insert(out.begin(), 'u');
  if (out.size() > 24) out.resize(24);
  return out;
}

std::string utc_now_iso8601() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm tm{};
#ifdef _WIN32
  gmtime_s(&tm, &time);
#else
  gmtime_r(&time, &tm);
#endif
  std::ostringstream stream;
  stream << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
  return stream.str();
}

#ifndef _WIN32
void close_fd(int fd) {
  if (fd >= 0) close(fd);
}

bool wait_for_read(int fd, int timeout_ms) {
  fd_set set;
  FD_ZERO(&set);
  FD_SET(fd, &set);
  timeval tv{};
  tv.tv_sec = timeout_ms / 1000;
  tv.tv_usec = (timeout_ms % 1000) * 1000;
  return select(fd + 1, &set, nullptr, nullptr, &tv) > 0;
}

bool write_all(int fd, const std::string& data) {
  const char* ptr = data.data();
  std::size_t left = data.size();
  while (left > 0) {
    const auto sent = send(fd, ptr, left, MSG_NOSIGNAL);
    if (sent <= 0) return false;
    ptr += sent;
    left -= static_cast<std::size_t>(sent);
  }
  return true;
}

std::vector<std::string> extract_irc_lines(std::string& buffer) {
  std::vector<std::string> lines;
  std::size_t pos = 0;
  while ((pos = buffer.find("\r\n")) != std::string::npos) {
    lines.push_back(buffer.substr(0, pos));
    buffer.erase(0, pos + 2);
  }
  return lines;
}

bool read_irc_lines(int fd, std::string& buffer, std::vector<std::string>& lines, int timeout_ms) {
  if (!wait_for_read(fd, timeout_ms)) return true;
  char chunk[2048];
  const auto n = recv(fd, chunk, sizeof(chunk), 0);
  if (n <= 0) return false;
  buffer.append(chunk, static_cast<std::size_t>(n));
  auto next = extract_irc_lines(buffer);
  lines.insert(lines.end(), next.begin(), next.end());
  return true;
}

bool handle_irc_ping(int fd, const std::string& line) {
  if (line.rfind("PING ", 0) != 0) return true;
  return write_all(fd, "PONG " + line.substr(5) + "\r\n");
}

bool open_irc_connection(
    const ChatGatewayServiceConfig& config,
    const std::string& nick,
    const std::string& realname,
    int& fd,
    std::string& error) {
  addrinfo hints{};
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  addrinfo* result = nullptr;
  const auto port = std::to_string(config.irc_port);
  if (getaddrinfo(config.irc_host.c_str(), port.c_str(), &hints, &result) != 0 || result == nullptr) {
    error = "irc_resolve_failed";
    return false;
  }
  fd = -1;
  for (auto* ai = result; ai != nullptr; ai = ai->ai_next) {
    fd = socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
    if (fd < 0) continue;
    if (connect(fd, ai->ai_addr, ai->ai_addrlen) == 0) break;
    close_fd(fd);
    fd = -1;
  }
  freeaddrinfo(result);
  if (fd < 0) {
    error = "irc_connect_failed";
    return false;
  }

  const std::string hello =
      "NICK " + nick + "\r\n"
      "USER " + nick + " 0 * :" + sanitize_irc_text(realname, 80) + "\r\n";
  if (!write_all(fd, hello)) {
    close_fd(fd);
    fd = -1;
    error = "irc_write_failed";
    return false;
  }

  bool registered = false;
  std::string buffer;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (std::chrono::steady_clock::now() < deadline) {
    std::vector<std::string> lines;
    if (!read_irc_lines(fd, buffer, lines, 300)) break;
    for (const auto& line : lines) {
      if (!handle_irc_ping(fd, line)) break;
      if (line.find(" 001 ") != std::string::npos || line.find(" 001 " + nick + " ") != std::string::npos) {
        registered = true;
        break;
      }
      if (line.find(" 433 ") != std::string::npos) {
        close_fd(fd);
        fd = -1;
        error = "irc_nick_in_use";
        return false;
      }
    }
    if (registered) break;
  }
  if (!registered) {
    close_fd(fd);
    fd = -1;
    error = "irc_registration_timeout";
    return false;
  }
  return true;
}
#endif

}  // namespace

struct ChatGatewayService::IrcSession {
  int fd = -1;
  std::string channel_id;
  std::string channel_name;
  std::string user_id;
  std::string nick;
  std::string username;
  std::string display_name;
  std::string avatar_url;
  std::string role;
  bool ready = false;
  mutable std::mutex write_mutex;
  std::atomic<bool> stop_requested{false};
  std::thread reader;
  std::chrono::steady_clock::time_point last_seen = std::chrono::steady_clock::now();
};

ChatGatewayServiceConfig load_chat_gateway_service_config(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) throw std::runtime_error("chat_gateway_config_open_failed");
  nlohmann::json data;
  stream >> data;
  ChatGatewayServiceConfig config;
  if (data.contains("http_port")) config.http_port = static_cast<std::uint16_t>(data.at("http_port").get<int>());
  if (data.contains("listen_host")) config.listen_host = data.at("listen_host").get<std::string>();
  if (data.contains("irc_host")) config.irc_host = data.at("irc_host").get<std::string>();
  if (data.contains("irc_port")) config.irc_port = static_cast<std::uint16_t>(data.at("irc_port").get<int>());
  if (data.contains("auth_token") && data.at("auth_token").is_string()) {
    const auto token = trim(data.at("auth_token").get<std::string>());
    if (!token.empty()) config.auth_token = token;
  }
  return config;
}

nlohmann::json to_json(const ChatGatewayServiceConfig& config) {
  return {
      {"http_port", config.http_port},
      {"listen_host", config.listen_host},
      {"irc_host", config.irc_host},
      {"irc_port", config.irc_port},
      {"auth_required", config.auth_token.has_value()},
  };
}

ChatGatewayService::ChatGatewayService(ChatGatewayServiceConfig config) : config_(std::move(config)) {}

ChatGatewayService::~ChatGatewayService() {
  stop_sessions();
}

bool ChatGatewayService::authorized(const std::optional<std::string>& bearer_token) const {
  if (!config_.auth_token.has_value()) return true;
  return bearer_token.has_value() && *bearer_token == *config_.auth_token;
}

nlohmann::json ChatGatewayService::handle_channels() const {
  return {
      {"ok", true},
      {"channels", nlohmann::json::array({
                       {{"id", "global:fr"}, {"irc", "#global-fr"}, {"title", "Global FR"}},
                       {{"id", "global:en"}, {"irc", "#global-en"}, {"title", "Global EN"}},
                   })},
  };
}

nlohmann::json ChatGatewayService::handle_list_messages(const std::string& channel_id) const {
  if (!is_safe_channel_id(channel_id)) return {{"ok", false}, {"error", "invalid_channel"}};
  nlohmann::json messages = nlohmann::json::array();
  {
    std::scoped_lock lock(messages_mutex_);
    const auto found = recent_messages_.find(channel_id);
    if (found != recent_messages_.end()) {
      for (const auto& message : found->second) messages.push_back(message);
    }
  }
  return {{"ok", true}, {"channel", channel_id}, {"messages", messages}};
}

nlohmann::json ChatGatewayService::handle_list_presence(const std::string& channel_id) const {
  if (!is_safe_channel_id(channel_id)) return {{"ok", false}, {"error", "invalid_channel"}};
#ifdef _WIN32
  (void)channel_id;
  return {{"ok", false}, {"error", "unsupported_platform"}};
#else
  const auto channel = irc_channel_name(channel_id);
  const auto probe_nick = sanitize_nick("sklnames" + std::to_string(
      static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count() % 1000000)));
  int fd = -1;
  std::string error;
  if (!open_irc_connection(config_, probe_nick, "SekaiLink Presence Probe", fd, error)) {
    return {{"ok", false}, {"error", error.empty() ? "irc_presence_probe_failed" : error}};
  }

  if (!write_all(fd, "JOIN " + channel + "\r\nNAMES " + channel + "\r\n")) {
    close_fd(fd);
    return {{"ok", false}, {"error", "irc_write_failed"}};
  }

  std::unordered_map<std::string, nlohmann::json> metadata_by_nick;
  {
    std::scoped_lock lock(sessions_mutex_);
    for (const auto& [_, session] : sessions_) {
      if (!session || session->channel_id != channel_id) continue;
      metadata_by_nick[lower(session->nick)] = {
          {"user_id", session->user_id},
          {"username", session->username},
          {"display_name", session->display_name},
          {"name", session->display_name.empty() ? session->username : session->display_name},
          {"avatar_url", session->avatar_url},
          {"role", session->role},
          {"ready", session->ready},
          {"last_seen", utc_now_iso8601()},
      };
    }
  }

  nlohmann::json users = nlohmann::json::array();
  std::string buffer;
  bool done = false;
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
  while (!done && std::chrono::steady_clock::now() < deadline) {
    std::vector<std::string> lines;
    if (!read_irc_lines(fd, buffer, lines, 300)) break;
    for (const auto& line : lines) {
      if (!handle_irc_ping(fd, line)) {
        done = true;
        break;
      }
      if (line.find(" 353 ") != std::string::npos) {
        const auto names_pos = line.find(" :");
        if (names_pos == std::string::npos) continue;
        std::istringstream names(line.substr(names_pos + 2));
        std::string raw_name;
        while (names >> raw_name) {
          bool is_op = false;
          bool is_voice = false;
          while (!raw_name.empty() && (raw_name.front() == '@' || raw_name.front() == '+')) {
            if (raw_name.front() == '@') is_op = true;
            if (raw_name.front() == '+') is_voice = true;
            raw_name.erase(raw_name.begin());
          }
          if (raw_name.empty() || raw_name == probe_nick) continue;
          auto entry = metadata_by_nick.count(lower(raw_name)) ? metadata_by_nick.at(lower(raw_name)) : nlohmann::json::object();
          if (!entry.contains("user_id")) entry["user_id"] = raw_name;
          if (!entry.contains("username")) entry["username"] = raw_name;
          if (!entry.contains("display_name")) entry["display_name"] = raw_name;
          if (!entry.contains("name")) entry["name"] = entry.value("display_name", raw_name);
          entry["nick"] = raw_name;
          entry["irc_op"] = is_op;
          entry["irc_voice"] = is_voice;
          entry["status"] = "online";
          if (!entry.contains("role") || entry.value("role", std::string{}).empty()) entry["role"] = is_op ? "host" : "player";
          if (!entry.contains("ready")) entry["ready"] = is_voice;
          users.push_back(entry);
        }
      }
      if (line.find(" 366 ") != std::string::npos) {
        done = true;
        break;
      }
    }
  }
  write_all(fd, "QUIT :SekaiLink presence probe done\r\n");
  close_fd(fd);
  return {{"ok", true}, {"channel", channel_id}, {"irc", channel}, {"users", users}};
#endif
}

nlohmann::json ChatGatewayService::handle_touch_presence(const std::string& channel_id, const nlohmann::json& body) const {
  if (!is_safe_channel_id(channel_id)) return {{"ok", false}, {"error", "invalid_channel"}};
  if (!body.is_object()) return {{"ok", false}, {"error", "invalid_body"}};
  std::string error;
  if (!ensure_irc_presence(channel_id, body, error)) {
    return {{"ok", false}, {"error", error.empty() ? "irc_presence_failed" : error}};
  }
  return {{"ok", true}, {"channel", channel_id}};
}

void ChatGatewayService::remember_message(const std::string& channel_id, const std::string& author, const std::string& content) const {
  std::scoped_lock lock(messages_mutex_);
  auto& messages = recent_messages_[channel_id];
  messages.push_back({
      {"id", next_message_id_++},
      {"channel", channel_id},
      {"author", author.empty() ? "SekaiLink" : author},
      {"content", content},
      {"created_at", utc_now_iso8601()},
      {"source", "gateway"},
  });
  while (messages.size() > 100) messages.pop_front();
}

nlohmann::json ChatGatewayService::handle_send_message(const std::string& channel_id, const nlohmann::json& body) const {
  if (!is_safe_channel_id(channel_id)) return {{"ok", false}, {"error", "invalid_channel"}};
  if (!body.is_object()) return {{"ok", false}, {"error", "invalid_body"}};
  const auto author = sanitize_irc_text(body.value("author", "SekaiLink"), 80);
  const auto content = sanitize_irc_text(body.value("content", ""), 400);
  if (content.empty()) return {{"ok", false}, {"error", "empty_message"}};
  std::string error;
  const auto has_identity = body.contains("user_id") && body.at("user_id").is_string() && !trim(body.at("user_id").get<std::string>()).empty();
  const auto sent = has_identity
      ? send_persistent_irc_message(channel_id, body, error)
      : send_irc_message(channel_id, author, content, error);
  if (!sent) {
    return {{"ok", false}, {"error", error.empty() ? "irc_send_failed" : error}};
  }
  remember_message(channel_id, author, content);
  return {{"ok", true}, {"channel", channel_id}};
}

bool ChatGatewayService::ensure_irc_presence(const std::string& channel_id, const nlohmann::json& body, std::string& error) const {
#ifdef _WIN32
  (void)channel_id;
  (void)body;
  error = "unsupported_platform";
  return false;
#else
  const auto raw_user_id = sanitize_irc_text(body.value("user_id", std::string{}), 80);
  if (raw_user_id.empty()) {
    error = "missing_user_id";
    return false;
  }
  const auto username = sanitize_irc_text(body.value("username", raw_user_id), 80);
  const auto display_name = sanitize_irc_text(body.value("display_name", username), 80);
  const auto avatar_url = sanitize_irc_text(body.value("avatar_url", std::string{}), 1024);
  const auto role = sanitize_irc_text(body.value("role", std::string("player")), 32);
  const auto ready = body.value("ready", false);
  const auto nick = sanitize_nick("u" + raw_user_id);
  const auto channel = irc_channel_name(channel_id);
  const auto key = channel_id + "\n" + raw_user_id;

  {
    std::shared_ptr<IrcSession> existing;
    {
      std::scoped_lock lock(sessions_mutex_);
      const auto found = sessions_.find(key);
      if (found != sessions_.end() && found->second && found->second->fd >= 0) {
        existing = found->second;
        existing->username = username;
        existing->display_name = display_name;
        existing->avatar_url = avatar_url;
        existing->role = role;
        existing->ready = ready;
        existing->last_seen = std::chrono::steady_clock::now();
      }
    }
    if (existing) {
      {
        std::scoped_lock write_lock(existing->write_mutex);
        if (role == "host") (void)write_all(existing->fd, "MODE " + channel + " +o " + existing->nick + "\r\n");
        if (ready) (void)write_all(existing->fd, "MODE " + channel + " +v " + existing->nick + "\r\n");
      }
      apply_channel_modes(channel_id);
      return true;
    }
  }

  int fd = -1;
  if (!open_irc_connection(config_, nick, display_name.empty() ? username : display_name, fd, error)) return false;
  const std::string join = "JOIN " + channel + "\r\n";
  if (!write_all(fd, join)) {
    close_fd(fd);
    error = "irc_join_failed";
    return false;
  }
  {
    std::string join_buffer;
    const auto join_deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (std::chrono::steady_clock::now() < join_deadline) {
      std::vector<std::string> lines;
      if (!read_irc_lines(fd, join_buffer, lines, 100)) {
        close_fd(fd);
        error = "irc_join_read_failed";
        return false;
      }
      bool joined = false;
      for (const auto& line : lines) {
        if (!handle_irc_ping(fd, line)) {
          close_fd(fd);
          error = "irc_pong_failed";
          return false;
        }
        if (line.find(" JOIN " + channel) != std::string::npos ||
            line.find(" 353 ") != std::string::npos ||
            line.find(" 366 ") != std::string::npos) {
          joined = true;
        }
      }
      if (joined) break;
    }
  }

  auto session = std::make_shared<IrcSession>();
  session->fd = fd;
  session->channel_id = channel_id;
  session->channel_name = channel;
  session->user_id = raw_user_id;
  session->nick = nick;
  session->username = username;
  session->display_name = display_name;
  session->avatar_url = avatar_url;
  session->role = role;
  session->ready = ready;
  session->last_seen = std::chrono::steady_clock::now();
  session->reader = std::thread([session]() {
    std::string buffer;
    while (!session->stop_requested.load()) {
      std::vector<std::string> lines;
      if (!read_irc_lines(session->fd, buffer, lines, 500)) break;
      for (const auto& line : lines) {
        if (line.rfind("PING ", 0) == 0) {
          std::scoped_lock lock(session->write_mutex);
          if (!write_all(session->fd, "PONG " + line.substr(5) + "\r\n")) {
            session->stop_requested = true;
            break;
          }
        }
      }
    }
    close_fd(session->fd);
    session->fd = -1;
  });

  {
    std::scoped_lock lock(sessions_mutex_);
    sessions_[key] = session;
  }

  {
    std::scoped_lock write_lock(session->write_mutex);
    if (role == "host") (void)write_all(session->fd, "MODE " + channel + " +o " + session->nick + "\r\n");
    if (ready) (void)write_all(session->fd, "MODE " + channel + " +v " + session->nick + "\r\n");
  }
  apply_channel_modes(channel_id);
  return true;
#endif
}

void ChatGatewayService::apply_channel_modes(const std::string& channel_id) const {
#ifndef _WIN32
  std::shared_ptr<IrcSession> host;
  std::vector<std::shared_ptr<IrcSession>> sessions;
  {
    std::scoped_lock lock(sessions_mutex_);
    for (const auto& [_, session] : sessions_) {
      if (!session || session->channel_id != channel_id || session->fd < 0) continue;
      sessions.push_back(session);
      if (!host && session->role == "host") host = session;
    }
  }
  if (!host || host->fd < 0) return;
  std::string mode_flags = "+";
  std::string args;
  bool any = false;
  for (const auto& session : sessions) {
    if (!session || session->fd < 0) continue;
    if (session->role == "host") {
      mode_flags.push_back('o');
      args += " " + session->nick;
      any = true;
    }
    if (session->ready) {
      mode_flags.push_back('v');
      args += " " + session->nick;
      any = true;
    }
  }
  if (!any) return;
  std::scoped_lock write_lock(host->write_mutex);
  (void)write_all(host->fd, "MODE " + host->channel_name + " " + mode_flags + args + "\r\n");
#else
  (void)channel_id;
#endif
}

bool ChatGatewayService::send_persistent_irc_message(const std::string& channel_id, const nlohmann::json& body, std::string& error) const {
#ifdef _WIN32
  (void)channel_id;
  (void)body;
  error = "unsupported_platform";
  return false;
#else
  if (!ensure_irc_presence(channel_id, body, error)) return false;
  const auto raw_user_id = sanitize_irc_text(body.value("user_id", std::string{}), 80);
  const auto key = channel_id + "\n" + raw_user_id;
  const auto content = sanitize_irc_text(body.value("content", ""), 400);
  if (content.empty()) {
    error = "empty_message";
    return false;
  }
  std::shared_ptr<IrcSession> session;
  {
    std::scoped_lock lock(sessions_mutex_);
    const auto found = sessions_.find(key);
    if (found != sessions_.end()) session = found->second;
  }
  if (!session || session->fd < 0) {
    error = "irc_session_missing";
    return false;
  }
  std::scoped_lock write_lock(session->write_mutex);
  if (!write_all(session->fd, "PRIVMSG " + session->channel_name + " :" + content + "\r\n")) {
    error = "irc_write_failed";
    return false;
  }
  return true;
#endif
}

bool ChatGatewayService::send_irc_message(const std::string& channel_id, const std::string& author, const std::string& content, std::string& error) const {
#ifdef _WIN32
  (void)channel_id;
  (void)author;
  (void)content;
  error = "unsupported_platform";
  return false;
#else
  int fd = -1;
  const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
  const auto nick = sanitize_nick("sklgw" + std::to_string(static_cast<unsigned long long>(now % 1000000)));
  const auto channel = irc_channel_name(channel_id);
  const auto line_prefix = author.empty() ? std::string("SekaiLink") : author;
  if (!open_irc_connection(config_, nick, "SekaiLink Chat Gateway", fd, error)) return false;

  const std::string payload =
      "JOIN " + channel + "\r\n"
      "PRIVMSG " + channel + " :[" + sanitize_irc_text(line_prefix, 80) + "] " + content + "\r\n"
      "QUIT :SekaiLink gateway done\r\n";
  const bool ok = write_all(fd, payload);
  close_fd(fd);
  if (!ok) error = "irc_write_failed";
  return ok;
#endif
}

void ChatGatewayService::stop_sessions() const {
#ifndef _WIN32
  std::vector<std::shared_ptr<IrcSession>> sessions;
  {
    std::scoped_lock lock(sessions_mutex_);
    for (const auto& [_, session] : sessions_) sessions.push_back(session);
    sessions_.clear();
  }
  for (auto& session : sessions) {
    if (!session) continue;
    session->stop_requested = true;
    {
      std::scoped_lock write_lock(session->write_mutex);
      if (session->fd >= 0) (void)write_all(session->fd, "QUIT :SekaiLink gateway shutting down\r\n");
    }
    if (session->reader.joinable()) session->reader.join();
  }
#endif
}

nlohmann::json ChatGatewayService::handle(
    const std::string& method,
    const std::string& path,
    const std::optional<std::string>& bearer_token,
    const std::optional<nlohmann::json>& body) const {
  const auto clean_path = path_without_query(path);
  const auto parts = split_path(clean_path);
  if (method == "GET" && clean_path == "/health") {
    return {{"ok", true}, {"service", "sekailink-chat-gateway"}, {"config", to_json(config_)}};
  }
  if (!authorized(bearer_token)) return {{"ok", false}, {"error", "unauthorized"}};
  if (method == "GET" && clean_path == "/channels") return handle_channels();
  if (method == "GET" && parts.size() == 3 && parts[0] == "channels" && parts[2] == "messages") {
    return handle_list_messages(url_decode(parts[1]));
  }
  if (method == "GET" && parts.size() == 3 && parts[0] == "channels" && parts[2] == "presence") {
    return handle_list_presence(url_decode(parts[1]));
  }
  if (method == "POST" && parts.size() == 3 && parts[0] == "channels" && parts[2] == "presence") {
    return handle_touch_presence(url_decode(parts[1]), body.value_or(nlohmann::json::object()));
  }
  if (method == "POST" && parts.size() == 3 && parts[0] == "channels" && parts[2] == "messages") {
    return handle_send_message(url_decode(parts[1]), body.value_or(nlohmann::json::object()));
  }
  return {{"ok", false}, {"error", "not_found"}};
}

ChatGatewayHttpServer::ChatGatewayHttpServer(ChatGatewayServiceConfig config)
    : service_(config), config_(std::move(config)) {}

ChatGatewayHttpServer::~ChatGatewayHttpServer() {
  stop();
}

bool ChatGatewayHttpServer::start() {
#ifdef _WIN32
  return false;
#else
  listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (listen_fd_ < 0) return false;
  int reuse = 1;
  setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(config_.http_port);
  if (inet_pton(AF_INET, config_.listen_host.c_str(), &addr.sin_addr) != 1) {
    close_fd(listen_fd_);
    listen_fd_ = -1;
    return false;
  }
  if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    close_fd(listen_fd_);
    listen_fd_ = -1;
    return false;
  }
  if (listen(listen_fd_, 64) != 0) {
    close_fd(listen_fd_);
    listen_fd_ = -1;
    return false;
  }
  socket_len_t len = sizeof(addr);
  if (getsockname(listen_fd_, reinterpret_cast<sockaddr*>(&addr), &len) == 0) {
    bound_port_ = ntohs(addr.sin_port);
  } else {
    bound_port_ = config_.http_port;
  }
  return true;
#endif
}

void ChatGatewayHttpServer::stop() {
#ifndef _WIN32
  if (listen_fd_ >= 0) {
    close_fd(listen_fd_);
    listen_fd_ = -1;
  }
#endif
}

std::uint16_t ChatGatewayHttpServer::port() const {
  return bound_port_;
}

void ChatGatewayHttpServer::serve_one() const {
#ifndef _WIN32
  if (listen_fd_ < 0) return;
  if (!wait_for_read(listen_fd_, 250)) return;
  sockaddr_in client{};
  socket_len_t len = sizeof(client);
  const int fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client), &len);
  if (fd < 0) return;
  std::string request;
  char buffer[4096];
  const auto n = recv(fd, buffer, sizeof(buffer), 0);
  if (n > 0) request.assign(buffer, static_cast<std::size_t>(n));

  std::istringstream input(request);
  std::string method;
  std::string path;
  std::string version;
  input >> method >> path >> version;
  std::string line;
  std::optional<std::string> bearer;
  std::size_t content_length = 0;
  std::getline(input, line);
  while (std::getline(input, line)) {
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (line.empty()) break;
    const auto colon = line.find(':');
    if (colon == std::string::npos) continue;
    const auto key = lower(trim(line.substr(0, colon)));
    const auto value = trim(line.substr(colon + 1));
    if (key == "authorization") {
      const std::string prefix = "Bearer ";
      if (value.rfind(prefix, 0) == 0) bearer = value.substr(prefix.size());
    } else if (key == "content-length") {
      try {
        content_length = static_cast<std::size_t>(std::stoul(value));
      } catch (...) {
        content_length = 0;
      }
    }
  }
  std::string body_text;
  if (content_length > 0) {
    const auto body_pos = request.find("\r\n\r\n");
    if (body_pos != std::string::npos) body_text = request.substr(body_pos + 4);
    while (body_text.size() < content_length && body_text.size() < 65536) {
      const auto more = recv(fd, buffer, sizeof(buffer), 0);
      if (more <= 0) break;
      body_text.append(buffer, static_cast<std::size_t>(more));
    }
  }
  std::optional<nlohmann::json> body;
  if (!body_text.empty()) {
    try {
      body = nlohmann::json::parse(body_text);
    } catch (...) {
      body = nlohmann::json::object();
    }
  }
  const auto result = service_.handle(method, path, bearer, body);
  int status = 200;
  const auto error = result.value("error", "");
  if (error == "unauthorized") status = 401;
  else if (error == "not_found") status = 404;
  else if (error == "irc_connect_failed" || error == "irc_registration_timeout" || error == "irc_send_failed") status = 502;
  else if (!error.empty()) status = 400;
  const auto response = json_http_response(status, result);
  write_all(fd, response);
  close_fd(fd);
#endif
}

}  // namespace sekailink_server
