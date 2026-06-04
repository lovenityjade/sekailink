#include "sekailink_server/identity_service.hpp"

#include "sekailink_server/room_state.hpp"

#include <argon2.h>

#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
using socket_len_t = socklen_t;
#endif

#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <sqlite3.h>

#include <algorithm>
#include <chrono>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <fstream>
#include <iomanip>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <vector>

namespace sekailink_server {

namespace {

std::string http_status_text(int status_code) {
  switch (status_code) {
    case 200:
      return "OK";
    case 201:
      return "Created";
    case 400:
      return "Bad Request";
    case 401:
      return "Unauthorized";
    case 403:
      return "Forbidden";
    case 404:
      return "Not Found";
    case 405:
      return "Method Not Allowed";
    case 409:
      return "Conflict";
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

std::string trim(std::string value) {
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front())) != 0) {
    value.erase(value.begin());
  }
  while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back())) != 0) {
    value.pop_back();
  }
  return value;
}

std::vector<unsigned char> random_bytes(std::size_t size);
std::string base64_encode(const std::vector<unsigned char>& bytes);

std::optional<std::string> normalize_optional_header(std::optional<std::string> value) {
  if (!value.has_value()) {
    return std::nullopt;
  }
  auto trimmed = trim(std::move(*value));
  if (trimmed.empty()) {
    return std::nullopt;
  }
  return trimmed;
}

void bind_optional_text(sqlite3_stmt* stmt, int index, const std::optional<std::string>& value) {
  if (!value.has_value()) {
    sqlite3_bind_null(stmt, index);
    return;
  }
  sqlite3_bind_text(stmt, index, value->c_str(), -1, SQLITE_TRANSIENT);
}

std::optional<std::string> column_optional_text(sqlite3_stmt* stmt, int index) {
  if (sqlite3_column_type(stmt, index) == SQLITE_NULL) {
    return std::nullopt;
  }
  return std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, index)));
}

nlohmann::json request_context_to_json(const IdentityRequestContext& context) {
  nlohmann::json json = nlohmann::json::object();
  if (context.user_agent.has_value()) {
    json["user_agent"] = *context.user_agent;
  }
  if (context.client_name.has_value()) {
    json["client_name"] = *context.client_name;
  }
  if (context.client_version.has_value()) {
    json["client_version"] = *context.client_version;
  }
  if (context.device_id.has_value()) {
    json["device_id"] = *context.device_id;
  }
  if (context.locale.has_value()) {
    json["requested_locale"] = *context.locale;
  }
  return json;
}

nlohmann::json admin_request_context_to_json(const IdentityRequestContext& context) {
  return {
      {"actor_type", "admin_token"},
      {"request_context", request_context_to_json(context)},
  };
}

std::string session_state_for_record(const IdentityStore::SessionRecord& session) {
  if (session.revoked_at.has_value()) {
    return "revoked";
  }
  if (session.expires_at < utc_timestamp_now()) {
    return "expired";
  }
  return "active";
}

std::string session_device_key(const IdentityStore::SessionRecord& session) {
  if (session.device_id.has_value() && !session.device_id->empty()) {
    return *session.device_id;
  }
  if (session.client_name.has_value() && !session.client_name->empty()) {
    return "client:" + *session.client_name;
  }
  if (session.user_agent.has_value() && !session.user_agent->empty()) {
    return "ua:" + *session.user_agent;
  }
  return "session:" + std::to_string(session.session_id);
}

std::string session_device_name(const IdentityStore::SessionRecord& session) {
  if (session.device_id.has_value() && !session.device_id->empty()) {
    return *session.device_id;
  }
  if (session.client_name.has_value() && !session.client_name->empty()) {
    if (session.client_version.has_value() && !session.client_version->empty()) {
      return *session.client_name + " " + *session.client_version;
    }
    return *session.client_name;
  }
  if (session.user_agent.has_value() && !session.user_agent->empty()) {
    return *session.user_agent;
  }
  return "Unknown device";
}

nlohmann::json build_session_inventory_json(
    const std::vector<IdentityStore::SessionRecord>& sessions,
    const std::optional<std::int64_t>& current_session_id = std::nullopt) {
  struct DeviceAggregate {
    std::string key;
    std::string display_name;
    std::size_t session_count = 0;
    std::size_t active_session_count = 0;
    bool current = false;
    std::optional<std::string> latest_created_at;
    std::vector<std::string> client_names;
    std::vector<std::string> locales;
  };

  auto append_unique = [](std::vector<std::string>& values, const std::optional<std::string>& candidate) {
    if (!candidate.has_value() || candidate->empty()) {
      return;
    }
    if (std::find(values.begin(), values.end(), *candidate) == values.end()) {
      values.push_back(*candidate);
    }
  };

  std::size_t active_sessions = 0;
  std::size_t revoked_sessions = 0;
  std::size_t expired_sessions = 0;
  std::map<std::string, DeviceAggregate> devices;
  for (const auto& session : sessions) {
    const auto state = session_state_for_record(session);
    if (state == "active") {
      ++active_sessions;
    } else if (state == "revoked") {
      ++revoked_sessions;
    } else if (state == "expired") {
      ++expired_sessions;
    }

    const auto device_key = session_device_key(session);
    auto& device = devices[device_key];
    device.key = device_key;
    device.display_name = session_device_name(session);
    ++device.session_count;
    if (state == "active") {
      ++device.active_session_count;
    }
    if (current_session_id.has_value() && session.session_id == *current_session_id) {
      device.current = true;
    }
    if (!device.latest_created_at.has_value() || session.created_at > *device.latest_created_at) {
      device.latest_created_at = session.created_at;
    }
    append_unique(device.client_names, session.client_name);
    append_unique(device.locales, session.requested_locale);
  }

  auto devices_json = nlohmann::json::array();
  for (auto& [_, device] : devices) {
    devices_json.push_back({
        {"device_key", device.key},
        {"display_name", device.display_name},
        {"session_count", device.session_count},
        {"active_session_count", device.active_session_count},
        {"current", device.current},
        {"latest_created_at", device.latest_created_at.has_value() ? nlohmann::json(*device.latest_created_at) : nlohmann::json(nullptr)},
        {"client_names", device.client_names},
        {"requested_locales", device.locales},
    });
  }

  return {
      {"total_sessions", sessions.size()},
      {"active_sessions", active_sessions},
      {"revoked_sessions", revoked_sessions},
      {"expired_sessions", expired_sessions},
      {"device_count", devices.size()},
      {"devices", std::move(devices_json)},
  };
}

std::vector<std::string> parse_permissions_json(const std::string& raw) {
  std::vector<std::string> permissions;
  if (raw.empty()) {
    return permissions;
  }
  try {
    const auto json = nlohmann::json::parse(raw);
    if (!json.is_array()) {
      return permissions;
    }
    for (const auto& item : json) {
      if (item.is_string()) {
        const auto value = trim(item.get<std::string>());
        if (!value.empty()) {
          permissions.push_back(value);
        }
      }
    }
  } catch (...) {
  }
  return permissions;
}

std::string serialize_permissions_json(const std::vector<std::string>& permissions) {
  nlohmann::json json = nlohmann::json::array();
  for (const auto& permission : permissions) {
    if (!permission.empty()) {
      json.push_back(permission);
    }
  }
  return json.dump();
}

std::vector<std::string> parse_string_array_json(const std::string& raw) {
  std::vector<std::string> values;
  if (raw.empty()) {
    return values;
  }
  try {
    const auto json = nlohmann::json::parse(raw);
    if (!json.is_array()) {
      return values;
    }
    for (const auto& item : json) {
      if (!item.is_string()) {
        continue;
      }
      auto value = trim(item.get<std::string>());
      if (value.empty()) {
        continue;
      }
      values.push_back(std::move(value));
    }
  } catch (...) {
  }
  return values;
}

std::string serialize_string_array_json(const std::vector<std::string>& values) {
  nlohmann::json json = nlohmann::json::array();
  for (const auto& value : values) {
    if (!value.empty()) {
      json.push_back(value);
    }
  }
  return json.dump();
}

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::string upper(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  return value;
}

std::optional<std::string> normalize_game_key_code(std::string value) {
  value = upper(trim(std::move(value)));
  std::string compact;
  compact.reserve(value.size());
  for (char c : value) {
    if (c == '-' || std::isspace(static_cast<unsigned char>(c)) != 0) {
      continue;
    }
    if ((c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')) {
      compact.push_back(c);
      continue;
    }
    return std::nullopt;
  }
  if (compact.size() != 16) {
    return std::nullopt;
  }
  std::string normalized;
  normalized.reserve(19);
  for (std::size_t index = 0; index < compact.size(); ++index) {
    if (index > 0 && index % 4 == 0) {
      normalized.push_back('-');
    }
    normalized.push_back(compact[index]);
  }
  return normalized;
}

std::string normalize_game_key_status(std::string status) {
  status = lower(trim(std::move(status)));
  if (status == "activated" || status == "free" || status == "deactivated") {
    return status;
  }
  throw std::runtime_error("invalid_game_key_status");
}

std::vector<std::string> normalize_entitlements(std::vector<std::string> entitlements) {
  std::vector<std::string> normalized;
  std::set<std::string> seen;
  for (auto& entitlement : entitlements) {
    entitlement = lower(trim(std::move(entitlement)));
    if (entitlement.empty() || !seen.insert(entitlement).second) {
      continue;
    }
    normalized.push_back(std::move(entitlement));
  }
  return normalized;
}

std::string random_game_key_code() {
  static constexpr char kAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  const auto bytes = random_bytes(16);
  std::string compact;
  compact.reserve(16);
  for (unsigned char byte : bytes) {
    compact.push_back(kAlphabet[byte % (sizeof(kAlphabet) - 1)]);
  }
  std::string formatted;
  formatted.reserve(19);
  for (std::size_t index = 0; index < compact.size(); ++index) {
    if (index > 0 && index % 4 == 0) {
      formatted.push_back('-');
    }
    formatted.push_back(compact[index]);
  }
  return formatted;
}

std::string percent_encode(std::string_view value) {
  std::ostringstream encoded;
  encoded << std::uppercase << std::hex;
  for (const unsigned char c : value) {
    if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' ||
        c == '~') {
      encoded << static_cast<char>(c);
    } else {
      encoded << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
  }
  return encoded.str();
}

std::string base64url_encode(const std::vector<unsigned char>& bytes) {
  auto encoded = base64_encode(bytes);
  std::replace(encoded.begin(), encoded.end(), '+', '-');
  std::replace(encoded.begin(), encoded.end(), '/', '_');
  while (!encoded.empty() && encoded.back() == '=') {
    encoded.pop_back();
  }
  return encoded;
}

std::string random_token_urlsafe(std::size_t size_bytes) {
  return base64url_encode(random_bytes(size_bytes));
}

struct SimpleHttpResponse {
  int status_code = 0;
  std::map<std::string, std::string> headers;
  std::string body;
};

std::string sqlite_quoted(const std::string& value) {
  std::string quoted;
  quoted.reserve(value.size() + 2);
  for (const char c : value) {
    quoted.push_back(c);
    if (c == '\'') {
      quoted.push_back('\'');
    }
  }
  return quoted;
}

SimpleHttpResponse https_request(
    const std::string& host,
    const std::string& target,
    const std::string& method,
    const std::map<std::string, std::string>& headers,
    const std::string& body) {
  SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
  if (ctx == nullptr) {
    throw std::runtime_error("identity_https_ctx_failed");
  }
  SSL_CTX_set_default_verify_paths(ctx);

  BIO* bio = BIO_new_ssl_connect(ctx);
  if (bio == nullptr) {
    SSL_CTX_free(ctx);
    throw std::runtime_error("identity_https_bio_failed");
  }

  std::string host_and_port = host + ":443";
  BIO_set_conn_hostname(bio, host_and_port.c_str());

  SSL* ssl = nullptr;
  BIO_get_ssl(bio, &ssl);
  if (ssl == nullptr) {
    BIO_free_all(bio);
    SSL_CTX_free(ctx);
    throw std::runtime_error("identity_https_ssl_missing");
  }
  SSL_set_tlsext_host_name(ssl, host.c_str());

  if (BIO_do_connect(bio) <= 0 || BIO_do_handshake(bio) <= 0) {
    BIO_free_all(bio);
    SSL_CTX_free(ctx);
    throw std::runtime_error("identity_https_connect_failed");
  }

  std::ostringstream request;
  request << method << " " << target << " HTTP/1.1\r\n";
  request << "Host: " << host << "\r\n";
  request << "User-Agent: SekaiLink/1.0\r\n";
  request << "Accept: application/json\r\n";
  for (const auto& [name, value] : headers) {
    request << name << ": " << value << "\r\n";
  }
  if (!body.empty()) {
    request << "Content-Length: " << body.size() << "\r\n";
  }
  request << "Connection: close\r\n\r\n";
  request << body;

  const auto request_payload = request.str();
  if (BIO_write(bio, request_payload.data(), static_cast<int>(request_payload.size())) <= 0) {
    BIO_free_all(bio);
    SSL_CTX_free(ctx);
    throw std::runtime_error("identity_https_write_failed");
  }
  (void) BIO_flush(bio);

  std::string response_raw;
  char buffer[4096];
  while (true) {
    const auto read = BIO_read(bio, buffer, static_cast<int>(sizeof(buffer)));
    if (read > 0) {
      response_raw.append(buffer, static_cast<std::size_t>(read));
      continue;
    }
    if (read == 0) {
      break;
    }
    if (!BIO_should_retry(bio)) {
      break;
    }
  }

  BIO_free_all(bio);
  SSL_CTX_free(ctx);

  const auto header_end = response_raw.find("\r\n\r\n");
  if (header_end == std::string::npos) {
    throw std::runtime_error("identity_https_invalid_response");
  }
  const auto header_block = response_raw.substr(0, header_end);
  const auto body_block = response_raw.substr(header_end + 4);

  std::istringstream header_stream(header_block);
  std::string status_line;
  std::getline(header_stream, status_line);
  if (!status_line.empty() && status_line.back() == '\r') {
    status_line.pop_back();
  }
  std::istringstream status_stream(status_line);
  std::string http_version;
  int status_code = 0;
  status_stream >> http_version >> status_code;
  if (status_code <= 0) {
    throw std::runtime_error("identity_https_invalid_status");
  }

  std::map<std::string, std::string> response_headers;
  std::string line;
  while (std::getline(header_stream, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    const auto separator = line.find(':');
    if (separator == std::string::npos) {
      continue;
    }
    response_headers.emplace(lower(trim(line.substr(0, separator))), trim(line.substr(separator + 1)));
  }

  return {
      .status_code = status_code,
      .headers = std::move(response_headers),
      .body = body_block,
  };
}

std::optional<std::string> json_optional_string(const nlohmann::json& json, const char* key) {
  if (!json.contains(key) || json.at(key).is_null()) {
    return std::nullopt;
  }
  const auto value = trim(json.at(key).get<std::string>());
  if (value.empty()) {
    return std::nullopt;
  }
  return value;
}

struct PatreonSupportInfo {
  std::optional<std::string> patreon_id;
  std::optional<std::string> patreon_email;
  std::optional<std::string> patreon_member_id;
  std::optional<std::string> patreon_status;
  std::optional<std::string> patreon_tier;
  bool patreon_is_supporter = false;
  nlohmann::json raw;
};

std::string form_urlencode(const std::map<std::string, std::string>& fields) {
  std::ostringstream encoded;
  bool first = true;
  for (const auto& [key, value] : fields) {
    if (!first) {
      encoded << '&';
    }
    first = false;
    encoded << percent_encode(key) << '=' << percent_encode(value);
  }
  return encoded.str();
}

PatreonSupportInfo extract_patreon_support_info(const nlohmann::json& payload) {
  PatreonSupportInfo info;
  info.raw = payload;

  const auto& data = payload.contains("data") ? payload.at("data") : nlohmann::json::object();
  const auto& included = payload.contains("included") ? payload.at("included") : nlohmann::json::array();
  info.patreon_id = json_optional_string(data, "id");
  if (data.contains("attributes") && data.at("attributes").is_object()) {
    info.patreon_email = json_optional_string(data.at("attributes"), "email");
  }

  std::set<std::string> membership_ids;
  if (data.contains("relationships") && data.at("relationships").is_object()) {
    const auto& relationships = data.at("relationships");
    if (relationships.contains("memberships") && relationships.at("memberships").is_object()) {
      const auto& membership_data = relationships.at("memberships").value("data", nlohmann::json::array());
      if (membership_data.is_array()) {
        for (const auto& item : membership_data) {
          if (item.is_object()) {
            if (const auto id = json_optional_string(item, "id"); id.has_value()) {
              membership_ids.insert(*id);
            }
          }
        }
      }
    }
  }

  nlohmann::json member = nlohmann::json::object();
  for (const auto& item : included) {
    if (!item.is_object()) {
      continue;
    }
    const auto type = json_optional_string(item, "type");
    const auto id = json_optional_string(item, "id");
    if (type.has_value() && id.has_value() && (*type == "member" || *type == "members") && membership_ids.contains(*id)) {
      member = item;
      break;
    }
  }

  std::optional<std::string> last_charge_status;
  if (!member.empty()) {
    info.patreon_member_id = json_optional_string(member, "id");
    if (member.contains("attributes") && member.at("attributes").is_object()) {
      info.patreon_status = json_optional_string(member.at("attributes"), "patron_status");
      last_charge_status = json_optional_string(member.at("attributes"), "last_charge_status");
    }
    std::set<std::string> tier_ids;
    if (member.contains("relationships") && member.at("relationships").is_object()) {
      const auto& relationships = member.at("relationships");
      if (relationships.contains("currently_entitled_tiers") && relationships.at("currently_entitled_tiers").is_object()) {
        const auto& tier_data = relationships.at("currently_entitled_tiers").value("data", nlohmann::json::array());
        if (tier_data.is_array()) {
          for (const auto& item : tier_data) {
            if (item.is_object()) {
              if (const auto id = json_optional_string(item, "id"); id.has_value()) {
                tier_ids.insert(*id);
              }
            }
          }
        }
      }
    }
    for (const auto& item : included) {
      if (!item.is_object()) {
        continue;
      }
      const auto type = json_optional_string(item, "type");
      const auto id = json_optional_string(item, "id");
      if (type.has_value() && id.has_value() && *type == "tier" && tier_ids.contains(*id) && item.contains("attributes") &&
          item.at("attributes").is_object()) {
        info.patreon_tier = json_optional_string(item.at("attributes"), "title");
        break;
      }
    }
  }

  const auto patron_active = info.patreon_status.has_value() && *info.patreon_status == "active_patron";
  const auto charge_ok = !last_charge_status.has_value() || *last_charge_status == "Paid" || *last_charge_status == "paid" ||
                         *last_charge_status == "successful";
  info.patreon_is_supporter = patron_active && charge_ok;
  return info;
}

IdentityStore::LoadedUser loaded_user_from_stmt(sqlite3_stmt* stmt, int offset = 0) {
  return {
      .user_id = sqlite3_column_int64(stmt, offset + 0),
      .username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 1)),
      .email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 2)),
      .email_verified = sqlite3_column_int(stmt, offset + 3) != 0,
      .display_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 4)),
      .avatar_url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 5)),
      .bio = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 6)),
      .locale = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 7)),
      .password_salt_b64 = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 8)),
      .password_hash_b64 = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 9)),
      .password_iterations = static_cast<std::uint32_t>(sqlite3_column_int(stmt, offset + 10)),
      .two_factor_enabled = sqlite3_column_int(stmt, offset + 11) != 0,
      .two_factor_secret_b32 = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 12)),
      .role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 13)),
      .permissions = parse_permissions_json(reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 14))),
      .disabled_at = column_optional_text(stmt, offset + 15),
      .patreon_id = column_optional_text(stmt, offset + 16),
      .patreon_email = column_optional_text(stmt, offset + 17),
      .patreon_member_id = column_optional_text(stmt, offset + 18),
      .patreon_status = column_optional_text(stmt, offset + 19),
      .patreon_tier = column_optional_text(stmt, offset + 20),
      .patreon_is_supporter = sqlite3_column_int(stmt, offset + 21) != 0,
      .patreon_linked_at = column_optional_text(stmt, offset + 22),
  };
}

IdentityStore::RegisteredUser registered_user_from_stmt(sqlite3_stmt* stmt, int offset = 0) {
  return {
      .user_id = sqlite3_column_int64(stmt, offset + 0),
      .username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 1)),
      .email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 2)),
      .email_verified = sqlite3_column_int(stmt, offset + 3) != 0,
      .display_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 4)),
      .avatar_url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 5)),
      .bio = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 6)),
      .locale = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 7)),
      .role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 8)),
      .permissions = parse_permissions_json(reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 9))),
      .disabled_at = column_optional_text(stmt, offset + 10),
      .patreon_tier = column_optional_text(stmt, offset + 11),
      .patreon_is_supporter = sqlite3_column_int(stmt, offset + 12) != 0,
      .patreon_linked_at = column_optional_text(stmt, offset + 13),
  };
}

IdentityStore::SessionRecord session_record_from_stmt(sqlite3_stmt* stmt, int offset = 0) {
  return {
      .session_id = sqlite3_column_int64(stmt, offset + 0),
      .session_token = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 1)),
      .user_id = sqlite3_column_int64(stmt, offset + 2),
      .username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 3)),
      .email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 4)),
      .email_verified = sqlite3_column_int(stmt, offset + 5) != 0,
      .display_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 6)),
      .avatar_url = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 7)),
      .bio = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 8)),
      .locale = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 9)),
      .created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 10)),
      .expires_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 11)),
      .revoked_at = column_optional_text(stmt, offset + 12),
      .user_agent = column_optional_text(stmt, offset + 13),
      .client_name = column_optional_text(stmt, offset + 14),
      .client_version = column_optional_text(stmt, offset + 15),
      .device_id = column_optional_text(stmt, offset + 16),
      .requested_locale = column_optional_text(stmt, offset + 17),
      .role = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 18)),
      .permissions = parse_permissions_json(reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 19))),
      .patreon_tier = column_optional_text(stmt, offset + 20),
      .patreon_is_supporter = sqlite3_column_int(stmt, offset + 21) != 0,
  };
}

IdentityStore::GameKeyRecord game_key_record_from_stmt(sqlite3_stmt* stmt, int offset = 0) {
  return {
      .key_id = sqlite3_column_int64(stmt, offset + 0),
      .key_code = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 1)),
      .status = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 2)),
      .entitlements = parse_string_array_json(reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 3))),
      .created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 4)),
      .activated_at = column_optional_text(stmt, offset + 5),
      .deactivated_at = column_optional_text(stmt, offset + 6),
      .bound_user_id = sqlite3_column_type(stmt, offset + 7) == SQLITE_NULL ? std::nullopt
                                                                            : std::optional<std::int64_t>(sqlite3_column_int64(stmt, offset + 7)),
      .bound_username = column_optional_text(stmt, offset + 8),
      .notes = sqlite3_column_type(stmt, offset + 9) == SQLITE_NULL
                   ? std::string()
                   : std::string(reinterpret_cast<const char*>(sqlite3_column_text(stmt, offset + 9))),
  };
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

std::size_t normalized_limit(const nlohmann::json& query, std::size_t fallback = 100, std::size_t max_value = 500) {
  if (!query.contains("limit") || !query.at("limit").is_string()) {
    return fallback;
  }
  try {
    const auto parsed = std::stoll(trim(query.at("limit").get<std::string>()));
    if (parsed <= 0) {
      return fallback;
    }
    return static_cast<std::size_t>(std::min<std::size_t>(static_cast<std::size_t>(parsed), max_value));
  } catch (...) {
    return fallback;
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

std::optional<std::string> normalized_query_text(const nlohmann::json& query) {
  if (!query.contains("query") || !query.at("query").is_string()) {
    return std::nullopt;
  }
  auto value = trim(query.at("query").get<std::string>());
  if (value.empty()) {
    return std::nullopt;
  }
  return value;
}

std::optional<std::string> normalized_query_value(const nlohmann::json& query, const char* key) {
  if (!query.contains(key) || !query.at(key).is_string()) {
    return std::nullopt;
  }
  auto value = lower(trim(query.at(key).get<std::string>()));
  if (value.empty()) {
    return std::nullopt;
  }
  return value;
}

std::optional<bool> normalized_state_filter(const nlohmann::json& query) {
  const auto state = normalized_query_value(query, "state");
  if (!state.has_value()) {
    return std::nullopt;
  }
  if (*state == "disabled" || *state == "true") {
    return true;
  }
  if (*state == "active" || *state == "enabled" || *state == "false") {
    return false;
  }
  return std::nullopt;
}

std::string canonical_mail_locale(std::string locale) {
  locale = lower(trim(std::move(locale)));
  if (locale.empty()) {
    return "en";
  }
  if (locale == "fr" || locale.rfind("fr-", 0) == 0) {
    return "fr";
  }
  if (locale == "es" || locale.rfind("es-", 0) == 0) {
    return "es";
  }
  if (locale == "ja" || locale == "jp" || locale.rfind("ja-", 0) == 0 || locale.rfind("jp-", 0) == 0) {
    return "ja";
  }
  if (locale == "zh" || locale == "cn" || locale == "zh-cn" || locale == "zh-hans" || locale.rfind("zh-hans", 0) == 0 ||
      locale.rfind("zh-cn", 0) == 0) {
    return "zh-Hans";
  }
  if (locale == "zh-tw" || locale == "zh-hant" || locale == "zh-hk" || locale.rfind("zh-hant", 0) == 0 ||
      locale.rfind("zh-tw", 0) == 0 || locale.rfind("zh-hk", 0) == 0 || locale == "zhc") {
    return "zh-Hant";
  }
  return "en";
}

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

std::vector<unsigned char> random_bytes(std::size_t size) {
  std::vector<unsigned char> bytes(size);
  if (RAND_bytes(bytes.data(), static_cast<int>(bytes.size())) != 1) {
    throw std::runtime_error("identity_random_bytes_failed");
  }
  return bytes;
}

std::string base64_encode(const std::vector<unsigned char>& bytes) {
  if (bytes.empty()) {
    return {};
  }
  std::string out(((bytes.size() + 2) / 3) * 4, '\0');
  const auto written = EVP_EncodeBlock(
      reinterpret_cast<unsigned char*>(out.data()),
      bytes.data(),
      static_cast<int>(bytes.size()));
  if (written < 0) {
    throw std::runtime_error("identity_base64_encode_failed");
  }
  out.resize(static_cast<std::size_t>(written));
  return out;
}

bool contains_non_ascii(const std::string& value) {
  return std::any_of(value.begin(), value.end(), [](unsigned char c) { return c > 0x7f; });
}

std::string mime_encode_utf8_header(const std::string& value) {
  if (value.empty() || !contains_non_ascii(value)) {
    return value;
  }
  const std::vector<unsigned char> bytes(value.begin(), value.end());
  return "=?UTF-8?B?" + base64_encode(bytes) + "?=";
}

struct LocalizedEmailTemplate {
  std::string locale;
  std::string subject;
  std::string intro;
  std::string action_label;
  std::string expires_label;
  std::string footer;
};

LocalizedEmailTemplate password_reset_template_for_locale(const std::string& raw_locale) {
  const auto locale = canonical_mail_locale(raw_locale);
  if (locale == "fr") {
    return {"fr", "Reinitialisation du mot de passe SekaiLink",
            "Une reinitialisation du mot de passe a ete demandee pour votre compte SekaiLink.",
            "Lien de reinitialisation", "Expiration",
            "Si vous n'etes pas a l'origine de cette demande, vous pouvez ignorer ce courriel."};
  }
  if (locale == "es") {
    return {"es", "Restablecimiento de contrasena de SekaiLink",
            "Se solicito un restablecimiento de contrasena para tu cuenta de SekaiLink.",
            "Enlace de restablecimiento", "Caduca el",
            "Si no solicitaste este cambio, puedes ignorar este correo."};
  }
  if (locale == "ja") {
    return {"ja", "SekaiLink パスワード再設定",
            "SekaiLinkアカウントのパスワード再設定が要求されました。",
            "再設定リンク", "有効期限",
            "この操作に心当たりがない場合は、このメールを無視してください。"};
  }
  if (locale == "zh-Hans") {
    return {"zh-Hans", "SekaiLink 密码重置",
            "已为你的 SekaiLink 账户请求密码重置。",
            "重置链接", "过期时间",
            "如果这不是你本人发起的操作，可以忽略这封邮件。"};
  }
  if (locale == "zh-Hant") {
    return {"zh-Hant", "SekaiLink 密碼重設",
            "已為你的 SekaiLink 帳戶提出密碼重設請求。",
            "重設連結", "到期時間",
            "如果這不是你本人發起的操作，可以忽略這封郵件。"};
  }
  return {"en", "SekaiLink password reset",
          "A password reset was requested for your SekaiLink account.",
          "Reset link", "Expires at",
          "If you did not request this change, you can ignore this email."};
}

LocalizedEmailTemplate email_verification_template_for_locale(const std::string& raw_locale) {
  const auto locale = canonical_mail_locale(raw_locale);
  if (locale == "fr") {
    return {"fr", "Verifiez votre adresse courriel SekaiLink",
            "Bienvenue sur SekaiLink.",
            "Lien de verification", "Expiration",
            "Si vous n'avez pas cree ce compte, vous pouvez ignorer ce courriel."};
  }
  if (locale == "es") {
    return {"es", "Verifica tu correo de SekaiLink",
            "Bienvenido a SekaiLink.",
            "Enlace de verificacion", "Caduca el",
            "Si no creaste esta cuenta, puedes ignorar este correo."};
  }
  if (locale == "ja") {
    return {"ja", "SekaiLink メール確認",
            "SekaiLinkへようこそ。",
            "確認リンク", "有効期限",
            "このアカウントを作成していない場合は、このメールを無視してください。"};
  }
  if (locale == "zh-Hans") {
    return {"zh-Hans", "验证你的 SekaiLink 邮箱",
            "欢迎来到 SekaiLink。",
            "验证链接", "过期时间",
            "如果这不是你创建的账户，可以忽略这封邮件。"};
  }
  if (locale == "zh-Hant") {
    return {"zh-Hant", "驗證你的 SekaiLink 電子郵件",
            "歡迎來到 SekaiLink。",
            "驗證連結", "到期時間",
            "如果這不是你建立的帳戶，可以忽略這封郵件。"};
  }
  return {"en", "Verify your SekaiLink email",
          "Welcome to SekaiLink.",
          "Verification link", "Expires at",
          "If you did not create this account, you can ignore this email."};
}

std::vector<unsigned char> base64_decode(const std::string& value) {
  if (value.empty()) {
    return {};
  }
  std::vector<unsigned char> out(value.size(), 0);
  const auto written = EVP_DecodeBlock(out.data(), reinterpret_cast<const unsigned char*>(value.data()), static_cast<int>(value.size()));
  if (written < 0) {
    throw std::runtime_error("identity_base64_decode_failed");
  }
  auto decoded_size = static_cast<std::size_t>(written);
  if (!value.empty() && value.back() == '=') {
    --decoded_size;
  }
  if (value.size() > 1 && value[value.size() - 2] == '=') {
    --decoded_size;
  }
  out.resize(decoded_size);
  return out;
}

std::string random_token_b64(std::size_t size_bytes) {
  return base64_encode(random_bytes(size_bytes));
}

std::string hash_password_argon2id_encoded(
    const std::string& password,
    const std::string& salt_b64,
    std::uint32_t time_cost,
    std::uint32_t memory_kib,
    std::uint32_t parallelism,
    std::uint32_t hash_length) {
  const auto salt = base64_decode(salt_b64);
  const auto encoded_length = argon2_encodedlen(
      time_cost,
      memory_kib,
      parallelism,
      static_cast<std::uint32_t>(salt.size()),
      hash_length,
      Argon2_id);
  std::string encoded(encoded_length, '\0');
  const auto result = ::argon2id_hash_encoded(
      time_cost,
      memory_kib,
      parallelism,
      password.data(),
      password.size(),
      salt.data(),
      salt.size(),
      hash_length,
      encoded.data(),
      encoded.size());
  if (result != ARGON2_OK) {
    throw std::runtime_error(std::string("identity_argon2_hash_failed: ") + argon2_error_message(result));
  }
  encoded.resize(std::strlen(encoded.c_str()));
  return encoded;
}

bool verify_argon2id_encoded(const std::string& encoded_hash, const std::string& password) {
  const auto result = argon2id_verify(encoded_hash.c_str(), password.data(), password.size());
  if (result == ARGON2_OK) {
    return true;
  }
  if (result == ARGON2_VERIFY_MISMATCH) {
    return false;
  }
  throw std::runtime_error(std::string("identity_argon2_verify_failed: ") + argon2_error_message(result));
}

std::string sha256_b64(const std::string& value) {
  std::vector<unsigned char> digest(SHA256_DIGEST_LENGTH, 0);
  SHA256(reinterpret_cast<const unsigned char*>(value.data()), value.size(), digest.data());
  return base64_encode(digest);
}

std::string random_recovery_code() {
  static constexpr char kAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
  const auto bytes = random_bytes(10);
  std::string code;
  code.reserve(11);
  for (std::size_t i = 0; i < 10; ++i) {
    if (i == 5) {
      code.push_back('-');
    }
    code.push_back(kAlphabet[bytes[i] % (sizeof(kAlphabet) - 1)]);
  }
  return code;
}

std::string base32_encode(const std::vector<unsigned char>& bytes) {
  static constexpr char kAlphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
  if (bytes.empty()) {
    return {};
  }
  std::string output;
  int buffer = bytes[0];
  int next = 1;
  int bits_left = 8;
  while (bits_left > 0 || next < static_cast<int>(bytes.size())) {
    if (bits_left < 5) {
      if (next < static_cast<int>(bytes.size())) {
        buffer <<= 8;
        buffer |= bytes[next++] & 0xff;
        bits_left += 8;
      } else {
        const int pad = 5 - bits_left;
        buffer <<= pad;
        bits_left += pad;
      }
    }
    const auto index = 0x1f & (buffer >> (bits_left - 5));
    bits_left -= 5;
    output.push_back(kAlphabet[index]);
  }
  return output;
}

std::vector<unsigned char> base32_decode(const std::string& value) {
  auto decode_char = [](char c) -> int {
    if (c >= 'A' && c <= 'Z') {
      return c - 'A';
    }
    if (c >= '2' && c <= '7') {
      return c - '2' + 26;
    }
    return -1;
  };

  std::vector<unsigned char> output;
  int buffer = 0;
  int bits_left = 0;
  for (char c : value) {
    if (c == '=' || std::isspace(static_cast<unsigned char>(c)) != 0) {
      continue;
    }
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    const auto decoded = decode_char(c);
    if (decoded < 0) {
      throw std::runtime_error("identity_base32_decode_failed");
    }
    buffer <<= 5;
    buffer |= decoded & 0x1f;
    bits_left += 5;
    if (bits_left >= 8) {
      output.push_back(static_cast<unsigned char>((buffer >> (bits_left - 8)) & 0xff));
      bits_left -= 8;
    }
  }
  return output;
}

std::string random_two_factor_secret_b32() {
  return base32_encode(random_bytes(20));
}

std::string hotp_code(const std::string& secret_b32, std::uint64_t counter) {
  const auto key = base32_decode(secret_b32);
  unsigned char message[8];
  for (int i = 7; i >= 0; --i) {
    message[i] = static_cast<unsigned char>(counter & 0xff);
    counter >>= 8;
  }
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len = 0;
  if (HMAC(EVP_sha1(), key.data(), static_cast<int>(key.size()), message, sizeof(message), digest, &digest_len) == nullptr) {
    throw std::runtime_error("identity_hotp_failed");
  }
  const auto offset = digest[digest_len - 1] & 0x0f;
  const std::uint32_t binary = ((digest[offset] & 0x7f) << 24) | ((digest[offset + 1] & 0xff) << 16) |
                               ((digest[offset + 2] & 0xff) << 8) | (digest[offset + 3] & 0xff);
  const auto code = binary % 1000000;
  std::ostringstream out;
  out << std::setw(6) << std::setfill('0') << code;
  return out.str();
}

bool verify_totp_code(const std::string& secret_b32, std::string code) {
  code = trim(std::move(code));
  if (code.size() != 6 || !std::all_of(code.begin(), code.end(), [](unsigned char c) { return std::isdigit(c) != 0; })) {
    return false;
  }
  const auto counter = static_cast<std::uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) / 30);
  for (std::int64_t delta = -1; delta <= 1; ++delta) {
    if (hotp_code(secret_b32, static_cast<std::uint64_t>(static_cast<std::int64_t>(counter) + delta)) == code) {
      return true;
    }
  }
  return false;
}

std::string compute_future_utc(std::uint32_t ttl_seconds) {
  const auto now = std::chrono::system_clock::now() + std::chrono::seconds(ttl_seconds);
  const auto tt = std::chrono::system_clock::to_time_t(now);
  std::tm utc_tm{};
#if defined(_WIN32)
  gmtime_s(&utc_tm, &tt);
#else
  gmtime_r(&tt, &utc_tm);
#endif
  std::ostringstream out;
  out << std::put_time(&utc_tm, "%Y-%m-%dT%H:%M:%SZ");
  return out.str();
}

std::string sqlite_error(sqlite3* db, const char* fallback) {
  const auto* message = sqlite3_errmsg(db);
  if (message == nullptr || *message == '\0') {
    return fallback;
  }
  return message;
}

std::string required_string(const nlohmann::json& body, const char* key) {
  if (!body.contains(key) || !body.at(key).is_string()) {
    throw std::runtime_error(std::string("missing_") + key);
  }
  return trim(body.at(key).get<std::string>());
}

std::optional<std::string> optional_trimmed_string(const nlohmann::json& body, const char* key) {
  if (!body.contains(key) || body.at(key).is_null()) {
    return std::nullopt;
  }
  if (!body.at(key).is_string()) {
    throw std::runtime_error(std::string("invalid_") + key);
  }
  return trim(body.at(key).get<std::string>());
}

std::optional<bool> optional_bool(const nlohmann::json& body, const char* key) {
  if (!body.contains(key) || body.at(key).is_null()) {
    return std::nullopt;
  }
  if (!body.at(key).is_boolean()) {
    throw std::runtime_error(std::string("invalid_") + key);
  }
  return body.at(key).get<bool>();
}

std::optional<std::vector<std::string>> optional_permissions(const nlohmann::json& body, const char* key) {
  if (!body.contains(key) || body.at(key).is_null()) {
    return std::nullopt;
  }
  if (!body.at(key).is_array()) {
    throw std::runtime_error(std::string("invalid_") + key);
  }
  std::vector<std::string> permissions;
  for (const auto& item : body.at(key)) {
    if (!item.is_string()) {
      throw std::runtime_error(std::string("invalid_") + key);
    }
    const auto value = trim(item.get<std::string>());
    if (!value.empty()) {
      permissions.push_back(value);
    }
  }
  return permissions;
}

std::optional<std::vector<std::string>> optional_string_array(const nlohmann::json& body, const char* key) {
  if (!body.contains(key) || body.at(key).is_null()) {
    return std::nullopt;
  }
  if (!body.at(key).is_array()) {
    throw std::runtime_error(std::string("invalid_") + key);
  }
  std::vector<std::string> values;
  for (const auto& item : body.at(key)) {
    if (!item.is_string()) {
      throw std::runtime_error(std::string("invalid_") + key);
    }
    const auto value = trim(item.get<std::string>());
    if (!value.empty()) {
      values.push_back(value);
    }
  }
  return values;
}

}  // namespace

IdentityServiceConfig load_identity_service_config(const std::filesystem::path& path) {
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("identity_service_config_open_failed");
  }
  nlohmann::json json;
  stream >> json;

  IdentityServiceConfig config;
  if (json.contains("http_port")) {
    config.http_port = json.at("http_port").get<std::uint16_t>();
  }
  if (json.contains("listen_host")) {
    config.listen_host = json.at("listen_host").get<std::string>();
  }
  if (json.contains("sqlite_path")) {
    config.sqlite_path = json.at("sqlite_path").get<std::string>();
  }
  if (json.contains("password_iterations")) {
    config.password_iterations = json.at("password_iterations").get<std::uint32_t>();
  }
  if (json.contains("password_time_cost")) {
    config.password_time_cost = json.at("password_time_cost").get<std::uint32_t>();
  }
  if (json.contains("password_memory_kib")) {
    config.password_memory_kib = json.at("password_memory_kib").get<std::uint32_t>();
  }
  if (json.contains("password_parallelism")) {
    config.password_parallelism = json.at("password_parallelism").get<std::uint32_t>();
  }
  if (json.contains("password_hash_length")) {
    config.password_hash_length = json.at("password_hash_length").get<std::uint32_t>();
  }
  if (json.contains("password_salt_length")) {
    config.password_salt_length = json.at("password_salt_length").get<std::uint32_t>();
  }
  if (json.contains("session_ttl_seconds")) {
    config.session_ttl_seconds = json.at("session_ttl_seconds").get<std::uint32_t>();
  }
  if (json.contains("password_reset_ttl_seconds")) {
    config.password_reset_ttl_seconds = json.at("password_reset_ttl_seconds").get<std::uint32_t>();
  }
  if (json.contains("email_verification_ttl_seconds")) {
    config.email_verification_ttl_seconds = json.at("email_verification_ttl_seconds").get<std::uint32_t>();
  }
  if (json.contains("public_base_url")) {
    config.public_base_url = json.at("public_base_url").get<std::string>();
  }
  if (json.contains("password_reset_path")) {
    config.password_reset_path = json.at("password_reset_path").get<std::string>();
  }
  if (json.contains("email_verification_path")) {
    config.email_verification_path = json.at("email_verification_path").get<std::string>();
  }
  if (json.contains("mail_from")) {
    config.mail_from = json.at("mail_from").get<std::string>();
  }
  if (json.contains("sendmail_path")) {
    config.sendmail_path = json.at("sendmail_path").get<std::string>();
  }
  if (json.contains("admin_token")) {
    config.admin_token = json.at("admin_token").get<std::string>();
  }
  if (json.contains("state_path")) {
    config.state_path = json.at("state_path").get<std::string>();
  }
  if (json.contains("patreon_client_id")) {
    config.patreon_client_id = json.at("patreon_client_id").get<std::string>();
  }
  if (json.contains("patreon_client_secret")) {
    config.patreon_client_secret = json.at("patreon_client_secret").get<std::string>();
  }
  if (json.contains("patreon_redirect_uri")) {
    config.patreon_redirect_uri = json.at("patreon_redirect_uri").get<std::string>();
  }
  if (json.contains("patreon_oauth_scopes")) {
    config.patreon_oauth_scopes = json.at("patreon_oauth_scopes").get<std::string>();
  }
  if (json.contains("patreon_api_version")) {
    config.patreon_api_version = json.at("patreon_api_version").get<std::string>();
  }
  if (json.contains("patreon_creator_access_token")) {
    config.patreon_creator_access_token = json.at("patreon_creator_access_token").get<std::string>();
  }
  if (json.contains("patreon_creator_refresh_token")) {
    config.patreon_creator_refresh_token = json.at("patreon_creator_refresh_token").get<std::string>();
  }
  return config;
}

nlohmann::json to_json(const IdentityServiceConfig& config) {
  return {
      {"http_port", config.http_port},
      {"listen_host", config.listen_host},
      {"sqlite_path", config.sqlite_path.string()},
      {"password_iterations", config.password_iterations},
      {"password_time_cost", config.password_time_cost},
      {"password_memory_kib", config.password_memory_kib},
      {"password_parallelism", config.password_parallelism},
      {"password_hash_length", config.password_hash_length},
      {"password_salt_length", config.password_salt_length},
      {"session_ttl_seconds", config.session_ttl_seconds},
      {"password_reset_ttl_seconds", config.password_reset_ttl_seconds},
      {"email_verification_ttl_seconds", config.email_verification_ttl_seconds},
      {"public_base_url", config.public_base_url},
      {"password_reset_path", config.password_reset_path},
      {"email_verification_path", config.email_verification_path},
      {"mail_from", config.mail_from},
      {"sendmail_path", config.sendmail_path},
      {"admin_token", config.admin_token.empty() ? nlohmann::json(nullptr) : nlohmann::json("<redacted>")},
      {"state_path", config.state_path.empty() ? nlohmann::json(nullptr) : nlohmann::json(config.state_path.string())},
      {"patreon_client_id", config.patreon_client_id.empty() ? nlohmann::json(nullptr) : nlohmann::json("<redacted>")},
      {"patreon_client_secret", config.patreon_client_secret.empty() ? nlohmann::json(nullptr) : nlohmann::json("<redacted>")},
      {"patreon_redirect_uri", config.patreon_redirect_uri.empty() ? nlohmann::json(nullptr) : nlohmann::json(config.patreon_redirect_uri)},
      {"patreon_oauth_scopes", config.patreon_oauth_scopes},
      {"patreon_api_version", config.patreon_api_version},
      {"patreon_creator_access_token", config.patreon_creator_access_token.empty() ? nlohmann::json(nullptr) : nlohmann::json("<redacted>")},
      {"patreon_creator_refresh_token", config.patreon_creator_refresh_token.empty() ? nlohmann::json(nullptr) : nlohmann::json("<redacted>")},
  };
}

IdentityStore::IdentityStore(std::filesystem::path sqlite_path)
    : sqlite_path_(std::move(sqlite_path)) {
  open();
  init_schema();
}

IdentityStore::~IdentityStore() {
  close();
}

void IdentityStore::open() {
  if (sqlite_path_.empty()) {
    throw std::runtime_error("identity_sqlite_path_required");
  }
  std::filesystem::create_directories(sqlite_path_.parent_path());
  if (sqlite3_open(sqlite_path_.string().c_str(), &db_) != SQLITE_OK) {
    throw std::runtime_error("identity_sqlite_open_failed");
  }
}

void IdentityStore::close() {
  if (db_ != nullptr) {
    sqlite3_close(db_);
    db_ = nullptr;
  }
}

void IdentityStore::init_schema() {
  exec(
      "CREATE TABLE IF NOT EXISTS users ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "username TEXT NOT NULL UNIQUE,"
      "email TEXT NOT NULL UNIQUE,"
      "display_name TEXT NOT NULL,"
      "avatar_url TEXT NOT NULL DEFAULT '',"
      "bio TEXT NOT NULL DEFAULT '',"
      "locale TEXT NOT NULL DEFAULT '',"
      "role TEXT NOT NULL DEFAULT 'player',"
      "permissions_json TEXT NOT NULL DEFAULT '[]',"
      "disabled_at TEXT,"
      "email_verified INTEGER NOT NULL DEFAULT 0,"
      "patreon_id TEXT,"
      "patreon_email TEXT,"
      "patreon_member_id TEXT,"
      "patreon_status TEXT,"
      "patreon_tier TEXT,"
      "patreon_is_supporter INTEGER NOT NULL DEFAULT 0,"
      "patreon_linked_at TEXT,"
      "password_salt_b64 TEXT NOT NULL,"
      "password_hash_b64 TEXT NOT NULL,"
      "password_iterations INTEGER NOT NULL,"
      "created_at TEXT NOT NULL,"
      "last_login_at TEXT"
      ");");
  exec_allow_duplicate_column("ALTER TABLE users ADD COLUMN avatar_url TEXT NOT NULL DEFAULT '';");
  exec_allow_duplicate_column("ALTER TABLE users ADD COLUMN bio TEXT NOT NULL DEFAULT '';");
  exec_allow_duplicate_column("ALTER TABLE users ADD COLUMN locale TEXT NOT NULL DEFAULT '';");
  exec_allow_duplicate_column("ALTER TABLE users ADD COLUMN role TEXT NOT NULL DEFAULT 'player';");
  exec_allow_duplicate_column("ALTER TABLE users ADD COLUMN permissions_json TEXT NOT NULL DEFAULT '[]';");
  exec_allow_duplicate_column("ALTER TABLE users ADD COLUMN disabled_at TEXT;");
  exec_allow_duplicate_column("ALTER TABLE users ADD COLUMN email_verified INTEGER NOT NULL DEFAULT 0;");
  exec_allow_duplicate_column("ALTER TABLE users ADD COLUMN patreon_id TEXT;");
  exec_allow_duplicate_column("ALTER TABLE users ADD COLUMN patreon_email TEXT;");
  exec_allow_duplicate_column("ALTER TABLE users ADD COLUMN patreon_member_id TEXT;");
  exec_allow_duplicate_column("ALTER TABLE users ADD COLUMN patreon_status TEXT;");
  exec_allow_duplicate_column("ALTER TABLE users ADD COLUMN patreon_tier TEXT;");
  exec_allow_duplicate_column("ALTER TABLE users ADD COLUMN patreon_is_supporter INTEGER NOT NULL DEFAULT 0;");
  exec_allow_duplicate_column("ALTER TABLE users ADD COLUMN patreon_linked_at TEXT;");
  exec_allow_duplicate_column("ALTER TABLE users ADD COLUMN two_factor_enabled INTEGER NOT NULL DEFAULT 0;");
  exec_allow_duplicate_column("ALTER TABLE users ADD COLUMN two_factor_secret_b32 TEXT NOT NULL DEFAULT '';");
  exec(
      "CREATE TABLE IF NOT EXISTS sessions ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "user_id INTEGER NOT NULL,"
      "session_token TEXT NOT NULL UNIQUE,"
      "created_at TEXT NOT NULL,"
      "expires_at TEXT NOT NULL,"
      "revoked_at TEXT,"
      "user_agent TEXT,"
      "client_name TEXT,"
      "client_version TEXT,"
      "device_id TEXT,"
      "requested_locale TEXT,"
      "FOREIGN KEY(user_id) REFERENCES users(id)"
      ");");
  exec_allow_duplicate_column("ALTER TABLE sessions ADD COLUMN user_agent TEXT;");
  exec_allow_duplicate_column("ALTER TABLE sessions ADD COLUMN client_name TEXT;");
  exec_allow_duplicate_column("ALTER TABLE sessions ADD COLUMN client_version TEXT;");
  exec_allow_duplicate_column("ALTER TABLE sessions ADD COLUMN device_id TEXT;");
  exec_allow_duplicate_column("ALTER TABLE sessions ADD COLUMN requested_locale TEXT;");
  exec(
      "CREATE TABLE IF NOT EXISTS recovery_codes ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "user_id INTEGER NOT NULL,"
      "code_hash_b64 TEXT NOT NULL,"
      "created_at TEXT NOT NULL,"
      "used_at TEXT,"
      "FOREIGN KEY(user_id) REFERENCES users(id)"
      ");");
  exec(
      "CREATE TABLE IF NOT EXISTS password_reset_tokens ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "user_id INTEGER NOT NULL,"
      "token_hash_b64 TEXT NOT NULL UNIQUE,"
      "created_at TEXT NOT NULL,"
      "expires_at TEXT NOT NULL,"
      "used_at TEXT,"
      "FOREIGN KEY(user_id) REFERENCES users(id)"
      ");");
  exec(
      "CREATE TABLE IF NOT EXISTS email_verification_tokens ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "user_id INTEGER NOT NULL,"
      "token_hash_b64 TEXT NOT NULL UNIQUE,"
      "created_at TEXT NOT NULL,"
      "expires_at TEXT NOT NULL,"
      "used_at TEXT,"
      "FOREIGN KEY(user_id) REFERENCES users(id)"
      ");");
  exec(
      "CREATE TABLE IF NOT EXISTS auth_audit ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "user_id INTEGER NOT NULL,"
      "event_type TEXT NOT NULL,"
      "payload_json TEXT NOT NULL,"
      "created_at TEXT NOT NULL,"
      "FOREIGN KEY(user_id) REFERENCES users(id)"
      ");");
  exec(
      "CREATE TABLE IF NOT EXISTS oauth_link_states ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "user_id INTEGER NOT NULL,"
      "provider TEXT NOT NULL,"
      "state_hash_b64 TEXT NOT NULL UNIQUE,"
      "created_at TEXT NOT NULL,"
      "expires_at TEXT NOT NULL,"
      "used_at TEXT,"
      "FOREIGN KEY(user_id) REFERENCES users(id)"
      ");");
  exec(
      "CREATE TABLE IF NOT EXISTS game_keys ("
      "id INTEGER PRIMARY KEY AUTOINCREMENT,"
      "key_code TEXT NOT NULL UNIQUE,"
      "status TEXT NOT NULL DEFAULT 'free',"
      "entitlements_json TEXT NOT NULL DEFAULT '[]',"
      "created_at TEXT NOT NULL,"
      "activated_at TEXT,"
      "deactivated_at TEXT,"
      "bound_user_id INTEGER,"
      "notes TEXT NOT NULL DEFAULT '',"
      "FOREIGN KEY(bound_user_id) REFERENCES users(id)"
      ");");
  exec_allow_duplicate_column("ALTER TABLE game_keys ADD COLUMN status TEXT NOT NULL DEFAULT 'free';");
  exec_allow_duplicate_column("ALTER TABLE game_keys ADD COLUMN entitlements_json TEXT NOT NULL DEFAULT '[]';");
  exec_allow_duplicate_column("ALTER TABLE game_keys ADD COLUMN created_at TEXT NOT NULL DEFAULT '';");
  exec_allow_duplicate_column("ALTER TABLE game_keys ADD COLUMN activated_at TEXT;");
  exec_allow_duplicate_column("ALTER TABLE game_keys ADD COLUMN deactivated_at TEXT;");
  exec_allow_duplicate_column("ALTER TABLE game_keys ADD COLUMN bound_user_id INTEGER;");
  exec_allow_duplicate_column("ALTER TABLE game_keys ADD COLUMN notes TEXT NOT NULL DEFAULT '';");
}

void IdentityStore::exec(const std::string& sql) const {
  char* error = nullptr;
  if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error) != SQLITE_OK) {
    const std::string message = error != nullptr ? error : "identity_sqlite_exec_failed";
    sqlite3_free(error);
    throw std::runtime_error(message);
  }
}

void IdentityStore::exec_allow_duplicate_column(const std::string& sql) const {
  char* error = nullptr;
  if (sqlite3_exec(db_, sql.c_str(), nullptr, nullptr, &error) == SQLITE_OK) {
    return;
  }
  const std::string message = error != nullptr ? error : "identity_sqlite_exec_failed";
  sqlite3_free(error);
  if (message.find("duplicate column name") != std::string::npos) {
    return;
  }
  throw std::runtime_error(message);
}

IdentityStore::RegisteredUser IdentityStore::create_user(
    const std::string& username,
    const std::string& email,
    const std::string& display_name,
    const std::string& avatar_url,
    const std::string& bio,
    const std::string& locale,
    const std::string& role,
    const std::vector<std::string>& permissions,
    bool email_verified,
    const std::string& password_salt_b64,
    const std::string& password_hash_b64,
    std::uint32_t password_iterations) {
  static constexpr const char* kSql =
      "INSERT INTO users (username, email, display_name, avatar_url, bio, locale, role, permissions_json, disabled_at, email_verified, password_salt_b64, password_hash_b64, password_iterations, created_at) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_insert_user_prepare_failed"));
  }
  const auto cleanup = [&stmt]() {
    if (stmt != nullptr) {
      sqlite3_finalize(stmt);
      stmt = nullptr;
    }
  };
  try {
    const auto now = utc_timestamp_now();
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, email.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, display_name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, avatar_url.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, bio.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, locale.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, role.c_str(), -1, SQLITE_TRANSIENT);
    const auto permissions_json = serialize_permissions_json(permissions);
    sqlite3_bind_text(stmt, 8, permissions_json.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_null(stmt, 9);
    sqlite3_bind_int(stmt, 10, email_verified ? 1 : 0);
    sqlite3_bind_text(stmt, 11, password_salt_b64.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 12, password_hash_b64.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 13, static_cast<int>(password_iterations));
    sqlite3_bind_text(stmt, 14, now.c_str(), -1, SQLITE_TRANSIENT);
    const auto rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
      const auto message = sqlite_error(db_, "identity_insert_user_failed");
      cleanup();
      if (message.find("UNIQUE constraint failed") != std::string::npos) {
        throw std::runtime_error("identity_user_conflict");
      }
      throw std::runtime_error(message);
    }
    RegisteredUser user{
        .user_id = sqlite3_last_insert_rowid(db_),
        .username = username,
        .email = email,
        .email_verified = email_verified,
        .display_name = display_name,
        .avatar_url = avatar_url,
        .bio = bio,
        .locale = locale,
        .role = role,
        .permissions = permissions,
    };
    cleanup();
    return user;
  } catch (...) {
    cleanup();
    throw;
  }
}

std::optional<IdentityStore::LoadedUser> IdentityStore::find_user_by_identity(const std::string& identity) const {
  static constexpr const char* kSql =
      "SELECT id, username, email, email_verified, display_name, avatar_url, bio, locale, password_salt_b64, password_hash_b64, password_iterations, two_factor_enabled, two_factor_secret_b32, role, permissions_json, disabled_at, "
      "patreon_id, patreon_email, patreon_member_id, patreon_status, patreon_tier, patreon_is_supporter, patreon_linked_at "
      "FROM users WHERE lower(username) = lower(?) OR lower(email) = lower(?) LIMIT 1;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_find_user_prepare_failed"));
  }
  const auto cleanup = [&stmt]() {
    if (stmt != nullptr) {
      sqlite3_finalize(stmt);
      stmt = nullptr;
    }
  };
  try {
    sqlite3_bind_text(stmt, 1, identity.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, identity.c_str(), -1, SQLITE_TRANSIENT);
    const auto rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
      LoadedUser user = loaded_user_from_stmt(stmt);
      cleanup();
      return user;
    }
    cleanup();
    return std::nullopt;
  } catch (...) {
    cleanup();
    throw;
  }
}

std::optional<IdentityStore::LoadedUser> IdentityStore::find_user_by_username(const std::string& username) const {
  static constexpr const char* kSql =
      "SELECT id, username, email, email_verified, display_name, avatar_url, bio, locale, password_salt_b64, password_hash_b64, password_iterations, two_factor_enabled, two_factor_secret_b32, role, permissions_json, disabled_at, "
      "patreon_id, patreon_email, patreon_member_id, patreon_status, patreon_tier, patreon_is_supporter, patreon_linked_at "
      "FROM users WHERE lower(username) = lower(?) LIMIT 1;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_find_user_by_username_prepare_failed"));
  }
  const auto cleanup = [&stmt]() {
    if (stmt != nullptr) {
      sqlite3_finalize(stmt);
      stmt = nullptr;
    }
  };
  try {
    sqlite3_bind_text(stmt, 1, username.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(stmt) == SQLITE_ROW) {
      LoadedUser user = loaded_user_from_stmt(stmt);
      cleanup();
      return user;
    }
    cleanup();
    return std::nullopt;
  } catch (...) {
    cleanup();
    throw;
  }
}

std::vector<IdentityStore::RegisteredUser> IdentityStore::list_users(
    std::size_t limit,
    const std::optional<std::string>& query,
    const std::optional<std::string>& role,
    const std::optional<bool>& disabled,
    std::size_t offset) const {
  const bool filtered = query.has_value();
  std::string sql =
      "SELECT id, username, email, email_verified, display_name, avatar_url, bio, locale, role, permissions_json, disabled_at, patreon_tier, patreon_is_supporter, patreon_linked_at "
      "FROM users";
  std::vector<std::string> conditions;
  if (filtered) {
    conditions.emplace_back("(lower(username) LIKE ? OR lower(email) LIKE ? OR lower(display_name) LIKE ?)");
  }
  if (role.has_value()) {
    conditions.emplace_back("lower(role) = ?");
  }
  if (disabled.has_value()) {
    conditions.emplace_back(*disabled ? "disabled_at IS NOT NULL" : "disabled_at IS NULL");
  }
  if (!conditions.empty()) {
    sql += " WHERE ";
    for (std::size_t index = 0; index < conditions.size(); ++index) {
      if (index > 0) {
        sql += " AND ";
      }
      sql += conditions[index];
    }
  }
  sql += " ORDER BY id DESC LIMIT ? OFFSET ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_list_users_prepare_failed"));
  }
  int bind_index = 1;
  if (filtered) {
    const auto pattern = "%" + lower(trim(*query)) + "%";
    sqlite3_bind_text(stmt, bind_index++, pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, bind_index++, pattern.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, bind_index++, pattern.c_str(), -1, SQLITE_TRANSIENT);
  }
  if (role.has_value()) {
    const auto normalized_role = lower(trim(*role));
    sqlite3_bind_text(stmt, bind_index++, normalized_role.c_str(), -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_int64(stmt, bind_index++, static_cast<sqlite3_int64>(limit));
  sqlite3_bind_int64(stmt, bind_index, static_cast<sqlite3_int64>(offset));
  std::vector<RegisteredUser> users;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    users.push_back(registered_user_from_stmt(stmt));
  }
  sqlite3_finalize(stmt);
  std::reverse(users.begin(), users.end());
  return users;
}

IdentityStore::SessionRecord IdentityStore::create_session(
    std::int64_t user_id,
    std::uint32_t ttl_seconds,
    const IdentityRequestContext& context) {
  static constexpr const char* kLookupSql =
      "SELECT username, email, email_verified, display_name, avatar_url, bio, locale, role, permissions_json, patreon_tier, patreon_is_supporter "
      "FROM users WHERE id = ? LIMIT 1;";
  sqlite3_stmt* lookup_stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kLookupSql, -1, &lookup_stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_session_lookup_prepare_failed"));
  }
  const auto lookup_cleanup = [&lookup_stmt]() {
    if (lookup_stmt != nullptr) {
      sqlite3_finalize(lookup_stmt);
      lookup_stmt = nullptr;
    }
  };
  sqlite3_bind_int64(lookup_stmt, 1, user_id);
  if (sqlite3_step(lookup_stmt) != SQLITE_ROW) {
    lookup_cleanup();
    throw std::runtime_error("identity_user_not_found");
  }
  const auto username = std::string(reinterpret_cast<const char*>(sqlite3_column_text(lookup_stmt, 0)));
  const auto email = std::string(reinterpret_cast<const char*>(sqlite3_column_text(lookup_stmt, 1)));
  const auto email_verified = sqlite3_column_int(lookup_stmt, 2) != 0;
  const auto display_name = std::string(reinterpret_cast<const char*>(sqlite3_column_text(lookup_stmt, 3)));
  const auto avatar_url = std::string(reinterpret_cast<const char*>(sqlite3_column_text(lookup_stmt, 4)));
  const auto bio = std::string(reinterpret_cast<const char*>(sqlite3_column_text(lookup_stmt, 5)));
  const auto locale = std::string(reinterpret_cast<const char*>(sqlite3_column_text(lookup_stmt, 6)));
  const auto role = std::string(reinterpret_cast<const char*>(sqlite3_column_text(lookup_stmt, 7)));
  const auto permissions = parse_permissions_json(reinterpret_cast<const char*>(sqlite3_column_text(lookup_stmt, 8)));
  const auto patreon_tier = column_optional_text(lookup_stmt, 9);
  const auto patreon_is_supporter = sqlite3_column_int(lookup_stmt, 10) != 0;
  lookup_cleanup();

  static constexpr const char* kInsertSql =
      "INSERT INTO sessions (user_id, session_token, created_at, expires_at, user_agent, client_name, client_version, device_id, requested_locale) "
      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?);";
  sqlite3_stmt* insert_stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kInsertSql, -1, &insert_stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_insert_session_prepare_failed"));
  }
  const auto insert_cleanup = [&insert_stmt]() {
    if (insert_stmt != nullptr) {
      sqlite3_finalize(insert_stmt);
      insert_stmt = nullptr;
    }
  };

  const auto created_at = utc_timestamp_now();
  const auto expires_at = compute_future_utc(ttl_seconds);
  const auto session_token = random_token_b64(32);
  sqlite3_bind_int64(insert_stmt, 1, user_id);
  sqlite3_bind_text(insert_stmt, 2, session_token.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(insert_stmt, 3, created_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(insert_stmt, 4, expires_at.c_str(), -1, SQLITE_TRANSIENT);
  bind_optional_text(insert_stmt, 5, normalize_optional_header(context.user_agent));
  bind_optional_text(insert_stmt, 6, normalize_optional_header(context.client_name));
  bind_optional_text(insert_stmt, 7, normalize_optional_header(context.client_version));
  bind_optional_text(insert_stmt, 8, normalize_optional_header(context.device_id));
  bind_optional_text(insert_stmt, 9, normalize_optional_header(context.locale));
  if (sqlite3_step(insert_stmt) != SQLITE_DONE) {
    const auto message = sqlite_error(db_, "identity_insert_session_failed");
    insert_cleanup();
    throw std::runtime_error(message);
  }
  insert_cleanup();

  return SessionRecord{
      .session_id = sqlite3_last_insert_rowid(db_),
      .session_token = session_token,
      .user_id = user_id,
      .username = username,
      .email = email,
      .email_verified = email_verified,
      .display_name = display_name,
      .avatar_url = avatar_url,
      .bio = bio,
      .locale = locale,
      .created_at = created_at,
      .expires_at = expires_at,
      .revoked_at = std::nullopt,
      .user_agent = normalize_optional_header(context.user_agent),
      .client_name = normalize_optional_header(context.client_name),
      .client_version = normalize_optional_header(context.client_version),
      .device_id = normalize_optional_header(context.device_id),
      .requested_locale = normalize_optional_header(context.locale),
      .role = role,
      .permissions = permissions,
      .patreon_tier = patreon_tier,
      .patreon_is_supporter = patreon_is_supporter,
  };
}

std::optional<IdentityStore::SessionRecord> IdentityStore::find_session(const std::string& session_token) const {
  static constexpr const char* kSql =
      "SELECT s.id, s.session_token, u.id, u.username, u.email, u.email_verified, u.display_name, u.avatar_url, u.bio, u.locale, s.created_at, s.expires_at, s.revoked_at, "
      "s.user_agent, s.client_name, s.client_version, s.device_id, s.requested_locale, u.role, u.permissions_json, u.patreon_tier, u.patreon_is_supporter "
      "FROM sessions s "
      "JOIN users u ON u.id = s.user_id "
      "WHERE s.session_token = ? AND s.revoked_at IS NULL AND s.expires_at >= ? "
      "LIMIT 1;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_find_session_prepare_failed"));
  }
  const auto cleanup = [&stmt]() {
    if (stmt != nullptr) {
      sqlite3_finalize(stmt);
      stmt = nullptr;
    }
  };
  const auto now = utc_timestamp_now();
  sqlite3_bind_text(stmt, 1, session_token.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, now.c_str(), -1, SQLITE_TRANSIENT);
  const auto rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    SessionRecord session = session_record_from_stmt(stmt);
    cleanup();
    return session;
  }
  cleanup();
  return std::nullopt;
}

std::vector<IdentityStore::SessionRecord> IdentityStore::list_sessions(std::int64_t user_id) const {
  static constexpr const char* kSql =
      "SELECT s.id, s.session_token, u.id, u.username, u.email, u.email_verified, u.display_name, u.avatar_url, u.bio, u.locale, s.created_at, s.expires_at, s.revoked_at, "
      "s.user_agent, s.client_name, s.client_version, s.device_id, s.requested_locale, u.role, u.permissions_json, u.patreon_tier, u.patreon_is_supporter "
      "FROM sessions s "
      "JOIN users u ON u.id = s.user_id "
      "WHERE s.user_id = ? "
      "ORDER BY s.created_at DESC;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_list_sessions_prepare_failed"));
  }
  sqlite3_bind_int64(stmt, 1, user_id);
  std::vector<SessionRecord> sessions;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    sessions.push_back(session_record_from_stmt(stmt));
  }
  sqlite3_finalize(stmt);
  return sessions;
}

std::optional<IdentityStore::LoadedUser> IdentityStore::update_profile(
    std::int64_t user_id,
    const std::optional<std::string>& display_name,
    const std::optional<std::string>& avatar_url,
    const std::optional<std::string>& bio,
    const std::optional<std::string>& locale) {
  static constexpr const char* kSql =
      "UPDATE users SET "
      "display_name = COALESCE(?, display_name), "
      "avatar_url = COALESCE(?, avatar_url), "
      "bio = COALESCE(?, bio), "
      "locale = COALESCE(?, locale) "
      "WHERE id = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_update_profile_prepare_failed"));
  }
  const auto cleanup = [&stmt]() {
    if (stmt != nullptr) {
      sqlite3_finalize(stmt);
      stmt = nullptr;
    }
  };
  auto bind_optional = [stmt](int index, const std::optional<std::string>& value) {
    if (!value.has_value()) {
      sqlite3_bind_null(stmt, index);
    } else {
      sqlite3_bind_text(stmt, index, value->c_str(), -1, SQLITE_TRANSIENT);
    }
  };
  bind_optional(1, display_name);
  bind_optional(2, avatar_url);
  bind_optional(3, bio);
  bind_optional(4, locale);
  sqlite3_bind_int64(stmt, 5, user_id);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    const auto message = sqlite_error(db_, "identity_update_profile_failed");
    cleanup();
    throw std::runtime_error(message);
  }
  cleanup();
  static constexpr const char* kLookupSql =
      "SELECT id, username, email, email_verified, display_name, avatar_url, bio, locale, password_salt_b64, password_hash_b64, password_iterations, two_factor_enabled, two_factor_secret_b32, role, permissions_json, disabled_at, "
      "patreon_id, patreon_email, patreon_member_id, patreon_status, patreon_tier, patreon_is_supporter, patreon_linked_at "
      "FROM users WHERE id = ? LIMIT 1;";
  sqlite3_stmt* lookup = nullptr;
  if (sqlite3_prepare_v2(db_, kLookupSql, -1, &lookup, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_update_profile_lookup_prepare_failed"));
  }
  const auto lookup_cleanup = [&lookup]() {
    if (lookup != nullptr) {
      sqlite3_finalize(lookup);
      lookup = nullptr;
    }
  };
  sqlite3_bind_int64(lookup, 1, user_id);
  if (sqlite3_step(lookup) == SQLITE_ROW) {
    LoadedUser user = loaded_user_from_stmt(lookup);
    lookup_cleanup();
    return user;
  }
  lookup_cleanup();
  return std::nullopt;
}

std::optional<IdentityStore::LoadedUser> IdentityStore::admin_update_user(
    std::int64_t user_id,
    const std::optional<std::string>& email,
    const std::optional<std::string>& display_name,
    const std::optional<std::string>& avatar_url,
    const std::optional<std::string>& bio,
    const std::optional<std::string>& locale,
    const std::optional<std::string>& role,
    const std::optional<std::vector<std::string>>& permissions,
    const std::optional<bool>& email_verified,
    const std::optional<bool>& disabled) {
  static constexpr const char* kSql =
      "UPDATE users SET "
      "email = COALESCE(?, email), "
      "display_name = COALESCE(?, display_name), "
      "avatar_url = COALESCE(?, avatar_url), "
      "bio = COALESCE(?, bio), "
      "locale = COALESCE(?, locale), "
      "role = COALESCE(?, role), "
      "permissions_json = COALESCE(?, permissions_json), "
      "email_verified = COALESCE(?, email_verified), "
      "disabled_at = CASE "
      "  WHEN ? IS NULL THEN disabled_at "
      "  WHEN ? = 1 THEN COALESCE(disabled_at, ?) "
      "  ELSE NULL "
      "END "
      "WHERE id = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_admin_update_user_prepare_failed"));
  }
  const auto cleanup = [&stmt]() {
    if (stmt != nullptr) {
      sqlite3_finalize(stmt);
      stmt = nullptr;
    }
  };
  auto bind_optional = [stmt](int index, const std::optional<std::string>& value) {
    if (!value.has_value()) {
      sqlite3_bind_null(stmt, index);
    } else {
      sqlite3_bind_text(stmt, index, value->c_str(), -1, SQLITE_TRANSIENT);
    }
  };
  bind_optional(1, email);
  bind_optional(2, display_name);
  bind_optional(3, avatar_url);
  bind_optional(4, bio);
  bind_optional(5, locale);
  bind_optional(6, role);
  if (!permissions.has_value()) {
    sqlite3_bind_null(stmt, 7);
  } else {
    const auto permissions_json = serialize_permissions_json(*permissions);
    sqlite3_bind_text(stmt, 7, permissions_json.c_str(), -1, SQLITE_TRANSIENT);
  }
  if (!email_verified.has_value()) {
    sqlite3_bind_null(stmt, 8);
  } else {
    sqlite3_bind_int(stmt, 8, *email_verified ? 1 : 0);
  }
  if (!disabled.has_value()) {
    sqlite3_bind_null(stmt, 9);
    sqlite3_bind_null(stmt, 10);
    sqlite3_bind_null(stmt, 11);
  } else {
    const auto now = utc_timestamp_now();
    sqlite3_bind_int(stmt, 9, *disabled ? 1 : 0);
    sqlite3_bind_int(stmt, 10, *disabled ? 1 : 0);
    sqlite3_bind_text(stmt, 11, now.c_str(), -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_int64(stmt, 12, user_id);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    const auto message = sqlite_error(db_, "identity_admin_update_user_failed");
    cleanup();
    if (message.find("UNIQUE constraint failed") != std::string::npos) {
      throw std::runtime_error("identity_user_conflict");
    }
    throw std::runtime_error(message);
  }
  cleanup();
  static constexpr const char* kLookupSql =
      "SELECT id, username, email, email_verified, display_name, avatar_url, bio, locale, password_salt_b64, password_hash_b64, password_iterations, two_factor_enabled, two_factor_secret_b32, role, permissions_json, disabled_at, "
      "patreon_id, patreon_email, patreon_member_id, patreon_status, patreon_tier, patreon_is_supporter, patreon_linked_at "
      "FROM users WHERE id = ? LIMIT 1;";
  sqlite3_stmt* lookup = nullptr;
  if (sqlite3_prepare_v2(db_, kLookupSql, -1, &lookup, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_admin_update_user_lookup_prepare_failed"));
  }
  const auto lookup_cleanup = [&lookup]() {
    if (lookup != nullptr) {
      sqlite3_finalize(lookup);
      lookup = nullptr;
    }
  };
  sqlite3_bind_int64(lookup, 1, user_id);
  if (sqlite3_step(lookup) == SQLITE_ROW) {
    LoadedUser user = loaded_user_from_stmt(lookup);
    lookup_cleanup();
    return user;
  }
  lookup_cleanup();
  return std::nullopt;
}

IdentityStore::SecurityState IdentityStore::security_state(std::int64_t user_id) const {
  SecurityState state;
  static constexpr const char* kUserSql =
      "SELECT two_factor_enabled FROM users WHERE id = ? LIMIT 1;";
  sqlite3_stmt* user_stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kUserSql, -1, &user_stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_security_state_user_prepare_failed"));
  }
  sqlite3_bind_int64(user_stmt, 1, user_id);
  if (sqlite3_step(user_stmt) == SQLITE_ROW) {
    state.two_factor_enabled = sqlite3_column_int(user_stmt, 0) != 0;
  }
  sqlite3_finalize(user_stmt);

  static constexpr const char* kCodeSql =
      "SELECT COUNT(*) FROM recovery_codes WHERE user_id = ? AND used_at IS NULL;";
  sqlite3_stmt* code_stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kCodeSql, -1, &code_stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_security_state_codes_prepare_failed"));
  }
  sqlite3_bind_int64(code_stmt, 1, user_id);
  if (sqlite3_step(code_stmt) == SQLITE_ROW) {
    state.recovery_code_count = static_cast<std::size_t>(sqlite3_column_int64(code_stmt, 0));
  }
  sqlite3_finalize(code_stmt);
  return state;
}

std::vector<std::string> IdentityStore::regenerate_recovery_codes(std::int64_t user_id, std::size_t count) {
  exec("BEGIN TRANSACTION;");
  try {
    sqlite3_stmt* delete_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM recovery_codes WHERE user_id = ?;", -1, &delete_stmt, nullptr) != SQLITE_OK) {
      throw std::runtime_error(sqlite_error(db_, "identity_recovery_delete_prepare_failed"));
    }
    sqlite3_bind_int64(delete_stmt, 1, user_id);
    if (sqlite3_step(delete_stmt) != SQLITE_DONE) {
      sqlite3_finalize(delete_stmt);
      throw std::runtime_error(sqlite_error(db_, "identity_recovery_delete_failed"));
    }
    sqlite3_finalize(delete_stmt);

    sqlite3_stmt* insert_stmt = nullptr;
    if (sqlite3_prepare_v2(
            db_,
            "INSERT INTO recovery_codes (user_id, code_hash_b64, created_at) VALUES (?, ?, ?);",
            -1,
            &insert_stmt,
            nullptr) != SQLITE_OK) {
      throw std::runtime_error(sqlite_error(db_, "identity_recovery_insert_prepare_failed"));
    }
    std::vector<std::string> codes;
    codes.reserve(count);
    for (std::size_t i = 0; i < count; ++i) {
      const auto code = random_recovery_code();
      const auto code_hash = sha256_b64(code);
      const auto created_at = utc_timestamp_now();
      sqlite3_reset(insert_stmt);
      sqlite3_clear_bindings(insert_stmt);
      sqlite3_bind_int64(insert_stmt, 1, user_id);
      sqlite3_bind_text(insert_stmt, 2, code_hash.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(insert_stmt, 3, created_at.c_str(), -1, SQLITE_TRANSIENT);
      if (sqlite3_step(insert_stmt) != SQLITE_DONE) {
        sqlite3_finalize(insert_stmt);
        throw std::runtime_error(sqlite_error(db_, "identity_recovery_insert_failed"));
      }
      codes.push_back(code);
    }
    sqlite3_finalize(insert_stmt);
    exec("COMMIT;");
    return codes;
  } catch (...) {
    try {
      exec("ROLLBACK;");
    } catch (...) {
    }
    throw;
  }
}

IdentityStore::TwoFactorSetup IdentityStore::issue_two_factor_secret(std::int64_t user_id) {
  const auto secret_b32 = random_two_factor_secret_b32();
  static constexpr const char* kSql =
      "UPDATE users SET two_factor_secret_b32 = ?, two_factor_enabled = 0 WHERE id = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_two_factor_setup_prepare_failed"));
  }
  sqlite3_bind_text(stmt, 1, secret_b32.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, user_id);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "identity_two_factor_setup_failed"));
  }
  sqlite3_finalize(stmt);
  return TwoFactorSetup{.secret_b32 = secret_b32};
}

bool IdentityStore::enable_two_factor(std::int64_t user_id, const std::string& secret_b32) {
  static constexpr const char* kSql =
      "UPDATE users SET two_factor_enabled = 1 WHERE id = ? AND two_factor_secret_b32 = ? AND two_factor_secret_b32 <> '';";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_two_factor_enable_prepare_failed"));
  }
  sqlite3_bind_int64(stmt, 1, user_id);
  sqlite3_bind_text(stmt, 2, secret_b32.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "identity_two_factor_enable_failed"));
  }
  const auto changed = sqlite3_changes(db_) > 0;
  sqlite3_finalize(stmt);
  return changed;
}

void IdentityStore::disable_two_factor(std::int64_t user_id) {
  static constexpr const char* kSql =
      "UPDATE users SET two_factor_enabled = 0, two_factor_secret_b32 = '' WHERE id = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_two_factor_disable_prepare_failed"));
  }
  sqlite3_bind_int64(stmt, 1, user_id);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "identity_two_factor_disable_failed"));
  }
  sqlite3_finalize(stmt);
}

bool IdentityStore::consume_recovery_code(std::int64_t user_id, const std::string& code) {
  static constexpr const char* kSql =
      "UPDATE recovery_codes SET used_at = ? WHERE user_id = ? AND code_hash_b64 = ? AND used_at IS NULL;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_recovery_consume_prepare_failed"));
  }
  const auto now = utc_timestamp_now();
  const auto code_hash_b64 = sha256_b64(trim(code));
  sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, user_id);
  sqlite3_bind_text(stmt, 3, code_hash_b64.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "identity_recovery_consume_failed"));
  }
  const auto changed = sqlite3_changes(db_) > 0;
  sqlite3_finalize(stmt);
  return changed;
}

void IdentityStore::update_password(
    std::int64_t user_id,
    const std::string& password_salt_b64,
    const std::string& password_hash_b64,
    std::uint32_t password_iterations) {
  static constexpr const char* kSql =
      "UPDATE users SET password_salt_b64 = ?, password_hash_b64 = ?, password_iterations = ? WHERE id = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_update_password_prepare_failed"));
  }
  sqlite3_bind_text(stmt, 1, password_salt_b64.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, password_hash_b64.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt, 3, static_cast<int>(password_iterations));
  sqlite3_bind_int64(stmt, 4, user_id);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "identity_update_password_failed"));
  }
  sqlite3_finalize(stmt);
}

void IdentityStore::revoke_all_sessions(std::int64_t user_id) {
  static constexpr const char* kSql =
      "UPDATE sessions SET revoked_at = ? WHERE user_id = ? AND revoked_at IS NULL;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_revoke_sessions_prepare_failed"));
  }
  const auto now = utc_timestamp_now();
  sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, user_id);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "identity_revoke_sessions_failed"));
  }
  sqlite3_finalize(stmt);
}

bool IdentityStore::revoke_session(std::int64_t user_id, std::int64_t session_id) {
  static constexpr const char* kSql =
      "UPDATE sessions SET revoked_at = ? WHERE id = ? AND user_id = ? AND revoked_at IS NULL;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_revoke_session_prepare_failed"));
  }
  const auto now = utc_timestamp_now();
  sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, session_id);
  sqlite3_bind_int64(stmt, 3, user_id);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "identity_revoke_session_failed"));
  }
  const auto changed = sqlite3_changes(db_) > 0;
  sqlite3_finalize(stmt);
  return changed;
}

std::size_t IdentityStore::revoke_other_sessions(
    std::int64_t user_id,
    std::optional<std::int64_t> keep_session_id) {
  std::string sql = "UPDATE sessions SET revoked_at = ? WHERE user_id = ? AND revoked_at IS NULL";
  if (keep_session_id.has_value()) {
    sql += " AND id != ?";
  }
  sql += ";";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_revoke_other_sessions_prepare_failed"));
  }
  const auto now = utc_timestamp_now();
  sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, user_id);
  if (keep_session_id.has_value()) {
    sqlite3_bind_int64(stmt, 3, *keep_session_id);
  }
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "identity_revoke_other_sessions_failed"));
  }
  const auto changed = static_cast<std::size_t>(sqlite3_changes(db_));
  sqlite3_finalize(stmt);
  return changed;
}

std::vector<std::int64_t> IdentityStore::revoke_device_sessions(std::int64_t user_id, const std::string& device_key) {
  std::vector<std::int64_t> revoked_session_ids;
  for (const auto& session : list_sessions(user_id)) {
    if (session.revoked_at.has_value()) {
      continue;
    }
    if (session_device_key(session) != device_key) {
      continue;
    }
    if (revoke_session(user_id, session.session_id)) {
      revoked_session_ids.push_back(session.session_id);
    }
  }
  return revoked_session_ids;
}

void IdentityStore::issue_password_reset(
    std::int64_t user_id,
    const std::string& token_hash_b64,
    const std::string& expires_at) {
  exec("BEGIN TRANSACTION;");
  try {
    sqlite3_stmt* delete_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM password_reset_tokens WHERE user_id = ? AND used_at IS NULL;", -1, &delete_stmt, nullptr) != SQLITE_OK) {
      throw std::runtime_error(sqlite_error(db_, "identity_password_reset_delete_prepare_failed"));
    }
    sqlite3_bind_int64(delete_stmt, 1, user_id);
    if (sqlite3_step(delete_stmt) != SQLITE_DONE) {
      sqlite3_finalize(delete_stmt);
      throw std::runtime_error(sqlite_error(db_, "identity_password_reset_delete_failed"));
    }
    sqlite3_finalize(delete_stmt);

    sqlite3_stmt* insert_stmt = nullptr;
    if (sqlite3_prepare_v2(
            db_,
            "INSERT INTO password_reset_tokens (user_id, token_hash_b64, created_at, expires_at) VALUES (?, ?, ?, ?);",
            -1,
            &insert_stmt,
            nullptr) != SQLITE_OK) {
      throw std::runtime_error(sqlite_error(db_, "identity_password_reset_insert_prepare_failed"));
    }
    const auto now = utc_timestamp_now();
    sqlite3_bind_int64(insert_stmt, 1, user_id);
    sqlite3_bind_text(insert_stmt, 2, token_hash_b64.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert_stmt, 3, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert_stmt, 4, expires_at.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(insert_stmt) != SQLITE_DONE) {
      sqlite3_finalize(insert_stmt);
      throw std::runtime_error(sqlite_error(db_, "identity_password_reset_insert_failed"));
    }
    sqlite3_finalize(insert_stmt);
    exec("COMMIT;");
  } catch (...) {
    try {
      exec("ROLLBACK;");
    } catch (...) {
    }
    throw;
  }
}

std::optional<IdentityStore::PasswordResetRecord> IdentityStore::find_password_reset(const std::string& raw_token) const {
  static constexpr const char* kSql =
      "SELECT u.id, u.username, u.email, u.email_verified, u.display_name, p.token_hash_b64, p.expires_at "
      "FROM password_reset_tokens p "
      "JOIN users u ON u.id = p.user_id "
      "WHERE p.token_hash_b64 = ? AND p.used_at IS NULL AND p.expires_at >= ? "
      "LIMIT 1;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_password_reset_lookup_prepare_failed"));
  }
  const auto token_hash_b64 = sha256_b64(raw_token);
  const auto now = utc_timestamp_now();
  sqlite3_bind_text(stmt, 1, token_hash_b64.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, now.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    PasswordResetRecord record{
        .user_id = sqlite3_column_int64(stmt, 0),
        .username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
        .email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
        .email_verified = sqlite3_column_int(stmt, 3) != 0,
        .display_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)),
        .token_hash_b64 = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)),
        .expires_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6)),
    };
    sqlite3_finalize(stmt);
    return record;
  }
  sqlite3_finalize(stmt);
  return std::nullopt;
}

void IdentityStore::consume_password_reset(std::int64_t user_id, const std::string& raw_token) {
  static constexpr const char* kSql =
      "UPDATE password_reset_tokens SET used_at = ? WHERE user_id = ? AND token_hash_b64 = ? AND used_at IS NULL;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_password_reset_consume_prepare_failed"));
  }
  const auto now = utc_timestamp_now();
  const auto token_hash_b64 = sha256_b64(raw_token);
  sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, user_id);
  sqlite3_bind_text(stmt, 3, token_hash_b64.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "identity_password_reset_consume_failed"));
  }
  sqlite3_finalize(stmt);
}

void IdentityStore::issue_email_verification(
    std::int64_t user_id,
    const std::string& token_hash_b64,
    const std::string& expires_at) {
  exec("BEGIN TRANSACTION;");
  try {
    sqlite3_stmt* delete_stmt = nullptr;
    if (sqlite3_prepare_v2(db_, "DELETE FROM email_verification_tokens WHERE user_id = ? AND used_at IS NULL;", -1, &delete_stmt, nullptr) != SQLITE_OK) {
      throw std::runtime_error(sqlite_error(db_, "identity_email_verification_delete_prepare_failed"));
    }
    sqlite3_bind_int64(delete_stmt, 1, user_id);
    if (sqlite3_step(delete_stmt) != SQLITE_DONE) {
      sqlite3_finalize(delete_stmt);
      throw std::runtime_error(sqlite_error(db_, "identity_email_verification_delete_failed"));
    }
    sqlite3_finalize(delete_stmt);

    sqlite3_stmt* insert_stmt = nullptr;
    if (sqlite3_prepare_v2(
            db_,
            "INSERT INTO email_verification_tokens (user_id, token_hash_b64, created_at, expires_at) VALUES (?, ?, ?, ?);",
            -1,
            &insert_stmt,
            nullptr) != SQLITE_OK) {
      throw std::runtime_error(sqlite_error(db_, "identity_email_verification_insert_prepare_failed"));
    }
    const auto now = utc_timestamp_now();
    sqlite3_bind_int64(insert_stmt, 1, user_id);
    sqlite3_bind_text(insert_stmt, 2, token_hash_b64.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert_stmt, 3, now.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(insert_stmt, 4, expires_at.c_str(), -1, SQLITE_TRANSIENT);
    if (sqlite3_step(insert_stmt) != SQLITE_DONE) {
      sqlite3_finalize(insert_stmt);
      throw std::runtime_error(sqlite_error(db_, "identity_email_verification_insert_failed"));
    }
    sqlite3_finalize(insert_stmt);
    exec("COMMIT;");
  } catch (...) {
    try {
      exec("ROLLBACK;");
    } catch (...) {
    }
    throw;
  }
}

std::optional<IdentityStore::EmailVerificationRecord> IdentityStore::find_email_verification(const std::string& raw_token) const {
  static constexpr const char* kSql =
      "SELECT u.id, u.username, u.email, u.display_name, e.token_hash_b64, e.expires_at "
      "FROM email_verification_tokens e "
      "JOIN users u ON u.id = e.user_id "
      "WHERE e.token_hash_b64 = ? AND e.used_at IS NULL AND e.expires_at >= ? "
      "LIMIT 1;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_email_verification_lookup_prepare_failed"));
  }
  const auto token_hash_b64 = sha256_b64(raw_token);
  const auto now = utc_timestamp_now();
  sqlite3_bind_text(stmt, 1, token_hash_b64.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, now.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) == SQLITE_ROW) {
    EmailVerificationRecord record{
        .user_id = sqlite3_column_int64(stmt, 0),
        .username = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
        .email = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
        .display_name = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3)),
        .token_hash_b64 = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)),
        .expires_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5)),
    };
    sqlite3_finalize(stmt);
    return record;
  }
  sqlite3_finalize(stmt);
  return std::nullopt;
}

void IdentityStore::consume_email_verification(std::int64_t user_id, const std::string& raw_token) {
  static constexpr const char* kSql =
      "UPDATE email_verification_tokens SET used_at = ? WHERE user_id = ? AND token_hash_b64 = ? AND used_at IS NULL;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_email_verification_consume_prepare_failed"));
  }
  const auto now = utc_timestamp_now();
  const auto token_hash_b64 = sha256_b64(raw_token);
  sqlite3_bind_text(stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, user_id);
  sqlite3_bind_text(stmt, 3, token_hash_b64.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "identity_email_verification_consume_failed"));
  }
  sqlite3_finalize(stmt);
}

void IdentityStore::mark_email_verified(std::int64_t user_id) {
  static constexpr const char* kSql = "UPDATE users SET email_verified = 1 WHERE id = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_mark_email_verified_prepare_failed"));
  }
  sqlite3_bind_int64(stmt, 1, user_id);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "identity_mark_email_verified_failed"));
  }
  sqlite3_finalize(stmt);
}

void IdentityStore::issue_oauth_link_state(
    std::int64_t user_id,
    const std::string& provider,
    const std::string& state_hash_b64,
    const std::string& expires_at) {
  static constexpr const char* kCleanupSql =
      "DELETE FROM oauth_link_states WHERE user_id = ? AND provider = ? AND used_at IS NULL;";
  sqlite3_stmt* cleanup_stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kCleanupSql, -1, &cleanup_stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_oauth_state_cleanup_prepare_failed"));
  }
  sqlite3_bind_int64(cleanup_stmt, 1, user_id);
  sqlite3_bind_text(cleanup_stmt, 2, provider.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(cleanup_stmt) != SQLITE_DONE) {
    sqlite3_finalize(cleanup_stmt);
    throw std::runtime_error(sqlite_error(db_, "identity_oauth_state_cleanup_failed"));
  }
  sqlite3_finalize(cleanup_stmt);

  static constexpr const char* kInsertSql =
      "INSERT INTO oauth_link_states (user_id, provider, state_hash_b64, created_at, expires_at, used_at) "
      "VALUES (?, ?, ?, ?, ?, NULL);";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kInsertSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_oauth_state_insert_prepare_failed"));
  }
  const auto created_at = utc_timestamp_now();
  sqlite3_bind_int64(stmt, 1, user_id);
  sqlite3_bind_text(stmt, 2, provider.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, state_hash_b64.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, created_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 5, expires_at.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "identity_oauth_state_insert_failed"));
  }
  sqlite3_finalize(stmt);
}

std::optional<IdentityStore::OAuthLinkStateRecord> IdentityStore::consume_oauth_link_state(
    std::int64_t user_id,
    const std::string& provider,
    const std::string& raw_state) {
  static constexpr const char* kLookupSql =
      "SELECT user_id, provider, expires_at FROM oauth_link_states "
      "WHERE user_id = ? AND provider = ? AND state_hash_b64 = ? AND used_at IS NULL AND expires_at >= ? "
      "LIMIT 1;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kLookupSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_oauth_state_lookup_prepare_failed"));
  }
  const auto state_hash_b64 = sha256_b64(raw_state);
  const auto now = utc_timestamp_now();
  sqlite3_bind_int64(stmt, 1, user_id);
  sqlite3_bind_text(stmt, 2, provider.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, state_hash_b64.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, now.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_ROW) {
    sqlite3_finalize(stmt);
    return std::nullopt;
  }
  OAuthLinkStateRecord record{
      .user_id = sqlite3_column_int64(stmt, 0),
      .provider = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1)),
      .expires_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
  };
  sqlite3_finalize(stmt);

  static constexpr const char* kConsumeSql =
      "UPDATE oauth_link_states SET used_at = ? WHERE user_id = ? AND provider = ? AND state_hash_b64 = ? AND used_at IS NULL;";
  sqlite3_stmt* consume_stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kConsumeSql, -1, &consume_stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_oauth_state_consume_prepare_failed"));
  }
  sqlite3_bind_text(consume_stmt, 1, now.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(consume_stmt, 2, user_id);
  sqlite3_bind_text(consume_stmt, 3, provider.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(consume_stmt, 4, state_hash_b64.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(consume_stmt) != SQLITE_DONE) {
    sqlite3_finalize(consume_stmt);
    throw std::runtime_error(sqlite_error(db_, "identity_oauth_state_consume_failed"));
  }
  sqlite3_finalize(consume_stmt);
  return record;
}

std::optional<IdentityStore::LoadedUser> IdentityStore::link_patreon_account(
    std::int64_t user_id,
    const std::optional<std::string>& patreon_id,
    const std::optional<std::string>& patreon_email,
    const std::optional<std::string>& patreon_member_id,
    const std::optional<std::string>& patreon_status,
    const std::optional<std::string>& patreon_tier,
    bool patreon_is_supporter,
    const std::optional<std::string>& patreon_linked_at) {
  static constexpr const char* kSql =
      "UPDATE users SET "
      "patreon_id = ?, "
      "patreon_email = ?, "
      "patreon_member_id = ?, "
      "patreon_status = ?, "
      "patreon_tier = ?, "
      "patreon_is_supporter = ?, "
      "patreon_linked_at = ? "
      "WHERE id = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_link_patreon_prepare_failed"));
  }
  bind_optional_text(stmt, 1, patreon_id);
  bind_optional_text(stmt, 2, patreon_email);
  bind_optional_text(stmt, 3, patreon_member_id);
  bind_optional_text(stmt, 4, patreon_status);
  bind_optional_text(stmt, 5, patreon_tier);
  sqlite3_bind_int(stmt, 6, patreon_is_supporter ? 1 : 0);
  bind_optional_text(stmt, 7, patreon_linked_at);
  sqlite3_bind_int64(stmt, 8, user_id);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "identity_link_patreon_failed"));
  }
  sqlite3_finalize(stmt);
  static constexpr const char* kLookupSql =
      "SELECT id, username, email, email_verified, display_name, avatar_url, bio, locale, password_salt_b64, password_hash_b64, password_iterations, two_factor_enabled, two_factor_secret_b32, role, permissions_json, disabled_at, "
      "patreon_id, patreon_email, patreon_member_id, patreon_status, patreon_tier, patreon_is_supporter, patreon_linked_at "
      "FROM users WHERE id = ? LIMIT 1;";
  sqlite3_stmt* lookup = nullptr;
  if (sqlite3_prepare_v2(db_, kLookupSql, -1, &lookup, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_link_patreon_lookup_prepare_failed"));
  }
  sqlite3_bind_int64(lookup, 1, user_id);
  if (sqlite3_step(lookup) == SQLITE_ROW) {
    auto user = loaded_user_from_stmt(lookup);
    sqlite3_finalize(lookup);
    return user;
  }
  sqlite3_finalize(lookup);
  return std::nullopt;
}

std::optional<IdentityStore::LoadedUser> IdentityStore::unlink_patreon_account(std::int64_t user_id) {
  static constexpr const char* kSql =
      "UPDATE users SET "
      "patreon_id = NULL, "
      "patreon_email = NULL, "
      "patreon_member_id = NULL, "
      "patreon_status = NULL, "
      "patreon_tier = NULL, "
      "patreon_is_supporter = 0, "
      "patreon_linked_at = NULL "
      "WHERE id = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_unlink_patreon_prepare_failed"));
  }
  sqlite3_bind_int64(stmt, 1, user_id);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "identity_unlink_patreon_failed"));
  }
  sqlite3_finalize(stmt);
  static constexpr const char* kLookupSql =
      "SELECT id, username, email, email_verified, display_name, avatar_url, bio, locale, password_salt_b64, password_hash_b64, password_iterations, two_factor_enabled, two_factor_secret_b32, role, permissions_json, disabled_at, "
      "patreon_id, patreon_email, patreon_member_id, patreon_status, patreon_tier, patreon_is_supporter, patreon_linked_at "
      "FROM users WHERE id = ? LIMIT 1;";
  sqlite3_stmt* lookup = nullptr;
  if (sqlite3_prepare_v2(db_, kLookupSql, -1, &lookup, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_unlink_patreon_lookup_prepare_failed"));
  }
  sqlite3_bind_int64(lookup, 1, user_id);
  if (sqlite3_step(lookup) == SQLITE_ROW) {
    auto user = loaded_user_from_stmt(lookup);
    sqlite3_finalize(lookup);
    return user;
  }
  sqlite3_finalize(lookup);
  return std::nullopt;
}

std::vector<IdentityStore::GameKeyRecord> IdentityStore::generate_game_keys(
    std::size_t count,
    const std::vector<std::string>& entitlements,
    const std::string& notes) {
  static constexpr const char* kSql =
      "INSERT INTO game_keys (key_code, status, entitlements_json, created_at, notes) "
      "VALUES (?, 'free', ?, ?, ?);";
  const auto normalized_entitlements = normalize_entitlements(entitlements);
  const auto entitlements_json = serialize_string_array_json(normalized_entitlements);
  const auto created_at = utc_timestamp_now();
  std::vector<GameKeyRecord> generated;
  generated.reserve(count);
  for (std::size_t index = 0; index < count; ++index) {
    bool inserted = false;
    for (int attempts = 0; attempts < 16 && !inserted; ++attempts) {
      const auto key_code = random_game_key_code();
      sqlite3_stmt* stmt = nullptr;
      if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
        throw std::runtime_error(sqlite_error(db_, "identity_game_key_generate_prepare_failed"));
      }
      sqlite3_bind_text(stmt, 1, key_code.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 2, entitlements_json.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 3, created_at.c_str(), -1, SQLITE_TRANSIENT);
      sqlite3_bind_text(stmt, 4, notes.c_str(), -1, SQLITE_TRANSIENT);
      const auto rc = sqlite3_step(stmt);
      if (rc == SQLITE_DONE) {
        generated.push_back(GameKeyRecord{
            .key_id = sqlite3_last_insert_rowid(db_),
            .key_code = key_code,
            .status = "free",
            .entitlements = normalized_entitlements,
            .created_at = created_at,
            .notes = notes,
        });
        inserted = true;
      }
      sqlite3_finalize(stmt);
      if (inserted) {
        break;
      }
      const auto message = sqlite_error(db_, "identity_game_key_generate_failed");
      if (message.find("UNIQUE constraint failed") == std::string::npos) {
        throw std::runtime_error(message);
      }
    }
    if (!inserted) {
      throw std::runtime_error("identity_game_key_generation_exhausted");
    }
  }
  return generated;
}

std::optional<IdentityStore::GameKeyRecord> IdentityStore::find_game_key_by_code(const std::string& key_code) const {
  const auto normalized_key = normalize_game_key_code(key_code);
  if (!normalized_key.has_value()) {
    return std::nullopt;
  }
  static constexpr const char* kSql =
      "SELECT gk.id, gk.key_code, gk.status, gk.entitlements_json, gk.created_at, gk.activated_at, gk.deactivated_at, "
      "gk.bound_user_id, u.username, gk.notes "
      "FROM game_keys gk "
      "LEFT JOIN users u ON u.id = gk.bound_user_id "
      "WHERE gk.key_code = ? LIMIT 1;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_game_key_lookup_prepare_failed"));
  }
  sqlite3_bind_text(stmt, 1, normalized_key->c_str(), -1, SQLITE_TRANSIENT);
  const auto rc = sqlite3_step(stmt);
  if (rc == SQLITE_ROW) {
    auto record = game_key_record_from_stmt(stmt);
    sqlite3_finalize(stmt);
    return record;
  }
  sqlite3_finalize(stmt);
  return std::nullopt;
}

std::vector<IdentityStore::GameKeyRecord> IdentityStore::list_game_keys(
    std::size_t limit,
    const std::optional<std::string>& status,
    const std::optional<std::string>& entitlement,
    const std::optional<std::string>& username,
    std::size_t offset) const {
  std::string sql =
      "SELECT gk.id, gk.key_code, gk.status, gk.entitlements_json, gk.created_at, gk.activated_at, gk.deactivated_at, "
      "gk.bound_user_id, u.username, gk.notes "
      "FROM game_keys gk "
      "LEFT JOIN users u ON u.id = gk.bound_user_id";
  std::vector<std::string> conditions;
  if (status.has_value()) {
    conditions.emplace_back("gk.status = ?");
  }
  if (entitlement.has_value()) {
    conditions.emplace_back("lower(gk.entitlements_json) LIKE ?");
  }
  if (username.has_value()) {
    conditions.emplace_back("lower(u.username) = ?");
  }
  if (!conditions.empty()) {
    sql += " WHERE ";
    for (std::size_t index = 0; index < conditions.size(); ++index) {
      if (index > 0) {
        sql += " AND ";
      }
      sql += conditions[index];
    }
  }
  sql += " ORDER BY gk.id DESC LIMIT ? OFFSET ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_game_key_list_prepare_failed"));
  }
  int bind_index = 1;
  if (status.has_value()) {
    const auto normalized_status = normalize_game_key_status(*status);
    sqlite3_bind_text(stmt, bind_index++, normalized_status.c_str(), -1, SQLITE_TRANSIENT);
  }
  if (entitlement.has_value()) {
    const auto pattern = "%\"" + lower(trim(*entitlement)) + "\"%";
    sqlite3_bind_text(stmt, bind_index++, pattern.c_str(), -1, SQLITE_TRANSIENT);
  }
  if (username.has_value()) {
    const auto normalized_username = lower(trim(*username));
    sqlite3_bind_text(stmt, bind_index++, normalized_username.c_str(), -1, SQLITE_TRANSIENT);
  }
  sqlite3_bind_int64(stmt, bind_index++, static_cast<sqlite3_int64>(limit));
  sqlite3_bind_int64(stmt, bind_index, static_cast<sqlite3_int64>(offset));
  std::vector<GameKeyRecord> keys;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    keys.push_back(game_key_record_from_stmt(stmt));
  }
  sqlite3_finalize(stmt);
  std::reverse(keys.begin(), keys.end());
  return keys;
}

std::vector<IdentityStore::GameKeyRecord> IdentityStore::list_user_game_keys(std::int64_t user_id) const {
  static constexpr const char* kSql =
      "SELECT gk.id, gk.key_code, gk.status, gk.entitlements_json, gk.created_at, gk.activated_at, gk.deactivated_at, "
      "gk.bound_user_id, u.username, gk.notes "
      "FROM game_keys gk "
      "LEFT JOIN users u ON u.id = gk.bound_user_id "
      "WHERE gk.bound_user_id = ? "
      "ORDER BY gk.id DESC;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_user_game_keys_prepare_failed"));
  }
  sqlite3_bind_int64(stmt, 1, user_id);
  std::vector<GameKeyRecord> keys;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    keys.push_back(game_key_record_from_stmt(stmt));
  }
  sqlite3_finalize(stmt);
  std::reverse(keys.begin(), keys.end());
  return keys;
}

IdentityStore::GameKeyActivationResult IdentityStore::activate_game_key(std::int64_t user_id, const std::string& key_code) {
  const auto normalized_key = normalize_game_key_code(key_code);
  if (!normalized_key.has_value()) {
    return {.outcome = GameKeyActivationResult::Outcome::InvalidFormat};
  }
  const auto existing = find_game_key_by_code(*normalized_key);
  if (!existing.has_value()) {
    return {.outcome = GameKeyActivationResult::Outcome::NotFound};
  }
  if (existing->status == "deactivated") {
    return {.outcome = GameKeyActivationResult::Outcome::Deactivated, .key = existing};
  }
  if (existing->status == "activated") {
    if (existing->bound_user_id.has_value() && *existing->bound_user_id == user_id) {
      return {.outcome = GameKeyActivationResult::Outcome::AlreadyActivatedBySelf, .key = existing};
    }
    return {.outcome = GameKeyActivationResult::Outcome::ActivatedByOther, .key = existing};
  }
  static constexpr const char* kSql =
      "UPDATE game_keys SET status = 'activated', activated_at = ?, deactivated_at = NULL, bound_user_id = ? "
      "WHERE key_code = ? AND status = 'free' AND bound_user_id IS NULL;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_activate_game_key_prepare_failed"));
  }
  const auto activated_at = utc_timestamp_now();
  sqlite3_bind_text(stmt, 1, activated_at.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int64(stmt, 2, user_id);
  sqlite3_bind_text(stmt, 3, normalized_key->c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "identity_activate_game_key_failed"));
  }
  sqlite3_finalize(stmt);
  const auto updated = find_game_key_by_code(*normalized_key);
  if (!updated.has_value()) {
    return {.outcome = GameKeyActivationResult::Outcome::NotFound};
  }
  if (updated->status == "activated" && updated->bound_user_id.has_value() && *updated->bound_user_id == user_id) {
    return {.outcome = GameKeyActivationResult::Outcome::Activated, .key = updated};
  }
  if (updated->status == "activated") {
    return {.outcome = GameKeyActivationResult::Outcome::ActivatedByOther, .key = updated};
  }
  if (updated->status == "deactivated") {
    return {.outcome = GameKeyActivationResult::Outcome::Deactivated, .key = updated};
  }
  return {.outcome = GameKeyActivationResult::Outcome::NotFound, .key = updated};
}

std::optional<IdentityStore::GameKeyRecord> IdentityStore::update_game_key(
    const std::string& key_code,
    const std::optional<std::string>& status,
    const std::optional<std::vector<std::string>>& entitlements,
    const std::optional<std::string>& notes) {
  const auto normalized_key = normalize_game_key_code(key_code);
  if (!normalized_key.has_value()) {
    throw std::runtime_error("invalid_game_key_code");
  }
  const auto current = find_game_key_by_code(*normalized_key);
  if (!current.has_value()) {
    return std::nullopt;
  }
  const auto next_status = status.has_value() ? normalize_game_key_status(*status) : current->status;
  const auto next_entitlements = entitlements.has_value() ? normalize_entitlements(*entitlements) : current->entitlements;
  const auto next_notes = notes.has_value() ? *notes : current->notes;
  const auto now = utc_timestamp_now();
  std::optional<std::string> activated_at = current->activated_at;
  std::optional<std::string> deactivated_at = current->deactivated_at;
  if (next_status == "deactivated") {
    deactivated_at = now;
  } else if (current->status == "deactivated" && next_status != "deactivated") {
    deactivated_at = std::nullopt;
  }
  if (next_status == "free") {
    activated_at = std::nullopt;
  } else if (next_status == "activated" && !activated_at.has_value()) {
    activated_at = now;
  }
  const bool clear_binding = next_status == "free";
  static constexpr const char* kSql =
      "UPDATE game_keys SET status = ?, entitlements_json = ?, activated_at = ?, deactivated_at = ?, "
      "bound_user_id = CASE WHEN ? THEN NULL ELSE bound_user_id END, notes = ? "
      "WHERE key_code = ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_update_game_key_prepare_failed"));
  }
  const auto entitlements_json = serialize_string_array_json(next_entitlements);
  sqlite3_bind_text(stmt, 1, next_status.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 2, entitlements_json.c_str(), -1, SQLITE_TRANSIENT);
  bind_optional_text(stmt, 3, activated_at);
  bind_optional_text(stmt, 4, deactivated_at);
  sqlite3_bind_int(stmt, 5, clear_binding ? 1 : 0);
  sqlite3_bind_text(stmt, 6, next_notes.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 7, normalized_key->c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "identity_update_game_key_failed"));
  }
  sqlite3_finalize(stmt);
  return find_game_key_by_code(*normalized_key);
}

bool IdentityStore::user_has_active_entitlement(std::int64_t user_id, const std::string& entitlement) const {
  const auto normalized_entitlement = lower(trim(entitlement));
  if (normalized_entitlement.empty()) {
    return false;
  }
  for (const auto& key : list_user_game_keys(user_id)) {
    if (key.status != "activated") {
      continue;
    }
    if (std::find(key.entitlements.begin(), key.entitlements.end(), normalized_entitlement) != key.entitlements.end()) {
      return true;
    }
  }
  return false;
}

std::vector<std::string> IdentityStore::list_user_active_entitlements(std::int64_t user_id) const {
  std::set<std::string> merged;
  for (const auto& key : list_user_game_keys(user_id)) {
    if (key.status != "activated") {
      continue;
    }
    merged.insert(key.entitlements.begin(), key.entitlements.end());
  }
  return {merged.begin(), merged.end()};
}

void IdentityStore::append_auth_audit(std::int64_t user_id, const std::string& event_type, const nlohmann::json& payload) {
  static constexpr const char* kSql =
      "INSERT INTO auth_audit (user_id, event_type, payload_json, created_at) VALUES (?, ?, ?, ?);";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_auth_audit_insert_prepare_failed"));
  }
  const auto created_at = utc_timestamp_now();
  const auto payload_json = payload.dump();
  sqlite3_bind_int64(stmt, 1, user_id);
  sqlite3_bind_text(stmt, 2, event_type.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 3, payload_json.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt, 4, created_at.c_str(), -1, SQLITE_TRANSIENT);
  if (sqlite3_step(stmt) != SQLITE_DONE) {
    sqlite3_finalize(stmt);
    throw std::runtime_error(sqlite_error(db_, "identity_auth_audit_insert_failed"));
  }
  sqlite3_finalize(stmt);
}

std::vector<IdentityStore::AuthAuditRecord> IdentityStore::list_auth_audit(std::int64_t user_id, std::size_t limit) const {
  static constexpr const char* kSql =
      "SELECT id, user_id, event_type, payload_json, created_at "
      "FROM auth_audit WHERE user_id = ? ORDER BY id DESC LIMIT ?;";
  sqlite3_stmt* stmt = nullptr;
  if (sqlite3_prepare_v2(db_, kSql, -1, &stmt, nullptr) != SQLITE_OK) {
    throw std::runtime_error(sqlite_error(db_, "identity_auth_audit_list_prepare_failed"));
  }
  sqlite3_bind_int64(stmt, 1, user_id);
  sqlite3_bind_int(stmt, 2, static_cast<int>(limit));
  std::vector<AuthAuditRecord> records;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    records.push_back(AuthAuditRecord{
        .event_id = sqlite3_column_int64(stmt, 0),
        .user_id = sqlite3_column_int64(stmt, 1),
        .event_type = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 2)),
        .created_at = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4)),
        .payload = nlohmann::json::parse(reinterpret_cast<const char*>(sqlite3_column_text(stmt, 3))),
    });
  }
  sqlite3_finalize(stmt);
  return records;
}

IdentityService::IdentityService(IdentityServiceConfig config)
    : config_(std::move(config)), store_(config_.sqlite_path) {
  write_state_file();
}

nlohmann::json IdentityService::handle(
    const std::string& method,
    const std::string& path,
    const std::optional<std::string>& bearer_token,
    const std::optional<nlohmann::json>& body,
    const IdentityRequestContext& context) const {
  record_request();
  try {
    const auto [normalized_path, query] = split_path_and_query(path);
    if (normalized_path == "/health") {
      return {
          {"ok", true},
          {"service", "sekailink_identity_service"},
      };
    }
    if (method == "POST" && normalized_path == "/register") {
      if (!body.has_value()) {
        return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      }
      return handle_register(*body, context);
    }
    if (method == "POST" && normalized_path == "/login") {
      if (!body.has_value()) {
        return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      }
      return handle_login(*body, context);
    }
    if (method == "POST" && normalized_path == "/password-recovery/request") {
      if (!body.has_value()) {
        return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      }
      return handle_password_reset_request(*body);
    }
    if (method == "POST" && normalized_path == "/email-verification/complete") {
      if (!body.has_value()) {
        return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      }
      return handle_email_verification_complete(*body);
    }
    if (method == "POST" && normalized_path == "/password-recovery/complete") {
      if (!body.has_value()) {
        return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      }
      return handle_password_reset_complete(*body);
    }
    if (method == "GET" && normalized_path == "/me") {
      return handle_me(bearer_token);
    }
    if (method == "GET" && normalized_path == "/me/sessions") {
      return handle_sessions(bearer_token);
    }
    if (method == "POST" && normalized_path == "/me/sessions/revoke") {
      if (!body.has_value()) {
        return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      }
      return handle_revoke_session(bearer_token, *body);
    }
    if (method == "POST" && normalized_path == "/me/sessions/revoke-others") {
      return handle_revoke_other_sessions(bearer_token);
    }
    if (method == "PATCH" && normalized_path == "/me/profile") {
      if (!body.has_value()) {
        return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      }
      return handle_update_profile(bearer_token, *body);
    }
    if (method == "GET" && normalized_path == "/me/security") {
      return handle_security(bearer_token);
    }
    if (method == "POST" && normalized_path == "/me/email-verification/request") {
      return handle_email_verification_request(bearer_token);
    }
    if (method == "GET" && normalized_path == "/me/linked-accounts/patreon") {
      return handle_patreon_link_status(bearer_token);
    }
    if (method == "POST" && normalized_path == "/me/linked-accounts/patreon/begin") {
      return handle_patreon_link_begin(bearer_token);
    }
    if (method == "POST" && normalized_path == "/me/linked-accounts/patreon/complete") {
      if (!body.has_value()) {
        return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      }
      return handle_patreon_link_complete(bearer_token, *body);
    }
    if (method == "POST" && normalized_path == "/me/linked-accounts/patreon/unlink") {
      return handle_patreon_link_unlink(bearer_token);
    }
    if (method == "GET" && normalized_path == "/me/game-keys") {
      return handle_my_game_keys(bearer_token);
    }
    if (method == "POST" && normalized_path == "/me/game-keys/activate") {
      if (!body.has_value()) {
        return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      }
      return handle_my_game_key_activate(bearer_token, *body);
    }
    if (method == "POST" && normalized_path == "/me/game-keys/check") {
      if (!body.has_value()) {
        return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      }
      return handle_my_game_key_check(bearer_token, *body);
    }
    if (method == "POST" && normalized_path == "/game-keys/lookup") {
      if (!body.has_value()) {
        return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      }
      return handle_game_key_lookup(*body);
    }
    if (method == "GET" && normalized_path == "/me/security/audit") {
      return handle_auth_audit(bearer_token);
    }
    if (method == "POST" && normalized_path == "/me/security/2fa/setup") {
      return handle_two_factor_setup(bearer_token);
    }
    if (method == "POST" && normalized_path == "/me/security/2fa/enable") {
      if (!body.has_value()) {
        return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      }
      return handle_two_factor_enable(bearer_token, *body);
    }
    if (method == "POST" && normalized_path == "/me/security/2fa/disable") {
      if (!body.has_value()) {
        return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
      }
      return handle_two_factor_disable(bearer_token, *body);
    }
    if (method == "POST" && normalized_path == "/me/security/recovery-codes/regenerate") {
      return handle_regenerate_recovery_codes(bearer_token);
    }
    const auto parts = split_path(normalized_path);
    if (parts.size() == 2 && parts[0] == "admin" && parts[1] == "users") {
      if (method == "GET") {
        return handle_admin_list_users(
            bearer_token,
            context,
            normalized_query_text(query),
            normalized_query_value(query, "role"),
            normalized_state_filter(query),
            normalized_limit(query),
            normalized_offset(query));
      }
      if (method == "POST") {
        if (!body.has_value()) {
          return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
        }
        return handle_admin_add_user(bearer_token, *body, context);
      }
    }
    if (parts.size() == 3 && parts[0] == "admin" && parts[1] == "game-keys" && parts[2] == "generate") {
      if (method == "POST") {
        if (!body.has_value()) {
          return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
        }
        return handle_admin_generate_game_keys(bearer_token, *body, context);
      }
    }
    if (parts.size() == 2 && parts[0] == "admin" && parts[1] == "game-keys") {
      if (method == "GET") {
        return handle_admin_list_game_keys(
            bearer_token,
            context,
            normalized_query_value(query, "status"),
            normalized_query_value(query, "entitlement"),
            normalized_query_value(query, "username"),
            normalized_limit(query),
            normalized_offset(query));
      }
    }
    if (parts.size() == 3 && parts[0] == "admin" && parts[1] == "game-keys") {
      if (method == "GET") {
        return handle_admin_game_key_info(bearer_token, parts[2], context);
      }
      if (method == "PATCH") {
        if (!body.has_value()) {
          return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
        }
        return handle_admin_update_game_key(bearer_token, parts[2], *body, context);
      }
    }
    if (parts.size() == 3 && parts[0] == "admin" && parts[1] == "users") {
      if (method == "GET") {
        return handle_admin_user_info(bearer_token, parts[2], context);
      }
      if (method == "PATCH") {
        if (!body.has_value()) {
          return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
        }
        return handle_admin_edit_user(bearer_token, parts[2], *body, context);
      }
      if (method == "DELETE") {
        return handle_admin_delete_user(bearer_token, parts[2], context);
      }
    }
    if (parts.size() == 4 && parts[0] == "admin" && parts[1] == "users" && parts[3] == "sessions") {
      if (method == "GET") {
        return handle_admin_list_user_sessions(bearer_token, parts[2], context);
      }
    }
    if (parts.size() == 4 && parts[0] == "admin" && parts[1] == "users" && parts[3] == "audit") {
      if (method == "GET") {
        return handle_admin_list_user_audit(
            bearer_token,
            parts[2],
            context,
            normalized_query_value(query, "event_type"),
            normalized_limit(query),
            normalized_offset(query));
      }
    }
    if (parts.size() == 4 && parts[0] == "admin" && parts[1] == "users" && parts[3] == "devices") {
      if (method == "GET") {
        return handle_admin_list_user_devices(bearer_token, parts[2], context);
      }
    }
    if (parts.size() == 5 && parts[0] == "admin" && parts[1] == "users" && parts[3] == "sessions") {
      if (method == "POST" && parts[4] == "revoke-others") {
        return handle_admin_revoke_other_user_sessions(bearer_token, parts[2], context);
      }
    }
    if (parts.size() == 6 && parts[0] == "admin" && parts[1] == "users" && parts[3] == "sessions") {
      if (method == "POST" && parts[5] == "revoke") {
        try {
          return handle_admin_revoke_user_session(bearer_token, parts[2], std::stoll(parts[4]), context);
        } catch (const std::exception&) {
          return {{"ok", false}, {"status", 400}, {"error", "invalid_session_id"}};
        }
      }
    }
    if (parts.size() == 5 && parts[0] == "admin" && parts[1] == "users" && parts[3] == "devices") {
      if (method == "POST" && parts[4] == "revoke") {
        if (!body.has_value()) {
          return {{"ok", false}, {"status", 400}, {"error", "missing_body"}};
        }
        return handle_admin_revoke_user_device_sessions(bearer_token, parts[2], *body, context);
      }
    }
    if (parts.size() == 4 && parts[0] == "admin" && parts[1] == "users" && parts[3] == "password-reset") {
      if (method == "POST") {
        return handle_admin_force_password_reset(bearer_token, parts[2], context);
      }
    }
    if (method == "GET" && parts.size() == 2 && parts[0] == "profiles") {
      return handle_profile(parts[1]);
    }
    return {
        {"ok", false},
        {"status", 404},
        {"error", "route_not_found"},
    };
  } catch (...) {
    record_error();
    throw;
  }
}

nlohmann::json IdentityService::handle_admin_list_users(
    const std::optional<std::string>& bearer_token,
    const IdentityRequestContext&,
    const std::optional<std::string>& query,
    const std::optional<std::string>& role,
    const std::optional<bool>& disabled,
    std::size_t limit,
    std::size_t offset) const {
  if (!is_admin_authorized(bearer_token)) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  nlohmann::json users = nlohmann::json::array();
  for (const auto& user : store_.list_users(limit, query, role, disabled, offset)) {
    users.push_back({
        {"user_id", user.user_id},
        {"username", user.username},
        {"email", user.email},
        {"email_verified", user.email_verified},
        {"display_name", user.display_name},
        {"avatar_url", user.avatar_url},
        {"bio", user.bio},
        {"locale", user.locale},
        {"role", user.role},
        {"permissions", user.permissions},
        {"disabled_at", user.disabled_at.has_value() ? nlohmann::json(*user.disabled_at) : nlohmann::json(nullptr)},
    });
  }
  return {
      {"ok", true},
      {"limit", limit},
      {"offset", offset},
      {"query", query.has_value() ? nlohmann::json(*query) : nlohmann::json(nullptr)},
      {"role", role.has_value() ? nlohmann::json(*role) : nlohmann::json(nullptr)},
      {"disabled", disabled.has_value() ? nlohmann::json(*disabled) : nlohmann::json(nullptr)},
      {"users", std::move(users)},
  };
}

nlohmann::json IdentityService::handle_register(const nlohmann::json& body, const IdentityRequestContext& context) const {
  try {
    const auto username = required_string(body, "username");
    const auto email = lower(required_string(body, "email"));
    const auto password = required_string(body, "password");
    const auto display_name = body.value("display_name", username);
    const auto avatar_url = body.value("avatar_url", "");
    const auto bio = body.value("bio", "");
    const auto locale = body.value("locale", "");
    if (username.empty() || email.empty() || password.size() < 8) {
      return {{"ok", false}, {"status", 400}, {"error", "invalid_registration_payload"}};
    }
    const auto salt_b64 = random_token_b64(config_.password_salt_length);
    const auto hash_b64 = hash_password_argon2id_encoded(
        password,
        salt_b64,
        config_.password_time_cost,
        config_.password_memory_kib,
        config_.password_parallelism,
        config_.password_hash_length);
    const auto user = store_.create_user(
        username,
        email,
        display_name,
        avatar_url,
        bio,
        locale,
        "player",
        {},
        false,
        salt_b64,
        hash_b64,
        config_.password_time_cost);
    const auto session = store_.create_session(user.user_id, config_.session_ttl_seconds, context);
    bool email_verification_sent = false;
    const auto loaded_user = store_.find_user_by_identity(user.username);
    if (loaded_user.has_value()) {
      try {
        const auto raw_token = random_token_b64(32);
        const auto expires_at = compute_future_utc(config_.email_verification_ttl_seconds);
        store_.issue_email_verification(user.user_id, sha256_b64(raw_token), expires_at);
        send_email_verification_email(*loaded_user, raw_token, expires_at);
        email_verification_sent = true;
        store_.append_auth_audit(
            user.user_id,
            "email_verification_requested",
            {{"expires_at", expires_at}, {"source", "register"}});
      } catch (...) {
      }
    }
    store_.append_auth_audit(
        user.user_id,
        "register",
        {
            {"session_id", session.session_id},
            {"email_verification_sent", email_verification_sent},
            {"request_context", request_context_to_json(context)},
        });
    return {
        {"ok", true},
        {"status", 201},
        {"user",
         {
             {"user_id", user.user_id},
             {"username", user.username},
             {"email", user.email},
             {"email_verified", user.email_verified},
             {"display_name", user.display_name},
             {"avatar_url", user.avatar_url},
             {"bio", user.bio},
             {"locale", user.locale},
             {"role", user.role},
             {"permissions", user.permissions},
         }},
        {"session", session_to_json(session)},
        {"game_keys", build_game_key_summary(user.user_id)},
        {"email_verification_sent", email_verification_sent},
    };
  } catch (const std::exception& exception) {
    if (std::string(exception.what()) == "identity_user_conflict") {
      return {{"ok", false}, {"status", 409}, {"error", "identity_user_conflict"}};
    }
    if (std::string(exception.what()).rfind("missing_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

nlohmann::json IdentityService::handle_login(const nlohmann::json& body, const IdentityRequestContext& context) const {
  try {
    const auto identity = required_string(body, "identity");
    const auto password = required_string(body, "password");
    const auto user = store_.find_user_by_identity(identity);
    if (!user.has_value()) {
      return {{"ok", false}, {"status", 401}, {"error", "invalid_credentials"}};
    }
    if (!verify_argon2id_encoded(user->password_hash_b64, password)) {
      return {{"ok", false}, {"status", 401}, {"error", "invalid_credentials"}};
    }
    if (user->disabled_at.has_value()) {
      return {{"ok", false}, {"status", 403}, {"error", "account_disabled"}};
    }
    if (user->two_factor_enabled) {
      const auto two_factor_code = optional_trimmed_string(body, "two_factor_code");
      const auto recovery_code = optional_trimmed_string(body, "recovery_code");
      bool satisfied = false;
      if (two_factor_code.has_value()) {
        satisfied = verify_totp_code(user->two_factor_secret_b32, *two_factor_code);
      }
      if (!satisfied && recovery_code.has_value()) {
        satisfied = store_.consume_recovery_code(user->user_id, *recovery_code);
      }
      if (!satisfied) {
        return {{"ok", false}, {"status", 401}, {"error", "two_factor_required"}};
      }
    }
    const auto session = store_.create_session(user->user_id, config_.session_ttl_seconds, context);
    store_.append_auth_audit(
        user->user_id,
        "login",
        {
            {"session_id", session.session_id},
            {"two_factor_required", user->two_factor_enabled},
            {"request_context", request_context_to_json(context)},
        });
    return {
        {"ok", true},
        {"session", session_to_json(session)},
        {"user", user_to_json(*user)},
        {"game_keys", build_game_key_summary(user->user_id)},
    };
  } catch (const std::exception& exception) {
    if (std::string(exception.what()).rfind("missing_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

nlohmann::json IdentityService::handle_password_reset_request(const nlohmann::json& body) const {
  try {
    const auto identity = required_string(body, "identity");
    const auto generic = nlohmann::json{
        {"ok", true},
        {"message", "If the account exists, a password reset email has been sent."},
    };
    const auto user = store_.find_user_by_identity(identity);
    if (!user.has_value()) {
      return generic;
    }
    const auto raw_token = random_token_b64(32);
    const auto expires_at = compute_future_utc(config_.password_reset_ttl_seconds);
    store_.issue_password_reset(user->user_id, sha256_b64(raw_token), expires_at);
    store_.append_auth_audit(user->user_id, "password_reset_requested", {{"expires_at", expires_at}});
    send_password_reset_email(*user, raw_token, expires_at);
    return generic;
  } catch (const std::exception& exception) {
    if (std::string(exception.what()).rfind("missing_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

nlohmann::json IdentityService::handle_email_verification_request(const std::optional<std::string>& bearer_token) const {
  if (!bearer_token.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto session = store_.find_session(*bearer_token);
  if (!session.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto user = store_.find_user_by_identity(session->username);
  if (!user.has_value()) {
    return {{"ok", false}, {"status", 404}, {"error", "identity_user_not_found"}};
  }
  if (user->email_verified) {
    return {
        {"ok", true},
        {"already_verified", true},
    };
  }
  const auto raw_token = random_token_b64(32);
  const auto expires_at = compute_future_utc(config_.email_verification_ttl_seconds);
  store_.issue_email_verification(user->user_id, sha256_b64(raw_token), expires_at);
  send_email_verification_email(*user, raw_token, expires_at);
  store_.append_auth_audit(
      user->user_id,
      "email_verification_requested",
      {{"expires_at", expires_at}, {"source", "manual"}});
  return {
      {"ok", true},
      {"already_verified", false},
      {"message", "Verification email sent."},
  };
}

nlohmann::json IdentityService::handle_patreon_link_status(const std::optional<std::string>& bearer_token) const {
  if (!bearer_token.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto session = store_.find_session(*bearer_token);
  if (!session.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto user = store_.find_user_by_identity(session->username);
  if (!user.has_value()) {
    return {{"ok", false}, {"status", 404}, {"error", "identity_user_not_found"}};
  }
  return {
      {"ok", true},
      {"patreon", patreon_to_json(*user, true)},
  };
}

nlohmann::json IdentityService::handle_patreon_link_begin(const std::optional<std::string>& bearer_token) const {
  if (!bearer_token.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  if (config_.patreon_client_id.empty() || config_.patreon_client_secret.empty() || config_.patreon_redirect_uri.empty()) {
    return {{"ok", false}, {"status", 503}, {"error", "patreon_oauth_not_configured"}};
  }
  const auto session = store_.find_session(*bearer_token);
  if (!session.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto state = random_token_urlsafe(24);
  const auto expires_at = compute_future_utc(600);
  store_.issue_oauth_link_state(session->user_id, "patreon", sha256_b64(state), expires_at);
  const std::string auth_url =
      "https://www.patreon.com/oauth2/authorize?response_type=code&client_id=" + percent_encode(config_.patreon_client_id) +
      "&redirect_uri=" + percent_encode(config_.patreon_redirect_uri) + "&scope=" + percent_encode(config_.patreon_oauth_scopes) +
      "&state=" + percent_encode(state);
  store_.append_auth_audit(
      session->user_id,
      "patreon_link_started",
      {{"expires_at", expires_at}});
  return {
      {"ok", true},
      {"provider", "patreon"},
      {"authorization_url", auth_url},
      {"state", state},
      {"expires_at", expires_at},
  };
}

nlohmann::json IdentityService::handle_patreon_link_complete(
    const std::optional<std::string>& bearer_token,
    const nlohmann::json& body) const {
  if (!bearer_token.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  if (config_.patreon_client_id.empty() || config_.patreon_client_secret.empty() || config_.patreon_redirect_uri.empty()) {
    return {{"ok", false}, {"status", 503}, {"error", "patreon_oauth_not_configured"}};
  }
  const auto session = store_.find_session(*bearer_token);
  if (!session.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  try {
    const auto code = required_string(body, "code");
    const auto state = required_string(body, "state");
    if (!store_.consume_oauth_link_state(session->user_id, "patreon", state).has_value()) {
      return {{"ok", false}, {"status", 401}, {"error", "invalid_or_expired_oauth_state"}};
    }

    const auto token_body = form_urlencode({
        {"grant_type", "authorization_code"},
        {"code", code},
        {"client_id", config_.patreon_client_id},
        {"client_secret", config_.patreon_client_secret},
        {"redirect_uri", config_.patreon_redirect_uri},
    });
    const auto token_response = https_request(
        "www.patreon.com",
        "/api/oauth2/token",
        "POST",
        {{"Content-Type", "application/x-www-form-urlencoded"}},
        token_body);
    if (token_response.status_code < 200 || token_response.status_code >= 300) {
      return {{"ok", false}, {"status", 502}, {"error", "patreon_token_exchange_failed"}};
    }
    const auto token_json = nlohmann::json::parse(token_response.body);
    const auto access_token = json_optional_string(token_json, "access_token");
    if (!access_token.has_value()) {
      return {{"ok", false}, {"status", 502}, {"error", "patreon_access_token_missing"}};
    }

    const std::string identity_target =
        "/api/oauth2/v2/identity?include=memberships,memberships.currently_entitled_tiers"
        "&fields%5Buser%5D=email,full_name"
        "&fields%5Bmember%5D=patron_status,last_charge_status,currently_entitled_amount_cents,pledge_relationship_start"
        "&fields%5Btier%5D=title";
    const auto identity_response = https_request(
        "www.patreon.com",
        identity_target,
        "GET",
        {{"Authorization", "Bearer " + *access_token}},
        "");
    if (identity_response.status_code < 200 || identity_response.status_code >= 300) {
      return {{"ok", false}, {"status", 502}, {"error", "patreon_identity_fetch_failed"}};
    }
    const auto identity_json = nlohmann::json::parse(identity_response.body);
    const auto support_info = extract_patreon_support_info(identity_json);
    const auto linked_at = utc_timestamp_now();
    const auto updated = store_.link_patreon_account(
        session->user_id,
        support_info.patreon_id,
        support_info.patreon_email,
        support_info.patreon_member_id,
        support_info.patreon_status,
        support_info.patreon_tier,
        support_info.patreon_is_supporter,
        linked_at);
    if (!updated.has_value()) {
      return {{"ok", false}, {"status", 404}, {"error", "identity_user_not_found"}};
    }
    store_.append_auth_audit(
        session->user_id,
        "patreon_link_completed",
        {
            {"patreon_id", support_info.patreon_id.has_value()},
            {"patreon_member_id", support_info.patreon_member_id.has_value()},
            {"patreon_tier", support_info.patreon_tier.has_value() ? nlohmann::json(*support_info.patreon_tier) : nlohmann::json(nullptr)},
            {"patreon_is_supporter", support_info.patreon_is_supporter},
        });
    return {
        {"ok", true},
        {"patreon", patreon_to_json(*updated, true)},
    };
  } catch (const std::exception& exception) {
    if (std::string(exception.what()).rfind("missing_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

nlohmann::json IdentityService::handle_patreon_link_unlink(const std::optional<std::string>& bearer_token) const {
  if (!bearer_token.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto session = store_.find_session(*bearer_token);
  if (!session.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto updated = store_.unlink_patreon_account(session->user_id);
  if (!updated.has_value()) {
    return {{"ok", false}, {"status", 404}, {"error", "identity_user_not_found"}};
  }
  store_.append_auth_audit(session->user_id, "patreon_unlinked", nlohmann::json::object());
  return {
      {"ok", true},
      {"patreon", patreon_to_json(*updated, true)},
  };
}

nlohmann::json IdentityService::handle_my_game_keys(const std::optional<std::string>& bearer_token) const {
  if (!bearer_token.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto session = store_.find_session(*bearer_token);
  if (!session.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  nlohmann::json keys = nlohmann::json::array();
  for (const auto& key : store_.list_user_game_keys(session->user_id)) {
    keys.push_back(game_key_to_json(key, true));
  }
  return {
      {"ok", true},
      {"keys", std::move(keys)},
      {"summary", build_game_key_summary(session->user_id)},
  };
}

nlohmann::json IdentityService::handle_my_game_key_activate(
    const std::optional<std::string>& bearer_token,
    const nlohmann::json& body) const {
  if (!bearer_token.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto session = store_.find_session(*bearer_token);
  if (!session.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  try {
    const auto key_code = required_string(body, "game_key");
    const auto result = store_.activate_game_key(session->user_id, key_code);
    auto payload = nlohmann::json{
        {"key_provided", true},
    };
    if (result.key.has_value()) {
      payload["key_code"] = result.key->key_code;
      payload["status"] = result.key->status;
      payload["bound_user_id"] = result.key->bound_user_id.has_value();
    }
    switch (result.outcome) {
      case IdentityStore::GameKeyActivationResult::Outcome::Activated:
        store_.append_auth_audit(session->user_id, "game_key_activated", payload);
        return {
            {"ok", true},
            {"activated", true},
            {"already_linked", false},
            {"key", result.key.has_value() ? game_key_to_json(*result.key, true) : nlohmann::json(nullptr)},
            {"summary", build_game_key_summary(session->user_id)},
        };
      case IdentityStore::GameKeyActivationResult::Outcome::AlreadyActivatedBySelf:
        return {
            {"ok", true},
            {"activated", false},
            {"already_linked", true},
            {"same_account", true},
            {"key", result.key.has_value() ? game_key_to_json(*result.key, true) : nlohmann::json(nullptr)},
            {"summary", build_game_key_summary(session->user_id)},
        };
      case IdentityStore::GameKeyActivationResult::Outcome::ActivatedByOther:
        return {
            {"ok", false},
            {"status", 409},
            {"error", "game_key_already_linked_to_other_account"},
            {"key", result.key.has_value() ? game_key_to_json(*result.key, false) : nlohmann::json(nullptr)},
        };
      case IdentityStore::GameKeyActivationResult::Outcome::Deactivated:
        return {
            {"ok", false},
            {"status", 403},
            {"error", "game_key_deactivated"},
            {"key", result.key.has_value() ? game_key_to_json(*result.key, true) : nlohmann::json(nullptr)},
        };
      case IdentityStore::GameKeyActivationResult::Outcome::InvalidFormat:
        return {{"ok", false}, {"status", 400}, {"error", "invalid_game_key_code"}};
      case IdentityStore::GameKeyActivationResult::Outcome::NotFound:
      default:
        return {{"ok", false}, {"status", 404}, {"error", "game_key_not_found"}};
    }
  } catch (const std::exception& exception) {
    if (std::string(exception.what()).rfind("missing_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

nlohmann::json IdentityService::handle_my_game_key_check(
    const std::optional<std::string>& bearer_token,
    const nlohmann::json& body) const {
  if (!bearer_token.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto session = store_.find_session(*bearer_token);
  if (!session.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  try {
    const auto key_code = required_string(body, "game_key");
    const auto normalized_key = normalize_game_key_code(key_code);
    if (!normalized_key.has_value()) {
      return {{"ok", false}, {"status", 400}, {"error", "invalid_game_key_code"}};
    }
    const auto key = store_.find_game_key_by_code(*normalized_key);
    if (!key.has_value()) {
      return {
          {"ok", true},
          {"exists", false},
          {"valid_format", true},
          {"normalized_key", *normalized_key},
      };
    }
    const bool linked_to_self = key->bound_user_id.has_value() && *key->bound_user_id == session->user_id;
    const bool linked_to_other = key->bound_user_id.has_value() && *key->bound_user_id != session->user_id;
    return {
        {"ok", true},
        {"exists", true},
        {"valid_format", true},
        {"normalized_key", *normalized_key},
        {"is_real", true},
        {"is_active", key->status == "activated"},
        {"is_usable", key->status == "activated" && linked_to_self},
        {"status", key->status},
        {"linked_to_current_account", linked_to_self},
        {"linked_to_other_account", linked_to_other},
        {"key", game_key_to_json(*key, linked_to_self || !linked_to_other)},
    };
  } catch (const std::exception& exception) {
    if (std::string(exception.what()).rfind("missing_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

nlohmann::json IdentityService::handle_game_key_lookup(const nlohmann::json& body) const {
  try {
    const auto key_code = required_string(body, "game_key");
    const auto normalized_key = normalize_game_key_code(key_code);
    if (!normalized_key.has_value()) {
      return {{"ok", false}, {"status", 400}, {"error", "invalid_game_key_code"}};
    }
    const auto key = store_.find_game_key_by_code(*normalized_key);
    if (!key.has_value()) {
      return {
          {"ok", true},
          {"exists", false},
          {"is_real", false},
          {"normalized_key", *normalized_key},
      };
    }
    return {
        {"ok", true},
        {"exists", true},
        {"is_real", true},
        {"normalized_key", *normalized_key},
        {"status", key->status},
        {"is_active", key->status == "activated"},
        {"is_available", key->status == "free"},
        {"is_deactivated", key->status == "deactivated"},
        {"linked", key->bound_user_id.has_value()},
        {"key", game_key_to_json(*key, false)},
    };
  } catch (const std::exception& exception) {
    if (std::string(exception.what()).rfind("missing_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

nlohmann::json IdentityService::handle_admin_generate_game_keys(
    const std::optional<std::string>& bearer_token,
    const nlohmann::json& body,
    const IdentityRequestContext& context) const {
  if (!is_admin_authorized(bearer_token)) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  try {
    if (!body.contains("count") || !body.at("count").is_number_integer()) {
      return {{"ok", false}, {"status", 400}, {"error", "missing_count"}};
    }
    const auto count = body.at("count").get<int>();
    if (count <= 0 || count > 500) {
      return {{"ok", false}, {"status", 400}, {"error", "invalid_count"}};
    }
    const auto entitlements = normalize_entitlements(optional_string_array(body, "entitlements").value_or(std::vector<std::string>{"sekaiemu"}));
    const auto notes = optional_trimmed_string(body, "notes").value_or(std::string());
    auto generated = store_.generate_game_keys(static_cast<std::size_t>(count), entitlements, notes);
    nlohmann::json keys = nlohmann::json::array();
    for (const auto& key : generated) {
      keys.push_back(game_key_to_json(key, true));
    }
    return {
        {"ok", true},
        {"count", generated.size()},
        {"entitlements", entitlements},
        {"keys", std::move(keys)},
        {"admin_context", admin_request_context_to_json(context)},
    };
  } catch (const std::exception& exception) {
    if (std::string(exception.what()).rfind("invalid_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

nlohmann::json IdentityService::handle_admin_list_game_keys(
    const std::optional<std::string>& bearer_token,
    const IdentityRequestContext&,
    const std::optional<std::string>& status,
    const std::optional<std::string>& entitlement,
    const std::optional<std::string>& username,
    std::size_t limit,
    std::size_t offset) const {
  if (!is_admin_authorized(bearer_token)) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  try {
    nlohmann::json keys = nlohmann::json::array();
    for (const auto& key : store_.list_game_keys(limit, status, entitlement, username, offset)) {
      keys.push_back(game_key_to_json(key, true));
    }
    return {
        {"ok", true},
        {"limit", limit},
        {"offset", offset},
        {"status_filter", status.has_value() ? nlohmann::json(*status) : nlohmann::json(nullptr)},
        {"entitlement_filter", entitlement.has_value() ? nlohmann::json(*entitlement) : nlohmann::json(nullptr)},
        {"username_filter", username.has_value() ? nlohmann::json(*username) : nlohmann::json(nullptr)},
        {"keys", std::move(keys)},
    };
  } catch (const std::exception& exception) {
    if (std::string(exception.what()).rfind("invalid_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

nlohmann::json IdentityService::handle_admin_game_key_info(
    const std::optional<std::string>& bearer_token,
    const std::string& key_code,
    const IdentityRequestContext&) const {
  if (!is_admin_authorized(bearer_token)) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto normalized_key = normalize_game_key_code(key_code);
  if (!normalized_key.has_value()) {
    return {{"ok", false}, {"status", 400}, {"error", "invalid_game_key_code"}};
  }
  const auto key = store_.find_game_key_by_code(*normalized_key);
  if (!key.has_value()) {
    return {{"ok", false}, {"status", 404}, {"error", "game_key_not_found"}};
  }
  return {
      {"ok", true},
      {"key", game_key_to_json(*key, true)},
  };
}

nlohmann::json IdentityService::handle_admin_update_game_key(
    const std::optional<std::string>& bearer_token,
    const std::string& key_code,
    const nlohmann::json& body,
    const IdentityRequestContext& context) const {
  if (!is_admin_authorized(bearer_token)) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  try {
    const auto updated = store_.update_game_key(
        key_code,
        optional_trimmed_string(body, "status"),
        optional_string_array(body, "entitlements"),
        optional_trimmed_string(body, "notes"));
    if (!updated.has_value()) {
      return {{"ok", false}, {"status", 404}, {"error", "game_key_not_found"}};
    }
    return {
        {"ok", true},
        {"key", game_key_to_json(*updated, true)},
        {"admin_context", admin_request_context_to_json(context)},
    };
  } catch (const std::exception& exception) {
    if (std::string(exception.what()).rfind("invalid_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

nlohmann::json IdentityService::handle_email_verification_complete(const nlohmann::json& body) const {
  try {
    const auto token = required_string(body, "token");
    const auto record = store_.find_email_verification(token);
    if (!record.has_value()) {
      return {{"ok", false}, {"status", 401}, {"error", "invalid_or_expired_verification_token"}};
    }
    store_.mark_email_verified(record->user_id);
    store_.consume_email_verification(record->user_id, token);
    store_.append_auth_audit(record->user_id, "email_verified", nlohmann::json::object());
    return {
        {"ok", true},
        {"message", "Email verified."},
    };
  } catch (const std::exception& exception) {
    if (std::string(exception.what()).rfind("missing_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

nlohmann::json IdentityService::handle_password_reset_complete(const nlohmann::json& body) const {
  try {
    const auto token = required_string(body, "token");
    const auto new_password = required_string(body, "new_password");
    if (new_password.size() < 8) {
      return {{"ok", false}, {"status", 400}, {"error", "invalid_new_password"}};
    }
    const auto reset_record = store_.find_password_reset(token);
    if (!reset_record.has_value()) {
      return {{"ok", false}, {"status", 401}, {"error", "invalid_or_expired_reset_token"}};
    }
    const auto salt_b64 = random_token_b64(config_.password_salt_length);
    const auto hash_b64 = hash_password_argon2id_encoded(
        new_password,
        salt_b64,
        config_.password_time_cost,
        config_.password_memory_kib,
        config_.password_parallelism,
        config_.password_hash_length);
    store_.update_password(reset_record->user_id, salt_b64, hash_b64, config_.password_time_cost);
    store_.consume_password_reset(reset_record->user_id, token);
    store_.revoke_all_sessions(reset_record->user_id);
    store_.append_auth_audit(reset_record->user_id, "password_reset_completed", nlohmann::json::object());
    return {
        {"ok", true},
        {"message", "Password reset completed. Please sign in again."},
    };
  } catch (const std::exception& exception) {
    if (std::string(exception.what()).rfind("missing_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

bool IdentityService::is_admin_authorized(const std::optional<std::string>& bearer_token) const {
  return bearer_token.has_value() && !config_.admin_token.empty() && *bearer_token == config_.admin_token;
}

nlohmann::json IdentityService::handle_admin_add_user(
    const std::optional<std::string>& bearer_token,
    const nlohmann::json& body,
    const IdentityRequestContext& context) const {
  if (!is_admin_authorized(bearer_token)) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  try {
    const auto username = required_string(body, "username");
    const auto email = lower(required_string(body, "email"));
    const auto password = required_string(body, "password");
    const auto display_name = body.value("display_name", username);
    const auto avatar_url = body.value("avatar_url", "");
    const auto bio = body.value("bio", "");
    const auto locale = body.value("locale", "");
    const auto role = body.value("role", "player");
    const auto permissions = optional_permissions(body, "permissions").value_or(std::vector<std::string>{});
    const auto email_verified = optional_bool(body, "email_verified").value_or(false);
    if (username.empty() || email.empty() || password.size() < 8) {
      return {{"ok", false}, {"status", 400}, {"error", "invalid_registration_payload"}};
    }
    const auto salt_b64 = random_token_b64(config_.password_salt_length);
    const auto hash_b64 = hash_password_argon2id_encoded(
        password,
        salt_b64,
        config_.password_time_cost,
        config_.password_memory_kib,
        config_.password_parallelism,
        config_.password_hash_length);
    const auto user = store_.create_user(
        username,
        email,
        display_name,
        avatar_url,
        bio,
        locale,
        role,
        permissions,
        email_verified,
        salt_b64,
        hash_b64,
        config_.password_time_cost);
    const auto loaded = store_.find_user_by_username(user.username);
    store_.append_auth_audit(
        user.user_id,
        "admin_user_created",
        {
            {"role", role},
            {"permissions", permissions},
            {"email_verified", email_verified},
            {"admin_context", admin_request_context_to_json(context)},
        });
    return {{"ok", true}, {"user", loaded.has_value() ? user_to_json(*loaded) : nlohmann::json::object()}};
  } catch (const std::exception& exception) {
    if (std::string(exception.what()) == "identity_user_conflict") {
      return {{"ok", false}, {"status", 409}, {"error", "identity_user_conflict"}};
    }
    if (std::string(exception.what()).rfind("missing_", 0) == 0 ||
        std::string(exception.what()).rfind("invalid_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

nlohmann::json IdentityService::handle_admin_user_info(
    const std::optional<std::string>& bearer_token,
    const std::string& username,
    const IdentityRequestContext& context) const {
  if (!is_admin_authorized(bearer_token)) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto user = store_.find_user_by_username(username);
  if (!user.has_value()) {
    return {{"ok", false}, {"status", 404}, {"error", "user_not_found"}};
  }
  store_.append_auth_audit(
      user->user_id,
      "admin_user_info_viewed",
      {
          {"admin_context", admin_request_context_to_json(context)},
      });
  auto sessions = nlohmann::json::array();
  const auto session_records = store_.list_sessions(user->user_id);
  for (const auto& session : session_records) {
    auto json = session_to_json(session);
    json["current"] = false;
    sessions.push_back(std::move(json));
  }
  const auto security = store_.security_state(user->user_id);
  auto audit = nlohmann::json::array();
  for (const auto& record : store_.list_auth_audit(user->user_id, 50)) {
    audit.push_back({
        {"event_id", record.event_id},
        {"event_type", record.event_type},
        {"created_at", record.created_at},
        {"payload", record.payload},
    });
  }
  return {
      {"ok", true},
      {"user", user_to_json(*user)},
      {"game_keys", build_game_key_summary(user->user_id)},
      {"sessions", std::move(sessions)},
      {"session_inventory", build_session_inventory_json(session_records)},
      {"security",
       {
           {"email_verified", user->email_verified},
           {"role", user->role},
           {"permissions", user->permissions},
           {"two_factor_enabled", security.two_factor_enabled},
           {"recovery_code_count", security.recovery_code_count},
       }},
      {"auth_audit", std::move(audit)},
  };
}

nlohmann::json IdentityService::handle_admin_list_user_sessions(
    const std::optional<std::string>& bearer_token,
    const std::string& username,
    const IdentityRequestContext& context) const {
  if (!is_admin_authorized(bearer_token)) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto user = store_.find_user_by_username(username);
  if (!user.has_value()) {
    return {{"ok", false}, {"status", 404}, {"error", "user_not_found"}};
  }
  store_.append_auth_audit(
      user->user_id,
      "admin_user_sessions_viewed",
      {
          {"admin_context", admin_request_context_to_json(context)},
      });
  auto sessions = nlohmann::json::array();
  const auto session_records = store_.list_sessions(user->user_id);
  for (const auto& session : session_records) {
    auto json = session_to_json(session);
    json["current"] = false;
    sessions.push_back(std::move(json));
  }
  return {
      {"ok", true},
      {"user", user_to_json(*user)},
      {"sessions", std::move(sessions)},
      {"session_inventory", build_session_inventory_json(session_records)},
  };
}

nlohmann::json IdentityService::handle_admin_list_user_audit(
    const std::optional<std::string>& bearer_token,
    const std::string& username,
    const IdentityRequestContext& context,
    const std::optional<std::string>& event_type,
    std::size_t limit,
    std::size_t offset) const {
  if (!is_admin_authorized(bearer_token)) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto user = store_.find_user_by_username(username);
  if (!user.has_value()) {
    return {{"ok", false}, {"status", 404}, {"error", "user_not_found"}};
  }
  const auto lowered_event_type = event_type.has_value() ? std::optional<std::string>(lower(*event_type)) : std::nullopt;
  std::vector<IdentityStore::AuthAuditRecord> filtered;
  const auto records = store_.list_auth_audit(user->user_id, std::max<std::size_t>(limit + offset, 200));
  filtered.reserve(records.size());
  for (const auto& record : records) {
    if (lowered_event_type.has_value() && lower(record.event_type) != *lowered_event_type) {
      continue;
    }
    filtered.push_back(record);
  }
  const auto bounded_offset = std::min(offset, filtered.size());
  filtered.erase(filtered.begin(), filtered.begin() + static_cast<std::ptrdiff_t>(bounded_offset));
  if (filtered.size() > limit) {
    filtered.resize(limit);
  }
  store_.append_auth_audit(
      user->user_id,
      "admin_user_audit_viewed",
      {
          {"event_type", event_type.has_value() ? nlohmann::json(*event_type) : nlohmann::json(nullptr)},
          {"limit", limit},
          {"offset", offset},
          {"admin_context", admin_request_context_to_json(context)},
      });
  auto audit = nlohmann::json::array();
  for (const auto& record : filtered) {
    audit.push_back({
        {"event_id", record.event_id},
        {"event_type", record.event_type},
        {"created_at", record.created_at},
        {"payload", record.payload},
    });
  }
  return {
      {"ok", true},
      {"user", user_to_json(*user)},
      {"limit", limit},
      {"offset", offset},
      {"event_type", event_type.has_value() ? nlohmann::json(*event_type) : nlohmann::json(nullptr)},
      {"auth_audit", std::move(audit)},
  };
}

nlohmann::json IdentityService::handle_admin_list_user_devices(
    const std::optional<std::string>& bearer_token,
    const std::string& username,
    const IdentityRequestContext& context) const {
  if (!is_admin_authorized(bearer_token)) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto user = store_.find_user_by_username(username);
  if (!user.has_value()) {
    return {{"ok", false}, {"status", 404}, {"error", "user_not_found"}};
  }
  const auto session_records = store_.list_sessions(user->user_id);
  const auto inventory = build_session_inventory_json(session_records);
  store_.append_auth_audit(
      user->user_id,
      "admin_user_devices_viewed",
      {
          {"admin_context", admin_request_context_to_json(context)},
      });
  return {
      {"ok", true},
      {"user", user_to_json(*user)},
      {"device_count", inventory.at("device_count")},
      {"devices", inventory.at("devices")},
      {"session_inventory", inventory},
  };
}

nlohmann::json IdentityService::handle_admin_revoke_user_session(
    const std::optional<std::string>& bearer_token,
    const std::string& username,
    std::int64_t session_id,
    const IdentityRequestContext& context) const {
  if (!is_admin_authorized(bearer_token)) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto user = store_.find_user_by_username(username);
  if (!user.has_value()) {
    return {{"ok", false}, {"status", 404}, {"error", "user_not_found"}};
  }
  const auto revoked = store_.revoke_session(user->user_id, session_id);
  if (revoked) {
    store_.append_auth_audit(
        user->user_id,
        "admin_user_session_revoked",
        {
            {"session_id", session_id},
            {"admin_context", admin_request_context_to_json(context)},
        });
  }
  return {
      {"ok", revoked},
      {"revoked", revoked},
      {"session_id", session_id},
  };
}

nlohmann::json IdentityService::handle_admin_revoke_other_user_sessions(
    const std::optional<std::string>& bearer_token,
    const std::string& username,
    const IdentityRequestContext& context) const {
  if (!is_admin_authorized(bearer_token)) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto user = store_.find_user_by_username(username);
  if (!user.has_value()) {
    return {{"ok", false}, {"status", 404}, {"error", "user_not_found"}};
  }
  const auto revoked_count = store_.revoke_other_sessions(user->user_id);
  store_.append_auth_audit(
      user->user_id,
      "admin_user_other_sessions_revoked",
      {
          {"revoked_count", revoked_count},
          {"admin_context", admin_request_context_to_json(context)},
      });
  return {
      {"ok", true},
      {"revoked_count", revoked_count},
  };
}

nlohmann::json IdentityService::handle_admin_revoke_user_device_sessions(
    const std::optional<std::string>& bearer_token,
    const std::string& username,
    const nlohmann::json& body,
    const IdentityRequestContext& context) const {
  if (!is_admin_authorized(bearer_token)) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto user = store_.find_user_by_username(username);
  if (!user.has_value()) {
    return {{"ok", false}, {"status", 404}, {"error", "user_not_found"}};
  }
  const auto device_key = optional_trimmed_string(body, "device_key");
  if (!device_key.has_value() || device_key->empty()) {
    return {{"ok", false}, {"status", 400}, {"error", "missing_device_key"}};
  }
  const auto revoked_session_ids = store_.revoke_device_sessions(user->user_id, *device_key);
  store_.append_auth_audit(
      user->user_id,
      "admin_user_device_sessions_revoked",
      {
          {"device_key", *device_key},
          {"revoked_count", revoked_session_ids.size()},
          {"session_ids", revoked_session_ids},
          {"admin_context", admin_request_context_to_json(context)},
      });
  return {
      {"ok", true},
      {"device_key", *device_key},
      {"revoked_count", revoked_session_ids.size()},
      {"session_ids", revoked_session_ids},
  };
}

nlohmann::json IdentityService::handle_admin_edit_user(
    const std::optional<std::string>& bearer_token,
    const std::string& username,
    const nlohmann::json& body,
    const IdentityRequestContext& context) const {
  if (!is_admin_authorized(bearer_token)) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  try {
    const auto user = store_.find_user_by_username(username);
    if (!user.has_value()) {
      return {{"ok", false}, {"status", 404}, {"error", "user_not_found"}};
    }
    const auto updated = store_.admin_update_user(
        user->user_id,
        optional_trimmed_string(body, "email"),
        optional_trimmed_string(body, "display_name"),
        optional_trimmed_string(body, "avatar_url"),
        optional_trimmed_string(body, "bio"),
        optional_trimmed_string(body, "locale"),
        optional_trimmed_string(body, "role"),
        optional_permissions(body, "permissions"),
        optional_bool(body, "email_verified"),
        optional_bool(body, "disabled"));
    if (!updated.has_value()) {
      return {{"ok", false}, {"status", 404}, {"error", "user_not_found"}};
    }
    store_.append_auth_audit(
        updated->user_id,
        "admin_user_updated",
        {
            {"email", optional_trimmed_string(body, "email").has_value()},
            {"display_name", optional_trimmed_string(body, "display_name").has_value()},
            {"avatar_url", optional_trimmed_string(body, "avatar_url").has_value()},
            {"bio", optional_trimmed_string(body, "bio").has_value()},
            {"locale", optional_trimmed_string(body, "locale").has_value()},
            {"role", optional_trimmed_string(body, "role").has_value()},
            {"permissions", body.contains("permissions")},
            {"email_verified", body.contains("email_verified")},
            {"disabled", body.contains("disabled")},
            {"admin_context", admin_request_context_to_json(context)},
        });
    return {{"ok", true}, {"user", user_to_json(*updated)}};
  } catch (const std::exception& exception) {
    if (std::string(exception.what()) == "identity_user_conflict") {
      return {{"ok", false}, {"status", 409}, {"error", "identity_user_conflict"}};
    }
    if (std::string(exception.what()).rfind("invalid_", 0) == 0) {
      return {{"ok", false}, {"status", 400}, {"error", exception.what()}};
    }
    throw;
  }
}

nlohmann::json IdentityService::handle_admin_delete_user(
    const std::optional<std::string>& bearer_token,
    const std::string& username,
    const IdentityRequestContext& context) const {
  if (!is_admin_authorized(bearer_token)) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto user = store_.find_user_by_username(username);
  if (!user.has_value()) {
    return {{"ok", false}, {"status", 404}, {"error", "user_not_found"}};
  }
  const auto updated = store_.admin_update_user(
      user->user_id,
      std::nullopt,
      std::nullopt,
      std::nullopt,
      std::nullopt,
      std::nullopt,
      std::nullopt,
      std::nullopt,
      std::nullopt,
      true);
  store_.revoke_all_sessions(user->user_id);
  store_.append_auth_audit(
      user->user_id,
      "admin_user_disabled",
      {
          {"admin_context", admin_request_context_to_json(context)},
      });
  return {
      {"ok", true},
      {"deleted", true},
      {"soft_deleted", true},
      {"user", updated.has_value() ? user_to_json(*updated) : nlohmann::json::object()},
  };
}

nlohmann::json IdentityService::handle_admin_force_password_reset(
    const std::optional<std::string>& bearer_token,
    const std::string& username,
    const IdentityRequestContext& context) const {
  if (!is_admin_authorized(bearer_token)) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto user = store_.find_user_by_username(username);
  if (!user.has_value()) {
    return {{"ok", false}, {"status", 404}, {"error", "user_not_found"}};
  }
  const auto raw_token = random_token_b64(32);
  const auto expires_at = compute_future_utc(config_.password_reset_ttl_seconds);
  store_.issue_password_reset(user->user_id, sha256_b64(raw_token), expires_at);
  store_.append_auth_audit(
      user->user_id,
      "admin_password_reset_forced",
      {
          {"expires_at", expires_at},
          {"admin_context", admin_request_context_to_json(context)},
      });
  send_password_reset_email(*user, raw_token, expires_at);
  return {
      {"ok", true},
      {"username", user->username},
      {"email", user->email},
      {"expires_at", expires_at},
      {"delivery", "email"},
  };
}

nlohmann::json IdentityService::handle_me(const std::optional<std::string>& bearer_token) const {
  if (!bearer_token.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto session = store_.find_session(*bearer_token);
  if (!session.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto user = store_.find_user_by_identity(session->username);
  return {
      {"ok", true},
      {"session", session_to_json(*session)},
      {"user", user.has_value() ? user_to_json(*user, true) : nlohmann::json(nullptr)},
      {"game_keys", build_game_key_summary(session->user_id)},
  };
}

nlohmann::json IdentityService::handle_sessions(const std::optional<std::string>& bearer_token) const {
  if (!bearer_token.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto session = store_.find_session(*bearer_token);
  if (!session.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  auto sessions = nlohmann::json::array();
  const auto session_records = store_.list_sessions(session->user_id);
  for (const auto& item : session_records) {
    auto json = session_to_json(item);
    json["revoked_at"] = item.revoked_at.has_value() ? nlohmann::json(*item.revoked_at) : nlohmann::json(nullptr);
    json["current"] = item.session_id == session->session_id;
    sessions.push_back(std::move(json));
  }
  return {
      {"ok", true},
      {"sessions", std::move(sessions)},
      {"session_inventory", build_session_inventory_json(session_records, session->session_id)},
  };
}

nlohmann::json IdentityService::handle_revoke_session(
    const std::optional<std::string>& bearer_token,
    const nlohmann::json& body) const {
  if (!bearer_token.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto session = store_.find_session(*bearer_token);
  if (!session.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  if (!body.contains("session_id") || !body.at("session_id").is_number_integer()) {
    return {{"ok", false}, {"status", 400}, {"error", "missing_session_id"}};
  }
  const auto target_session_id = body.at("session_id").get<std::int64_t>();
  if (target_session_id == session->session_id) {
    return {{"ok", false}, {"status", 400}, {"error", "cannot_revoke_current_session"}};
  }
  const auto revoked = store_.revoke_session(session->user_id, target_session_id);
  if (revoked) {
    store_.append_auth_audit(session->user_id, "session_revoked", {{"session_id", target_session_id}});
  }
  return {
      {"ok", revoked},
      {"revoked", revoked},
      {"session_id", target_session_id},
  };
}

nlohmann::json IdentityService::handle_revoke_other_sessions(const std::optional<std::string>& bearer_token) const {
  if (!bearer_token.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto session = store_.find_session(*bearer_token);
  if (!session.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto revoked_count = store_.revoke_other_sessions(session->user_id, session->session_id);
  store_.append_auth_audit(
      session->user_id,
      "other_sessions_revoked",
      {{"current_session_id", session->session_id}, {"revoked_count", revoked_count}});
  return {
      {"ok", true},
      {"message", "Other sessions revoked."},
      {"revoked_count", revoked_count},
  };
}

nlohmann::json IdentityService::handle_update_profile(
    const std::optional<std::string>& bearer_token,
    const nlohmann::json& body) const {
  if (!bearer_token.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto session = store_.find_session(*bearer_token);
  if (!session.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto updated_user = store_.update_profile(
      session->user_id,
      optional_trimmed_string(body, "display_name"),
      optional_trimmed_string(body, "avatar_url"),
      optional_trimmed_string(body, "bio"),
      optional_trimmed_string(body, "locale"));
  if (!updated_user.has_value()) {
    return {{"ok", false}, {"status", 404}, {"error", "profile_not_found"}};
  }
  return {
      {"ok", true},
      {"profile", user_to_json(*updated_user)},
  };
}

nlohmann::json IdentityService::handle_security(const std::optional<std::string>& bearer_token) const {
  if (!bearer_token.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto session = store_.find_session(*bearer_token);
  if (!session.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto state = store_.security_state(session->user_id);
  const auto session_records = store_.list_sessions(session->user_id);
  return {
      {"ok", true},
      {"security",
       {
           {"email_verified", session->email_verified},
           {"role", session->role},
           {"permissions", session->permissions},
           {"sekaiemu_access", store_.user_has_active_entitlement(session->user_id, "sekaiemu")},
           {"two_factor_enabled", state.two_factor_enabled},
           {"recovery_code_count", state.recovery_code_count},
           {"session_inventory", build_session_inventory_json(session_records, session->session_id)},
       }},
  };
}

nlohmann::json IdentityService::handle_auth_audit(const std::optional<std::string>& bearer_token) const {
  if (!bearer_token.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto session = store_.find_session(*bearer_token);
  if (!session.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  auto records = nlohmann::json::array();
  for (const auto& record : store_.list_auth_audit(session->user_id, 50)) {
    records.push_back({
        {"event_id", record.event_id},
        {"event_type", record.event_type},
        {"created_at", record.created_at},
        {"payload", record.payload},
    });
  }
  return {
      {"ok", true},
      {"events", std::move(records)},
  };
}

nlohmann::json IdentityService::handle_two_factor_setup(const std::optional<std::string>& bearer_token) const {
  if (!bearer_token.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto session = store_.find_session(*bearer_token);
  if (!session.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto setup = store_.issue_two_factor_secret(session->user_id);
  store_.append_auth_audit(session->user_id, "two_factor_setup_issued", nlohmann::json::object());
  return {
      {"ok", true},
      {"two_factor",
       {
           {"secret_b32", setup.secret_b32},
           {"otpauth_url",
            "otpauth://totp/SekaiLink:" + session->username + "?secret=" + setup.secret_b32 +
                "&issuer=SekaiLink"},
       }},
  };
}

nlohmann::json IdentityService::handle_two_factor_enable(
    const std::optional<std::string>& bearer_token,
    const nlohmann::json& body) const {
  if (!bearer_token.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto session = store_.find_session(*bearer_token);
  if (!session.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto user = store_.find_user_by_identity(session->username);
  if (!user.has_value() || user->two_factor_secret_b32.empty()) {
    return {{"ok", false}, {"status", 400}, {"error", "two_factor_not_initialized"}};
  }
  const auto code = required_string(body, "code");
  if (!verify_totp_code(user->two_factor_secret_b32, code)) {
    return {{"ok", false}, {"status", 401}, {"error", "invalid_two_factor_code"}};
  }
  store_.enable_two_factor(session->user_id, user->two_factor_secret_b32);
  store_.append_auth_audit(session->user_id, "two_factor_enabled", nlohmann::json::object());
  return {
      {"ok", true},
      {"security",
       {
           {"two_factor_enabled", true},
       }},
  };
}

nlohmann::json IdentityService::handle_two_factor_disable(
    const std::optional<std::string>& bearer_token,
    const nlohmann::json& body) const {
  if (!bearer_token.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto session = store_.find_session(*bearer_token);
  if (!session.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto user = store_.find_user_by_identity(session->username);
  if (!user.has_value() || !user->two_factor_enabled || user->two_factor_secret_b32.empty()) {
    return {{"ok", false}, {"status", 400}, {"error", "two_factor_not_enabled"}};
  }
  const auto code = optional_trimmed_string(body, "code");
  const auto recovery_code = optional_trimmed_string(body, "recovery_code");
  bool authorized = false;
  if (code.has_value()) {
    authorized = verify_totp_code(user->two_factor_secret_b32, *code);
  }
  if (!authorized && recovery_code.has_value()) {
    authorized = store_.consume_recovery_code(session->user_id, *recovery_code);
  }
  if (!authorized) {
    return {{"ok", false}, {"status", 401}, {"error", "invalid_two_factor_code"}};
  }
  store_.disable_two_factor(session->user_id);
  store_.revoke_all_sessions(session->user_id);
  store_.append_auth_audit(session->user_id, "two_factor_disabled", nlohmann::json::object());
  return {
      {"ok", true},
      {"security",
       {
           {"two_factor_enabled", false},
       }},
  };
}

nlohmann::json IdentityService::handle_regenerate_recovery_codes(
    const std::optional<std::string>& bearer_token) const {
  if (!bearer_token.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto session = store_.find_session(*bearer_token);
  if (!session.has_value()) {
    return {{"ok", false}, {"status", 401}, {"error", "unauthorized"}};
  }
  const auto codes = store_.regenerate_recovery_codes(session->user_id, 8);
  store_.append_auth_audit(session->user_id, "recovery_codes_regenerated", {{"count", codes.size()}});
  return {
      {"ok", true},
      {"recovery_codes", codes},
  };
}

nlohmann::json IdentityService::handle_profile(const std::string& username) const {
  const auto user = store_.find_user_by_identity(username);
  if (!user.has_value()) {
    return {{"ok", false}, {"status", 404}, {"error", "profile_not_found"}};
  }
  return {
      {"ok", true},
      {"profile", user_to_json(*user)},
  };
}

void IdentityService::send_password_reset_email(
    const IdentityStore::LoadedUser& user,
    const std::string& raw_token,
    const std::string& expires_at) const {
  const auto separator = config_.password_reset_path.find('?') == std::string::npos ? '?' : '&';
  const auto reset_url = config_.public_base_url + config_.password_reset_path + separator + "token=" + raw_token;
  const auto localized = password_reset_template_for_locale(user.locale);

  std::ostringstream message;
  message << "To: " << user.email << "\n";
  message << "From: " << config_.mail_from << "\n";
  message << "Subject: " << mime_encode_utf8_header(localized.subject) << "\n";
  message << "Content-Language: " << localized.locale << "\n";
  message << "Content-Type: text/plain; charset=UTF-8\n\n";
  message << localized.intro << "\n\n";
  message << "Username: " << user.username << "\n";
  message << localized.action_label << ": " << reset_url << "\n";
  message << localized.expires_label << ": " << expires_at << "\n\n";
  message << localized.footer << "\n";

  const auto command = config_.sendmail_path + " -t";
  FILE* pipe = popen(command.c_str(), "w");
  if (pipe == nullptr) {
    throw std::runtime_error("identity_sendmail_open_failed");
  }
  const auto payload = message.str();
  const auto written = fwrite(payload.data(), 1, payload.size(), pipe);
  const auto rc = pclose(pipe);
  if (written != payload.size() || rc != 0) {
    throw std::runtime_error("identity_sendmail_failed");
  }
}

void IdentityService::send_email_verification_email(
    const IdentityStore::LoadedUser& user,
    const std::string& raw_token,
    const std::string& expires_at) const {
  const auto separator = config_.email_verification_path.find('?') == std::string::npos ? '?' : '&';
  const auto verification_url = config_.public_base_url + config_.email_verification_path + separator + "token=" + raw_token;
  const auto localized = email_verification_template_for_locale(user.locale);

  std::ostringstream message;
  message << "To: " << user.email << "\n";
  message << "From: " << config_.mail_from << "\n";
  message << "Subject: " << mime_encode_utf8_header(localized.subject) << "\n";
  message << "Content-Language: " << localized.locale << "\n";
  message << "Content-Type: text/plain; charset=UTF-8\n\n";
  message << localized.intro << "\n\n";
  message << "Username: " << user.username << "\n";
  message << localized.action_label << ": " << verification_url << "\n";
  message << localized.expires_label << ": " << expires_at << "\n\n";
  message << localized.footer << "\n";

  const auto command = config_.sendmail_path + " -t";
  FILE* pipe = popen(command.c_str(), "w");
  if (pipe == nullptr) {
    throw std::runtime_error("identity_sendmail_open_failed");
  }
  const auto payload = message.str();
  const auto written = fwrite(payload.data(), 1, payload.size(), pipe);
  const auto rc = pclose(pipe);
  if (written != payload.size() || rc != 0) {
    throw std::runtime_error("identity_sendmail_failed");
  }
}

void IdentityService::record_request() const {
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    ++total_requests_;
  }
  write_state_file();
}

void IdentityService::record_error() const {
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    ++total_errors_;
  }
  write_state_file();
}

void IdentityService::write_state_file() const {
  if (config_.state_path.empty()) {
    return;
  }
  std::lock_guard<std::mutex> lock(state_mutex_);
  std::filesystem::create_directories(config_.state_path.parent_path());
  nlohmann::json state = {
      {"ok", true},
      {"service", "sekailink_identity_service"},
      {"listen_host", config_.listen_host},
      {"http_port", config_.http_port},
      {"sqlite_path", config_.sqlite_path.string()},
      {"password_iterations", config_.password_iterations},
      {"password_time_cost", config_.password_time_cost},
      {"password_memory_kib", config_.password_memory_kib},
      {"password_parallelism", config_.password_parallelism},
      {"password_hash_length", config_.password_hash_length},
      {"password_salt_length", config_.password_salt_length},
      {"session_ttl_seconds", config_.session_ttl_seconds},
      {"password_reset_ttl_seconds", config_.password_reset_ttl_seconds},
      {"email_verification_ttl_seconds", config_.email_verification_ttl_seconds},
      {"email_verification_path", config_.email_verification_path},
      {"sendmail_path", config_.sendmail_path},
      {"state_path", config_.state_path.string()},
      {"total_requests", total_requests_},
      {"total_errors", total_errors_},
      {"updated_at", utc_timestamp_now()},
  };
  std::ofstream stream(config_.state_path);
  stream << state.dump(2) << "\n";
}

nlohmann::json IdentityService::session_to_json(const IdentityStore::SessionRecord& session) {
  const auto session_state = session_state_for_record(session);
  return {
      {"session_id", session.session_id},
      {"session_token", session.session_token},
      {"user_id", session.user_id},
      {"username", session.username},
      {"email", session.email},
      {"email_verified", session.email_verified},
      {"display_name", session.display_name},
      {"avatar_url", session.avatar_url},
      {"bio", session.bio},
      {"locale", session.locale},
      {"role", session.role},
      {"permissions", session.permissions},
      {"patreon",
       {
           {"tier", session.patreon_tier.has_value() ? nlohmann::json(*session.patreon_tier) : nlohmann::json(nullptr)},
           {"is_supporter", session.patreon_is_supporter},
       }},
      {"created_at", session.created_at},
      {"expires_at", session.expires_at},
      {"revoked_at", session.revoked_at.has_value() ? nlohmann::json(*session.revoked_at) : nlohmann::json(nullptr)},
      {"session_state", session_state},
      {"client_summary", session_device_name(session)},
      {"device",
       {
           {"device_key", session_device_key(session)},
           {"display_name", session_device_name(session)},
       }},
      {"client",
       {
           {"user_agent", session.user_agent.has_value() ? nlohmann::json(*session.user_agent) : nlohmann::json(nullptr)},
           {"client_name", session.client_name.has_value() ? nlohmann::json(*session.client_name) : nlohmann::json(nullptr)},
           {"client_version", session.client_version.has_value() ? nlohmann::json(*session.client_version) : nlohmann::json(nullptr)},
           {"device_id", session.device_id.has_value() ? nlohmann::json(*session.device_id) : nlohmann::json(nullptr)},
           {"requested_locale", session.requested_locale.has_value() ? nlohmann::json(*session.requested_locale) : nlohmann::json(nullptr)},
       }},
  };
}

nlohmann::json IdentityService::game_key_to_json(const IdentityStore::GameKeyRecord& key, bool include_binding) {
  nlohmann::json json = {
      {"key_id", key.key_id},
      {"key_code", key.key_code},
      {"status", key.status},
      {"entitlements", key.entitlements},
      {"created_at", key.created_at},
      {"activated_at", key.activated_at.has_value() ? nlohmann::json(*key.activated_at) : nlohmann::json(nullptr)},
      {"deactivated_at", key.deactivated_at.has_value() ? nlohmann::json(*key.deactivated_at) : nlohmann::json(nullptr)},
      {"notes", key.notes},
      {"linked", key.bound_user_id.has_value()},
      {"is_active", key.status == "activated"},
      {"is_available", key.status == "free"},
      {"is_deactivated", key.status == "deactivated"},
  };
  if (include_binding) {
    json["bound_user_id"] = key.bound_user_id.has_value() ? nlohmann::json(*key.bound_user_id) : nlohmann::json(nullptr);
    json["bound_username"] = key.bound_username.has_value() ? nlohmann::json(*key.bound_username) : nlohmann::json(nullptr);
  }
  return json;
}

nlohmann::json IdentityService::build_game_key_summary(std::int64_t user_id) const {
  const auto keys = store_.list_user_game_keys(user_id);
  std::size_t activated = 0;
  std::size_t free = 0;
  std::size_t deactivated = 0;
  for (const auto& key : keys) {
    if (key.status == "activated") {
      ++activated;
    } else if (key.status == "deactivated") {
      ++deactivated;
    } else {
      ++free;
    }
  }
  const auto entitlements = store_.list_user_active_entitlements(user_id);
  return {
      {"total", keys.size()},
      {"activated", activated},
      {"free", free},
      {"deactivated", deactivated},
      {"entitlements", entitlements},
      {"sekaiemu_access", std::find(entitlements.begin(), entitlements.end(), "sekaiemu") != entitlements.end()},
  };
}

nlohmann::json IdentityService::patreon_to_json(const IdentityStore::LoadedUser& user, bool include_private) {
  nlohmann::json patreon = {
      {"linked", user.patreon_id.has_value() || user.patreon_member_id.has_value()},
      {"tier", user.patreon_tier.has_value() ? nlohmann::json(*user.patreon_tier) : nlohmann::json(nullptr)},
      {"status", user.patreon_status.has_value() ? nlohmann::json(*user.patreon_status) : nlohmann::json(nullptr)},
      {"is_supporter", user.patreon_is_supporter},
      {"linked_at", user.patreon_linked_at.has_value() ? nlohmann::json(*user.patreon_linked_at) : nlohmann::json(nullptr)},
  };
  if (include_private) {
    patreon["patreon_id"] = user.patreon_id.has_value() ? nlohmann::json(*user.patreon_id) : nlohmann::json(nullptr);
    patreon["patreon_email"] = user.patreon_email.has_value() ? nlohmann::json(*user.patreon_email) : nlohmann::json(nullptr);
    patreon["patreon_member_id"] = user.patreon_member_id.has_value() ? nlohmann::json(*user.patreon_member_id) : nlohmann::json(nullptr);
  }
  return patreon;
}

nlohmann::json IdentityService::user_to_json(const IdentityStore::LoadedUser& user, bool include_private_links) {
  return {
      {"user_id", user.user_id},
      {"username", user.username},
      {"email", user.email},
      {"email_verified", user.email_verified},
      {"display_name", user.display_name},
      {"avatar_url", user.avatar_url},
      {"bio", user.bio},
      {"locale", user.locale},
      {"role", user.role},
      {"permissions", user.permissions},
      {"disabled_at", user.disabled_at.has_value() ? nlohmann::json(*user.disabled_at) : nlohmann::json(nullptr)},
      {"patreon", patreon_to_json(user, include_private_links)},
  };
}

IdentityHttpServer::IdentityHttpServer(IdentityServiceConfig config)
    : service_(config), config_(std::move(config)) {}

IdentityHttpServer::~IdentityHttpServer() {
  stop();
}

bool IdentityHttpServer::start() {
#ifdef _WIN32
  throw std::runtime_error("identity_http_not_supported_on_windows_yet");
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

void IdentityHttpServer::stop() {
#ifndef _WIN32
  if (listen_fd_ >= 0) {
    ::shutdown(listen_fd_, SHUT_RDWR);
    ::close(listen_fd_);
    listen_fd_ = -1;
    bound_port_ = 0;
  }
#endif
}

std::uint16_t IdentityHttpServer::port() const {
  return bound_port_;
}

void IdentityHttpServer::serve_one() const {
#ifdef _WIN32
  throw std::runtime_error("identity_http_not_supported_on_windows_yet");
#else
  fd_set read_fds;
  FD_ZERO(&read_fds);
  FD_SET(listen_fd_, &read_fds);
  timeval timeout{};
  timeout.tv_sec = 1;
  timeout.tv_usec = 0;
  const auto ready = ::select(listen_fd_ + 1, &read_fds, nullptr, nullptr, &timeout);
  if (ready <= 0) {
    return;
  }

  sockaddr_in client_address{};
  socklen_t client_length = sizeof(client_address);
  const int client_fd = ::accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_address), &client_length);
  if (client_fd < 0) {
    throw std::runtime_error("identity_accept_failed");
  }
  timeval recv_timeout{};
  recv_timeout.tv_sec = 15;
  recv_timeout.tv_usec = 0;
  ::setsockopt(client_fd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));

  std::string request;
  char buffer[4096];
  while (request.find("\r\n\r\n") == std::string::npos) {
    const auto received = ::recv(client_fd, buffer, sizeof(buffer), 0);
    if (received <= 0) {
      ::close(client_fd);
      throw std::runtime_error("identity_recv_failed");
    }
    request.append(buffer, static_cast<std::size_t>(received));
  }

  const auto headers_end = request.find("\r\n\r\n");
  const auto body_start = headers_end + 4;
  std::istringstream request_stream(request.substr(0, headers_end));
  std::string method;
  std::string path;
  std::string version;
  request_stream >> method >> path >> version;

  std::optional<std::string> bearer_token;
  IdentityRequestContext context;
  std::size_t content_length = 0;
  bool expect_continue = false;
  std::string header_line;
  std::getline(request_stream, header_line);
  while (std::getline(request_stream, header_line)) {
    if (!header_line.empty() && header_line.back() == '\r') {
      header_line.pop_back();
    }
    if (header_line.rfind("Authorization: Bearer ", 0) == 0) {
      auto token = header_line.substr(std::string("Authorization: Bearer ").size());
      bearer_token = std::move(token);
      continue;
    }
    const auto separator = header_line.find(':');
    if (separator == std::string::npos) {
      continue;
    }
    const auto header_name = lower(trim(header_line.substr(0, separator)));
    const auto header_value = trim(header_line.substr(separator + 1));
    if (header_name == "content-length") {
      if (!header_value.empty()) {
        content_length = static_cast<std::size_t>(std::stoul(header_value));
      }
    } else if (header_name == "expect") {
      expect_continue = lower(header_value) == "100-continue";
    } else if (header_name == "user-agent") {
      context.user_agent = header_value;
    } else if (header_name == "x-sekailink-client") {
      context.client_name = header_value;
    } else if (header_name == "x-sekailink-client-version") {
      context.client_version = header_value;
    } else if (header_name == "x-sekailink-device-id") {
      context.device_id = header_value;
    } else if (header_name == "x-sekailink-locale") {
      context.locale = header_value;
    }
  }

  if (expect_continue && content_length > 0 && request.size() < body_start + content_length) {
    static constexpr char continue_response[] = "HTTP/1.1 100 Continue\r\n\r\n";
    ::send(client_fd, continue_response, sizeof(continue_response) - 1, 0);
  }

  while (request.size() < body_start + content_length) {
    const auto received = ::recv(client_fd, buffer, sizeof(buffer), 0);
    if (received <= 0) {
      break;
    }
    request.append(buffer, static_cast<std::size_t>(received));
  }

  std::optional<nlohmann::json> body;
  if (content_length > 0 && request.size() >= body_start + content_length) {
    body = nlohmann::json::parse(request.substr(body_start, content_length));
  }

  int status_code = 200;
  nlohmann::json response_body;
  try {
    response_body = service_.handle(method, path, bearer_token, body, context);
    if (response_body.contains("status") && response_body.at("status").is_number_integer()) {
      status_code = response_body.at("status").get<int>();
      response_body.erase("status");
    }
  } catch (const std::exception& exception) {
    status_code = 500;
    response_body = {
        {"ok", false},
        {"error", "internal_error"},
        {"message", exception.what()},
    };
  }

  const auto response = json_http_response(status_code, response_body);
  ::send(client_fd, response.data(), response.size(), 0);
  ::close(client_fd);
#endif
}

}  // namespace sekailink_server
