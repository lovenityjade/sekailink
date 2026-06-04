#ifndef _WIN32
#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <regex>
#include <stdexcept>
#include <thread>

#include "nlohmann/json.hpp"

#include <openssl/evp.h>
#include <openssl/hmac.h>

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::vector<unsigned char> base32_decode(const std::string& value) {
  auto decode_char = [](char c) -> int {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= '2' && c <= '7') return c - '2' + 26;
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
      throw std::runtime_error("identity_smoke_base32_decode_failed");
    }
    buffer = (buffer << 5) | (decoded & 0x1f);
    bits_left += 5;
    if (bits_left >= 8) {
      output.push_back(static_cast<unsigned char>((buffer >> (bits_left - 8)) & 0xff));
      bits_left -= 8;
    }
  }
  return output;
}

std::string totp_code(const std::string& secret_b32) {
  const auto key = base32_decode(secret_b32);
  auto counter = static_cast<std::uint64_t>(std::chrono::system_clock::to_time_t(std::chrono::system_clock::now()) / 30);
  unsigned char message[8];
  for (int i = 7; i >= 0; --i) {
    message[i] = static_cast<unsigned char>(counter & 0xff);
    counter >>= 8;
  }
  unsigned char digest[EVP_MAX_MD_SIZE];
  unsigned int digest_len = 0;
  if (HMAC(EVP_sha1(), key.data(), static_cast<int>(key.size()), message, sizeof(message), digest, &digest_len) == nullptr) {
    throw std::runtime_error("identity_smoke_hotp_failed");
  }
  const auto offset = digest[digest_len - 1] & 0x0f;
  const std::uint32_t binary = ((digest[offset] & 0x7f) << 24) | ((digest[offset + 1] & 0xff) << 16) |
                               ((digest[offset + 2] & 0xff) << 8) | (digest[offset + 3] & 0xff);
  std::ostringstream out;
  out << std::setw(6) << std::setfill('0') << (binary % 1000000);
  return out.str();
}

#ifndef _WIN32
std::string http_request(
    std::uint16_t port,
    const std::string& method,
    const std::string& path,
    const std::string& bearer_token,
    const std::string& body,
    const std::vector<std::pair<std::string, std::string>>& headers = {}) {
  const int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    throw std::runtime_error("identity_smoke_socket_failed");
  }
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(sock);
    throw std::runtime_error("identity_smoke_connect_failed");
  }
  std::string request = method + " " + path + " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n";
  if (!bearer_token.empty()) {
    request += "Authorization: Bearer " + bearer_token + "\r\n";
  }
  for (const auto& [name, value] : headers) {
    request += name + ": " + value + "\r\n";
  }
  if (!body.empty()) {
    request += "Content-Type: application/json\r\n";
    request += "Content-Length: " + std::to_string(body.size()) + "\r\n";
  }
  request += "\r\n";
  request += body;
  if (::send(sock, request.data(), request.size(), 0) < 0) {
    ::close(sock);
    throw std::runtime_error("identity_smoke_send_failed");
  }
  std::string response;
  char buffer[4096];
  while (true) {
    const auto received = ::recv(sock, buffer, sizeof(buffer), 0);
    if (received <= 0) {
      break;
    }
    response.append(buffer, static_cast<std::size_t>(received));
  }
  ::close(sock);
  return response;
}

std::string http_expect_continue_request(std::uint16_t port, const std::string& path, const std::string& body) {
  const int sock = ::socket(AF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    throw std::runtime_error("identity_smoke_socket_failed");
  }
  timeval recv_timeout{};
  recv_timeout.tv_sec = 5;
  recv_timeout.tv_usec = 0;
  ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout));
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  if (::connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
    ::close(sock);
    throw std::runtime_error("identity_smoke_connect_failed");
  }
  const std::string request_headers = "POST " + path +
                                      " HTTP/1.1\r\nHost: 127.0.0.1\r\nConnection: close\r\n"
                                      "Content-Type: application/json\r\nExpect: 100-continue\r\n"
                                      "Content-Length: " +
                                      std::to_string(body.size()) + "\r\n\r\n";
  if (::send(sock, request_headers.data(), request_headers.size(), 0) < 0) {
    ::close(sock);
    throw std::runtime_error("identity_smoke_send_failed");
  }
  std::string response;
  char buffer[4096];
  while (response.find("\r\n\r\n") == std::string::npos) {
    const auto received = ::recv(sock, buffer, sizeof(buffer), 0);
    if (received <= 0) {
      ::close(sock);
      throw std::runtime_error("identity_smoke_continue_missing");
    }
    response.append(buffer, static_cast<std::size_t>(received));
  }
  require(response.find("100 Continue") != std::string::npos, "identity_expect_continue_missing");
  if (::send(sock, body.data(), body.size(), 0) < 0) {
    ::close(sock);
    throw std::runtime_error("identity_smoke_body_send_failed");
  }
  while (true) {
    const auto received = ::recv(sock, buffer, sizeof(buffer), 0);
    if (received <= 0) {
      break;
    }
    response.append(buffer, static_cast<std::size_t>(received));
  }
  ::close(sock);
  return response;
}

std::string extract_body(const std::string& response) {
  const auto pos = response.find("\r\n\r\n");
  if (pos == std::string::npos) {
    throw std::runtime_error("identity_smoke_body_missing");
  }
  return response.substr(pos + 4);
}
#endif

}  // namespace

int main(int argc, char** argv) {
#ifdef _WIN32
  std::cerr << "identity_service_smoke failed: not supported on Windows yet\n";
  return 1;
#else
  try {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "sekailink_identity_service_smoke";
    fs::remove_all(root);
    fs::create_directories(root);
    const std::uint16_t port = static_cast<std::uint16_t>(19195 + (static_cast<unsigned int>(::getpid()) % 1000));

    const fs::path config_path = root / "identity_service.json";
    const fs::path sqlite_path = root / "identity.sqlite3";
    const fs::path state_path = root / "identity_state.json";
    const fs::path mail_capture_path = root / "mail.txt";
    const fs::path sendmail_script_path = root / "sendmail.sh";
    {
      std::ofstream sendmail_stream(sendmail_script_path);
      sendmail_stream << "#!/usr/bin/env bash\n";
      sendmail_stream << "cat > \"" << mail_capture_path.string() << "\"\n";
      sendmail_stream << "exit 0\n";
    }
    fs::permissions(
        sendmail_script_path,
        fs::perms::owner_exec | fs::perms::owner_read | fs::perms::owner_write,
        fs::perm_options::replace);
    {
      std::ofstream config_stream(config_path);
      config_stream << "{\n"
                    << "  \"http_port\": " << port << ",\n"
                    << "  \"sqlite_path\": \"" << sqlite_path.string() << "\",\n"
                    << "  \"password_iterations\": 10000,\n"
                    << "  \"password_time_cost\": 2,\n"
                    << "  \"password_memory_kib\": 8192,\n"
                    << "  \"password_parallelism\": 1,\n"
                    << "  \"password_hash_length\": 32,\n"
                    << "  \"password_salt_length\": 16,\n"
                    << "  \"session_ttl_seconds\": 3600,\n"
                    << "  \"password_reset_ttl_seconds\": 1800,\n"
                    << "  \"email_verification_ttl_seconds\": 3600,\n"
                    << "  \"public_base_url\": \"https://sekailink.com\",\n"
                    << "  \"password_reset_path\": \"/reset-password\",\n"
                    << "  \"email_verification_path\": \"/verify-email\",\n"
                    << "  \"mail_from\": \"noreply@sekailink.com\",\n"
                    << "  \"sendmail_path\": \"" << sendmail_script_path.string() << "\",\n"
                    << "  \"admin_token\": \"identity-admin-secret\",\n"
                    << "  \"state_path\": \"" << state_path.string() << "\"\n"
                    << "}\n";
    }

    const fs::path binary =
        argc > 0 ? fs::path(argv[0]).parent_path() / "sekailink_identity_service"
                 : fs::path("/tmp/sekailink-server-build/sekailink_identity_service");
    const pid_t pid = ::fork();
    if (pid < 0) {
      throw std::runtime_error("identity_smoke_fork_failed");
    }
    if (pid == 0) {
      ::execl(binary.c_str(), binary.c_str(), "--config", config_path.c_str(), static_cast<char*>(nullptr));
      _exit(127);
    }

    bool ready = false;
    for (int i = 0; i < 50; ++i) {
      try {
        const auto health = http_request(port, "GET", "/health", "", "");
        if (health.find("200 OK") != std::string::npos) {
          ready = true;
          break;
        }
      } catch (...) {
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!ready) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("identity_smoke_not_ready");
    }

    const auto expect_login_response = http_expect_continue_request(
        port,
        "/login",
        R"({"identity":"missing@example.com","password":"sekailink-password"})");
    require(expect_login_response.find("100 Continue") != std::string::npos, "identity_expect_continue_status");
    require(expect_login_response.find("401 Unauthorized") != std::string::npos, "identity_expect_continue_final_status");

    const auto register_response = http_request(
        port,
        "POST",
        "/register",
        "",
        R"({"username":"jade","email":"jade@example.com","password":"sekailink-password","display_name":"Jade","locale":"fr"})",
        {{"User-Agent", "SekaiLinkSmoke/1.0"},
         {"X-SekaiLink-Client", "sekaiemu"},
         {"X-SekaiLink-Client-Version", "0.9.0-smoke"},
         {"X-SekaiLink-Device-Id", "device-smoke-1"},
         {"X-SekaiLink-Locale", "fr-CA"}});
    require(register_response.find("201 Created") != std::string::npos, "identity_register_status");
    const auto register_json = nlohmann::json::parse(extract_body(register_response));
    require(register_json.at("ok").get<bool>() == true, "identity_register_ok");
    const auto session_token = register_json.at("session").at("session_token").get<std::string>();
    require(!session_token.empty(), "identity_session_token");
    require(register_json.at("user").at("email_verified").get<bool>() == false, "identity_register_email_unverified");
    require(register_json.at("session").at("email_verified").get<bool>() == false, "identity_register_session_email_unverified");
    require(register_json.at("session").at("client").at("client_name").get<std::string>() == "sekaiemu", "identity_register_client_name");
    require(register_json.at("session").at("client").at("device_id").get<std::string>() == "device-smoke-1", "identity_register_device_id");
    require(fs::exists(mail_capture_path), "identity_verification_mail_missing");
    std::ifstream verification_mail_stream(mail_capture_path);
    const std::string verification_mail_body((std::istreambuf_iterator<char>(verification_mail_stream)), std::istreambuf_iterator<char>());
    require(verification_mail_body.find("Content-Language: fr") != std::string::npos, "identity_verification_mail_locale");
    require(verification_mail_body.find("Bienvenue sur SekaiLink.") != std::string::npos, "identity_verification_mail_body_fr");
    const std::regex verification_token_regex(R"(token=([A-Za-z0-9+/=]+))");
    std::smatch verification_token_match;
    require(std::regex_search(verification_mail_body, verification_token_match, verification_token_regex), "identity_verification_token_missing");
    const auto verification_token = verification_token_match[1].str();
    require(!verification_token.empty(), "identity_verification_token_empty");

    const auto verification_complete_response = http_request(
        port,
        "POST",
        "/email-verification/complete",
        "",
        std::string("{\"token\":\"") + verification_token + "\"}");
    require(verification_complete_response.find("200 OK") != std::string::npos, "identity_verification_complete_status");

    const auto me_response = http_request(port, "GET", "/me", session_token, "");
    require(me_response.find("200 OK") != std::string::npos, "identity_me_status");
    const auto me_json = nlohmann::json::parse(extract_body(me_response));
    require(me_json.at("session").at("username").get<std::string>() == "jade", "identity_me_username");
    require(me_json.at("session").at("email_verified").get<bool>() == true, "identity_me_email_verified");
    require(me_json.at("session").at("client").at("client_version").get<std::string>() == "0.9.0-smoke", "identity_me_client_version");
    require(me_json.at("game_keys").at("total").get<int>() == 0, "identity_me_game_keys_empty");

    const auto security_response = http_request(port, "GET", "/me/security", session_token, "");
    require(security_response.find("200 OK") != std::string::npos, "identity_security_status");
    const auto security_json = nlohmann::json::parse(extract_body(security_response));
    require(security_json.at("security").at("email_verified").get<bool>() == true, "identity_security_email_verified");
    require(security_json.at("security").at("sekaiemu_access").get<bool>() == false, "identity_security_no_sekaiemu_access");
    require(security_json.at("security").at("recovery_code_count").get<int>() == 0, "identity_security_initial_codes");
    require(
        security_json.at("security").at("session_inventory").at("total_sessions").get<int>() == 1,
        "identity_security_initial_session_inventory");

    const auto verification_request_after_response =
        http_request(port, "POST", "/me/email-verification/request", session_token, "");
    require(verification_request_after_response.find("200 OK") != std::string::npos, "identity_verification_request_after_status");
    const auto verification_request_after_json = nlohmann::json::parse(extract_body(verification_request_after_response));
    require(verification_request_after_json.at("already_verified").get<bool>() == true, "identity_verification_request_after_already_verified");

    const auto patch_response = http_request(
        port,
        "PATCH",
        "/me/profile",
        session_token,
        R"({"display_name":"Jade Runner","bio":"Testing profile updates","avatar_url":"https://cdn.sekailink.com/avatar/jade.png","locale":"fr-CA"})");
    require(patch_response.find("200 OK") != std::string::npos, "identity_patch_status");
    const auto patch_json = nlohmann::json::parse(extract_body(patch_response));
    require(patch_json.at("profile").at("display_name").get<std::string>() == "Jade Runner", "identity_patch_display_name");
    require(patch_json.at("profile").at("locale").get<std::string>() == "fr-CA", "identity_patch_locale");

    const auto recovery_response =
        http_request(port, "POST", "/me/security/recovery-codes/regenerate", session_token, "");
    require(recovery_response.find("200 OK") != std::string::npos, "identity_recovery_status");
    const auto recovery_json = nlohmann::json::parse(extract_body(recovery_response));
    require(recovery_json.at("recovery_codes").size() == 8, "identity_recovery_count");
    const auto recovery_code = recovery_json.at("recovery_codes").at(0).get<std::string>();

    const auto security_after_response = http_request(port, "GET", "/me/security", session_token, "");
    require(security_after_response.find("200 OK") != std::string::npos, "identity_security_after_status");
    const auto security_after_json = nlohmann::json::parse(extract_body(security_after_response));
    require(security_after_json.at("security").at("recovery_code_count").get<int>() == 8, "identity_security_after_codes");

    const auto two_factor_setup_response =
        http_request(port, "POST", "/me/security/2fa/setup", session_token, "");
    require(two_factor_setup_response.find("200 OK") != std::string::npos, "identity_two_factor_setup_status");
    const auto two_factor_setup_json = nlohmann::json::parse(extract_body(two_factor_setup_response));
    const auto two_factor_secret = two_factor_setup_json.at("two_factor").at("secret_b32").get<std::string>();
    require(!two_factor_secret.empty(), "identity_two_factor_secret");
    const auto two_factor_code = totp_code(two_factor_secret);

    const auto two_factor_enable_response = http_request(
        port,
        "POST",
        "/me/security/2fa/enable",
        session_token,
        std::string("{\"code\":\"") + two_factor_code + "\"}");
    require(two_factor_enable_response.find("200 OK") != std::string::npos, "identity_two_factor_enable_status");

    const auto security_totp_response = http_request(port, "GET", "/me/security", session_token, "");
    require(security_totp_response.find("200 OK") != std::string::npos, "identity_security_totp_status");
    const auto security_totp_json = nlohmann::json::parse(extract_body(security_totp_response));
    require(security_totp_json.at("security").at("two_factor_enabled").get<bool>() == true, "identity_two_factor_enabled");

    const auto login_response = http_request(
        port,
        "POST",
        "/login",
        "",
        R"({"identity":"jade@example.com","password":"sekailink-password"})");
    require(login_response.find("401 Unauthorized") != std::string::npos, "identity_login_requires_two_factor");

    const auto login_two_factor_response = http_request(
        port,
        "POST",
        "/login",
        "",
        std::string("{\"identity\":\"jade@example.com\",\"password\":\"sekailink-password\",\"two_factor_code\":\"") + totp_code(two_factor_secret) + "\"}",
        {{"User-Agent", "SekaiLinkConnect/2.0"},
         {"X-SekaiLink-Client", "connect-android"},
         {"X-SekaiLink-Client-Version", "2.0.1"},
         {"X-SekaiLink-Device-Id", "pixel-9-pro"}});
    require(login_two_factor_response.find("200 OK") != std::string::npos, "identity_login_two_factor_status");
    const auto login_json = nlohmann::json::parse(extract_body(login_two_factor_response));
    require(login_json.at("ok").get<bool>() == true, "identity_login_two_factor_ok");
    const auto secondary_session_token = login_json.at("session").at("session_token").get<std::string>();
    const auto secondary_session_id = login_json.at("session").at("session_id").get<std::int64_t>();
    require(login_json.at("session").at("client").at("client_name").get<std::string>() == "connect-android", "identity_login_client_name");
    require(login_json.at("game_keys").at("sekaiemu_access").get<bool>() == false, "identity_login_no_game_key_access");

    const auto generate_key_response = http_request(
        port,
        "POST",
        "/admin/game-keys/generate",
        "identity-admin-secret",
        R"({"count":1,"entitlements":["sekaiemu"],"notes":"beta access"})");
    require(generate_key_response.find("200 OK") != std::string::npos, "identity_game_key_generate_status");
    const auto generate_key_json = nlohmann::json::parse(extract_body(generate_key_response));
    require(generate_key_json.at("count").get<int>() == 1, "identity_game_key_generate_count");
    const auto game_key_code = generate_key_json.at("keys").at(0).at("key_code").get<std::string>();
    require(!game_key_code.empty(), "identity_game_key_generate_code");

    const auto lookup_response = http_request(
        port,
        "POST",
        "/game-keys/lookup",
        "",
        std::string("{\"game_key\":\"") + game_key_code + "\"}");
    if (lookup_response.find("200 OK") == std::string::npos) {
      throw std::runtime_error(std::string("identity_game_key_lookup_status: ") + lookup_response);
    }
    const auto lookup_json = nlohmann::json::parse(extract_body(lookup_response));
    require(lookup_json.at("exists").get<bool>() == true, "identity_game_key_lookup_exists");
    require(lookup_json.at("status").get<std::string>() == "free", "identity_game_key_lookup_free");

    const auto activate_response = http_request(
        port,
        "POST",
        "/me/game-keys/activate",
        session_token,
        std::string("{\"game_key\":\"") + game_key_code + "\"}");
    require(activate_response.find("200 OK") != std::string::npos, "identity_game_key_activate_status");
    const auto activate_json = nlohmann::json::parse(extract_body(activate_response));
    require(activate_json.at("activated").get<bool>() == true, "identity_game_key_activate_ok");
    require(activate_json.at("summary").at("sekaiemu_access").get<bool>() == true, "identity_game_key_activate_access");

    const auto game_keys_response = http_request(port, "GET", "/me/game-keys", session_token, "");
    require(game_keys_response.find("200 OK") != std::string::npos, "identity_game_key_list_status");
    const auto game_keys_json = nlohmann::json::parse(extract_body(game_keys_response));
    require(game_keys_json.at("summary").at("activated").get<int>() == 1, "identity_game_key_list_activated_count");

    const auto game_key_check_response = http_request(
        port,
        "POST",
        "/me/game-keys/check",
        session_token,
        std::string("{\"game_key\":\"") + game_key_code + "\"}");
    require(game_key_check_response.find("200 OK") != std::string::npos, "identity_game_key_check_status");
    const auto game_key_check_json = nlohmann::json::parse(extract_body(game_key_check_response));
    require(game_key_check_json.at("linked_to_current_account").get<bool>() == true, "identity_game_key_check_self");
    require(game_key_check_json.at("is_usable").get<bool>() == true, "identity_game_key_check_usable");

    const auto login_recovery_response = http_request(
        port,
        "POST",
        "/login",
        "",
        std::string("{\"identity\":\"jade@example.com\",\"password\":\"sekailink-password\",\"recovery_code\":\"") + recovery_code + "\"}");
    require(login_recovery_response.find("200 OK") != std::string::npos, "identity_login_recovery_status");
    const auto recovery_login_json = nlohmann::json::parse(extract_body(login_recovery_response));
    const auto third_session_token = recovery_login_json.at("session").at("session_token").get<std::string>();

    const auto sessions_response = http_request(port, "GET", "/me/sessions", session_token, "");
    require(sessions_response.find("200 OK") != std::string::npos, "identity_sessions_status");
    const auto sessions_json = nlohmann::json::parse(extract_body(sessions_response));
    require(sessions_json.at("sessions").size() >= 3, "identity_sessions_count");
    require(
        sessions_json.at("session_inventory").at("active_sessions").get<int>() >= 3,
        "identity_sessions_inventory_active_count");
    bool found_device = false;
    bool found_client_summary = false;
    for (const auto& item : sessions_json.at("sessions")) {
      if (item.at("client").at("device_id").is_string() &&
          item.at("client").at("device_id").get<std::string>() == "pixel-9-pro") {
        found_device = true;
      }
      if (item.at("client_summary").is_string() &&
          item.at("client_summary").get<std::string>().find("pixel-9-pro") != std::string::npos) {
        found_client_summary = true;
      }
    }
    require(found_device, "identity_sessions_device_roundtrip");
    require(found_client_summary, "identity_sessions_client_summary");

    const auto revoke_session_response = http_request(
        port,
        "POST",
        "/me/sessions/revoke",
        session_token,
        std::string("{\"session_id\":") + std::to_string(secondary_session_id) + "}");
    require(revoke_session_response.find("200 OK") != std::string::npos, "identity_revoke_session_status");
    const auto secondary_me_response = http_request(port, "GET", "/me", secondary_session_token, "");
    require(secondary_me_response.find("401 Unauthorized") != std::string::npos, "identity_revoked_session_rejected");

    const auto revoke_others_response = http_request(port, "POST", "/me/sessions/revoke-others", session_token, "");
    require(revoke_others_response.find("200 OK") != std::string::npos, "identity_revoke_others_status");
    const auto third_me_response = http_request(port, "GET", "/me", third_session_token, "");
    require(third_me_response.find("401 Unauthorized") != std::string::npos, "identity_revoke_others_effect");

    const auto audit_response = http_request(port, "GET", "/me/security/audit", session_token, "");
    require(audit_response.find("200 OK") != std::string::npos, "identity_audit_status");
    const auto audit_json = nlohmann::json::parse(extract_body(audit_response));
    require(audit_json.at("events").size() >= 5, "identity_audit_count");
    bool found_context = false;
    for (const auto& event : audit_json.at("events")) {
      if (event.at("payload").contains("request_context") &&
          event.at("payload").at("request_context").contains("client_name")) {
        found_context = true;
      }
    }
    require(found_context, "identity_audit_request_context");

    const auto two_factor_disable_response = http_request(
        port,
        "POST",
        "/me/security/2fa/disable",
        session_token,
        std::string("{\"code\":\"") + totp_code(two_factor_secret) + "\"}");
    require(two_factor_disable_response.find("200 OK") != std::string::npos, "identity_two_factor_disable_status");

    const auto password_reset_request_response = http_request(
        port,
        "POST",
        "/password-recovery/request",
        "",
        R"({"identity":"jade@example.com"})");
    require(password_reset_request_response.find("200 OK") != std::string::npos, "identity_password_reset_request_status");
    require(fs::exists(mail_capture_path), "identity_password_reset_mail_missing");
    std::ifstream mail_stream(mail_capture_path);
    const std::string mail_body((std::istreambuf_iterator<char>(mail_stream)), std::istreambuf_iterator<char>());
    require(mail_body.find("Content-Language: fr") != std::string::npos, "identity_password_reset_mail_locale");
    require(mail_body.find("Une reinitialisation du mot de passe") != std::string::npos, "identity_password_reset_mail_body_fr");
    const std::regex token_regex(R"(token=([A-Za-z0-9+/=]+))");
    std::smatch token_match;
    require(std::regex_search(mail_body, token_match, token_regex), "identity_password_reset_token_missing");
    const auto reset_token = token_match[1].str();
    require(!reset_token.empty(), "identity_password_reset_token_empty");

    const auto password_reset_complete_response = http_request(
        port,
        "POST",
        "/password-recovery/complete",
        "",
        std::string("{\"token\":\"") + reset_token + "\",\"new_password\":\"sekailink-password-2\"}");
    require(password_reset_complete_response.find("200 OK") != std::string::npos, "identity_password_reset_complete_status");

    const auto login_old_password_response = http_request(
        port,
        "POST",
        "/login",
        "",
        R"({"identity":"jade@example.com","password":"sekailink-password"})");
    require(login_old_password_response.find("401 Unauthorized") != std::string::npos, "identity_login_old_password_rejected");

    const auto login_new_password_response = http_request(
        port,
        "POST",
        "/login",
        "",
        R"({"identity":"jade@example.com","password":"sekailink-password-2"})");
    require(login_new_password_response.find("200 OK") != std::string::npos, "identity_login_new_password_status");
    const auto login_new_password_json = nlohmann::json::parse(extract_body(login_new_password_response));
    const auto post_reset_session_token = login_new_password_json.at("session").at("session_token").get<std::string>();

    const auto profile_response = http_request(port, "GET", "/profiles/jade", "", "");
    require(profile_response.find("200 OK") != std::string::npos, "identity_profile_status");
    const auto profile_json = nlohmann::json::parse(extract_body(profile_response));
    require(profile_json.at("profile").at("display_name").get<std::string>() == "Jade Runner", "identity_profile_display_name");
    require(profile_json.at("profile").at("email_verified").get<bool>() == true, "identity_profile_email_verified");
    require(profile_json.at("profile").at("bio").get<std::string>() == "Testing profile updates", "identity_profile_bio");

    const auto admin_add_response = http_request(
        port,
        "POST",
        "/admin/users",
        "identity-admin-secret",
        R"({"username":"ops-user","email":"ops@example.com","password":"sekailink-password-3","display_name":"Ops User","role":"supporter","permissions":["users.read","lobbies.read"],"email_verified":true})",
        {{"User-Agent", "SekaiLinkAdminCLI/1.0"},
         {"X-SekaiLink-Client", "admin-cli"},
         {"X-SekaiLink-Client-Version", "1.0.0-smoke"},
         {"X-SekaiLink-Device-Id", "ops-terminal-1"},
         {"X-SekaiLink-Locale", "en-US"}});
    require(admin_add_response.find("200 OK") != std::string::npos, "identity_admin_add_status");
    const auto admin_add_json = nlohmann::json::parse(extract_body(admin_add_response));
    require(admin_add_json.at("user").at("role").get<std::string>() == "supporter", "identity_admin_add_role");
    require(admin_add_json.at("user").at("permissions").size() == 2, "identity_admin_add_permissions");

    const auto ops_login_response = http_request(
        port,
        "POST",
        "/login",
        "",
        R"({"identity":"ops@example.com","password":"sekailink-password-3"})",
        {{"User-Agent", "SekaiLinkDesktop/1.0"},
         {"X-SekaiLink-Client", "sekai-client"},
         {"X-SekaiLink-Client-Version", "1.0.0-smoke"},
         {"X-SekaiLink-Device-Id", "ops-laptop-1"}});
    require(ops_login_response.find("200 OK") != std::string::npos, "identity_admin_target_login_status");
    const auto ops_login_json = nlohmann::json::parse(extract_body(ops_login_response));
    const auto ops_session_token = ops_login_json.at("session").at("session_token").get<std::string>();
    const auto ops_session_id = ops_login_json.at("session").at("session_id").get<std::int64_t>();

    const auto ops_activate_same_key_response = http_request(
        port,
        "POST",
        "/me/game-keys/activate",
        ops_session_token,
        std::string("{\"game_key\":\"") + game_key_code + "\"}");
    require(
        ops_activate_same_key_response.find("409 Conflict") != std::string::npos,
        "identity_game_key_reuse_rejected");

    const auto admin_game_key_info_response = http_request(
        port,
        "GET",
        std::string("/admin/game-keys/") + game_key_code,
        "identity-admin-secret",
        "");
    require(admin_game_key_info_response.find("200 OK") != std::string::npos, "identity_admin_game_key_info_status");
    const auto admin_game_key_info_json = nlohmann::json::parse(extract_body(admin_game_key_info_response));
    require(
        admin_game_key_info_json.at("key").at("bound_username").get<std::string>() == "jade",
        "identity_admin_game_key_bound_username");

    const auto admin_game_key_list_response = http_request(
        port,
        "GET",
        "/admin/game-keys?status=activated&entitlement=sekaiemu&username=jade",
        "identity-admin-secret",
        "");
    require(admin_game_key_list_response.find("200 OK") != std::string::npos, "identity_admin_game_key_list_status");
    const auto admin_game_key_list_json = nlohmann::json::parse(extract_body(admin_game_key_list_response));
    require(admin_game_key_list_json.at("keys").size() == 1, "identity_admin_game_key_list_count");

    const auto admin_game_key_deactivate_response = http_request(
        port,
        "PATCH",
        std::string("/admin/game-keys/") + game_key_code,
        "identity-admin-secret",
        R"({"status":"deactivated","notes":"manual suspend"})");
    require(
        admin_game_key_deactivate_response.find("200 OK") != std::string::npos,
        "identity_admin_game_key_deactivate_status");
    const auto admin_game_key_deactivate_json = nlohmann::json::parse(extract_body(admin_game_key_deactivate_response));
    require(
        admin_game_key_deactivate_json.at("key").at("status").get<std::string>() == "deactivated",
        "identity_admin_game_key_deactivated");

    const auto game_key_check_after_deactivate_response = http_request(
        port,
        "POST",
        "/me/game-keys/check",
        post_reset_session_token,
        std::string("{\"game_key\":\"") + game_key_code + "\"}");
    if (game_key_check_after_deactivate_response.find("200 OK") == std::string::npos) {
      throw std::runtime_error(
          std::string("identity_game_key_check_after_deactivate_status: ") + game_key_check_after_deactivate_response);
    }
    const auto game_key_check_after_deactivate_json =
        nlohmann::json::parse(extract_body(game_key_check_after_deactivate_response));
    require(
        game_key_check_after_deactivate_json.at("is_usable").get<bool>() == false,
        "identity_game_key_check_after_deactivate_usable");

    const auto security_after_key_deactivate_response = http_request(port, "GET", "/me/security", post_reset_session_token, "");
    require(
        security_after_key_deactivate_response.find("200 OK") != std::string::npos,
        "identity_security_after_key_deactivate_status");
    const auto security_after_key_deactivate_json =
        nlohmann::json::parse(extract_body(security_after_key_deactivate_response));
    require(
        security_after_key_deactivate_json.at("security").at("sekaiemu_access").get<bool>() == false,
        "identity_security_after_key_deactivate_access");

    const auto ops_phone_login_response = http_request(
        port,
        "POST",
        "/login",
        "",
        R"({"identity":"ops@example.com","password":"sekailink-password-3"})",
        {{"User-Agent", "SekaiLinkConnect/1.0"},
         {"X-SekaiLink-Client", "sekailink-connect"},
         {"X-SekaiLink-Client-Version", "1.0.0-smoke"},
         {"X-SekaiLink-Device-Id", "ops-phone-1"}});
    require(ops_phone_login_response.find("200 OK") != std::string::npos, "identity_admin_target_phone_login_status");
    const auto ops_phone_login_json = nlohmann::json::parse(extract_body(ops_phone_login_response));
    const auto ops_phone_session_token = ops_phone_login_json.at("session").at("session_token").get<std::string>();

    const auto ops_phone_duplicate_login_response = http_request(
        port,
        "POST",
        "/login",
        "",
        R"({"identity":"ops@example.com","password":"sekailink-password-3"})",
        {{"User-Agent", "SekaiLinkConnect/1.0"},
         {"X-SekaiLink-Client", "sekailink-connect"},
         {"X-SekaiLink-Client-Version", "1.0.0-smoke"},
         {"X-SekaiLink-Device-Id", "ops-phone-1"}});
    require(
        ops_phone_duplicate_login_response.find("200 OK") != std::string::npos,
        "identity_admin_target_phone_duplicate_login_status");
    const auto ops_phone_duplicate_login_json = nlohmann::json::parse(extract_body(ops_phone_duplicate_login_response));
    const auto ops_phone_duplicate_session_token =
        ops_phone_duplicate_login_json.at("session").at("session_token").get<std::string>();

    const auto admin_info_response = http_request(
        port,
        "GET",
        "/admin/users/ops-user",
        "identity-admin-secret",
        "",
        {{"User-Agent", "SekaiLinkAdminCLI/1.0"},
         {"X-SekaiLink-Client", "admin-cli"},
         {"X-SekaiLink-Client-Version", "1.0.0-smoke"},
         {"X-SekaiLink-Device-Id", "ops-terminal-1"}});
    require(admin_info_response.find("200 OK") != std::string::npos, "identity_admin_info_status");
    const auto admin_info_json = nlohmann::json::parse(extract_body(admin_info_response));
    require(admin_info_json.at("user").at("email_verified").get<bool>() == true, "identity_admin_info_email_verified");
    require(
        admin_info_json.at("session_inventory").at("total_sessions").get<int>() >= 1,
        "identity_admin_info_session_inventory_nonempty");

    const auto admin_sessions_response = http_request(
        port,
        "GET",
        "/admin/users/ops-user/sessions",
        "identity-admin-secret",
        "",
        {{"User-Agent", "SekaiLinkAdminCLI/1.0"},
         {"X-SekaiLink-Client", "admin-cli"},
         {"X-SekaiLink-Client-Version", "1.0.0-smoke"},
         {"X-SekaiLink-Device-Id", "ops-terminal-1"}});
    require(admin_sessions_response.find("200 OK") != std::string::npos, "identity_admin_sessions_status");
    const auto admin_sessions_json = nlohmann::json::parse(extract_body(admin_sessions_response));
    require(admin_sessions_json.at("session_inventory").at("total_sessions").get<int>() >= 3, "identity_admin_sessions_inventory");
    require(admin_sessions_json.at("sessions").at(0).at("session_state").is_string(), "identity_admin_sessions_state");

    const auto admin_devices_response = http_request(
        port,
        "GET",
        "/admin/users/ops-user/devices",
        "identity-admin-secret",
        "",
        {{"User-Agent", "SekaiLinkAdminCLI/1.0"},
         {"X-SekaiLink-Client", "admin-cli"},
         {"X-SekaiLink-Client-Version", "1.0.0-smoke"},
         {"X-SekaiLink-Device-Id", "ops-terminal-1"}});
    require(admin_devices_response.find("200 OK") != std::string::npos, "identity_admin_devices_status");
    const auto admin_devices_json = nlohmann::json::parse(extract_body(admin_devices_response));
    require(admin_devices_json.at("device_count").get<int>() >= 2, "identity_admin_devices_count");

    const auto admin_revoke_device_response = http_request(
        port,
        "POST",
        "/admin/users/ops-user/devices/revoke",
        "identity-admin-secret",
        R"({"device_key":"ops-phone-1"})",
        {{"User-Agent", "SekaiLinkAdminCLI/1.0"},
         {"X-SekaiLink-Client", "admin-cli"},
         {"X-SekaiLink-Client-Version", "1.0.0-smoke"},
         {"X-SekaiLink-Device-Id", "ops-terminal-1"}});
    require(admin_revoke_device_response.find("200 OK") != std::string::npos, "identity_admin_revoke_device_status");
    const auto admin_revoke_device_json = nlohmann::json::parse(extract_body(admin_revoke_device_response));
    require(admin_revoke_device_json.at("revoked_count").get<int>() == 2, "identity_admin_revoke_device_count");
    const auto ops_me_after_device_revoke_response = http_request(port, "GET", "/me", ops_phone_session_token, "");
    require(
        ops_me_after_device_revoke_response.find("401 Unauthorized") != std::string::npos,
        "identity_admin_revoke_device_effect_primary");
    const auto ops_me_after_device_revoke_duplicate_response =
        http_request(port, "GET", "/me", ops_phone_duplicate_session_token, "");
    require(
        ops_me_after_device_revoke_duplicate_response.find("401 Unauthorized") != std::string::npos,
        "identity_admin_revoke_device_effect_duplicate");

    const auto ops_tablet_login_response = http_request(
        port,
        "POST",
        "/login",
        "",
        R"({"identity":"ops@example.com","password":"sekailink-password-3"})",
        {{"User-Agent", "SekaiLinkTablet/1.0"},
         {"X-SekaiLink-Client", "sekai-client"},
         {"X-SekaiLink-Client-Version", "1.0.0-smoke"},
         {"X-SekaiLink-Device-Id", "ops-tablet-1"}});
    require(ops_tablet_login_response.find("200 OK") != std::string::npos, "identity_admin_target_tablet_login_status");
    const auto ops_tablet_login_json = nlohmann::json::parse(extract_body(ops_tablet_login_response));
    const auto ops_tablet_session_token = ops_tablet_login_json.at("session").at("session_token").get<std::string>();

    const auto admin_revoke_others_response = http_request(
        port,
        "POST",
        "/admin/users/ops-user/sessions/revoke-others",
        "identity-admin-secret",
        "",
        {{"User-Agent", "SekaiLinkAdminCLI/1.0"},
         {"X-SekaiLink-Client", "admin-cli"},
         {"X-SekaiLink-Client-Version", "1.0.0-smoke"},
         {"X-SekaiLink-Device-Id", "ops-terminal-1"}});
    require(admin_revoke_others_response.find("200 OK") != std::string::npos, "identity_admin_revoke_others_status");
    const auto admin_revoke_others_json = nlohmann::json::parse(extract_body(admin_revoke_others_response));
    require(admin_revoke_others_json.at("revoked_count").get<int>() >= 2, "identity_admin_revoke_others_count");
    const auto ops_me_after_revoke_others_response = http_request(port, "GET", "/me", ops_tablet_session_token, "");
    require(
        ops_me_after_revoke_others_response.find("401 Unauthorized") != std::string::npos,
        "identity_admin_revoke_others_effect");

    const auto admin_revoke_session_response = http_request(
        port,
        "POST",
        std::string("/admin/users/ops-user/sessions/") + std::to_string(ops_session_id) + "/revoke",
        "identity-admin-secret",
        "",
        {{"User-Agent", "SekaiLinkAdminCLI/1.0"},
         {"X-SekaiLink-Client", "admin-cli"},
         {"X-SekaiLink-Client-Version", "1.0.0-smoke"},
         {"X-SekaiLink-Device-Id", "ops-terminal-1"}});
    require(admin_revoke_session_response.find("200 OK") != std::string::npos, "identity_admin_revoke_session_status");
    const auto ops_me_after_revoke_response = http_request(port, "GET", "/me", ops_session_token, "");
    require(ops_me_after_revoke_response.find("401 Unauthorized") != std::string::npos, "identity_admin_revoke_session_effect");
    const auto admin_list_response = http_request(
        port,
        "GET",
        "/admin/users",
        "identity-admin-secret",
        "",
        {{"User-Agent", "SekaiLinkAdminCLI/1.0"},
         {"X-SekaiLink-Client", "admin-cli"},
         {"X-SekaiLink-Client-Version", "1.0.0-smoke"},
         {"X-SekaiLink-Device-Id", "ops-terminal-1"}});
    require(admin_list_response.find("200 OK") != std::string::npos, "identity_admin_list_status");
    const auto admin_list_json = nlohmann::json::parse(extract_body(admin_list_response));
    require(!admin_list_json.at("users").empty(), "identity_admin_list_nonempty");
    const auto admin_list_filtered_response = http_request(
        port,
        "GET",
        "/admin/users?limit=1&query=ops",
        "identity-admin-secret",
        "",
        {{"User-Agent", "SekaiLinkAdminCLI/1.0"},
         {"X-SekaiLink-Client", "admin-cli"},
         {"X-SekaiLink-Client-Version", "1.0.0-smoke"},
         {"X-SekaiLink-Device-Id", "ops-terminal-1"}});
    require(admin_list_filtered_response.find("200 OK") != std::string::npos, "identity_admin_list_filtered_status");
    const auto admin_list_filtered_json = nlohmann::json::parse(extract_body(admin_list_filtered_response));
    require(admin_list_filtered_json.at("users").size() == 1, "identity_admin_list_filtered_size");
    require(admin_list_filtered_json.at("users").at(0).at("username").get<std::string>() == "ops-user", "identity_admin_list_filtered_user");
    bool found_admin_create_context = false;
    bool found_admin_info_context = false;
    for (const auto& event : admin_info_json.at("auth_audit")) {
      if (!event.at("payload").contains("admin_context")) {
        continue;
      }
      const auto& admin_context = event.at("payload").at("admin_context");
      if (admin_context.at("actor_type").get<std::string>() != "admin_token") {
        continue;
      }
      if (!admin_context.contains("request_context") || !admin_context.at("request_context").contains("client_name")) {
        continue;
      }
      if (admin_context.at("request_context").at("client_name").get<std::string>() != "admin-cli") {
        continue;
      }
      const auto event_type = event.at("event_type").get<std::string>();
      if (event_type == "admin_user_created") {
        found_admin_create_context = true;
      }
      if (event_type == "admin_user_info_viewed") {
        found_admin_info_context = true;
      }
    }
    require(found_admin_create_context, "identity_admin_create_audit_context");
    require(found_admin_info_context, "identity_admin_info_audit_context");
    const auto admin_audit_response = http_request(
        port,
        "GET",
        "/admin/users/ops-user/audit?limit=10&event_type=admin_user_info_viewed",
        "identity-admin-secret",
        "",
        {{"User-Agent", "SekaiLinkAdminCLI/1.0"},
         {"X-SekaiLink-Client", "admin-cli"},
         {"X-SekaiLink-Client-Version", "1.0.0-smoke"},
         {"X-SekaiLink-Device-Id", "ops-terminal-1"}});
    require(admin_audit_response.find("200 OK") != std::string::npos, "identity_admin_audit_status");
    const auto admin_audit_json = nlohmann::json::parse(extract_body(admin_audit_response));
    require(admin_audit_json.at("auth_audit").is_array(), "identity_admin_audit_array");

    const auto admin_edit_response = http_request(
        port,
        "PATCH",
        "/admin/users/ops-user",
        "identity-admin-secret",
        R"({"role":"mod","permissions":["users.read","users.write"],"disabled":false})",
        {{"User-Agent", "SekaiLinkAdminCLI/1.0"},
         {"X-SekaiLink-Client", "admin-cli"},
         {"X-SekaiLink-Client-Version", "1.0.0-smoke"},
         {"X-SekaiLink-Device-Id", "ops-terminal-1"}});
    require(admin_edit_response.find("200 OK") != std::string::npos, "identity_admin_edit_status");
    const auto admin_edit_json = nlohmann::json::parse(extract_body(admin_edit_response));
    require(admin_edit_json.at("user").at("role").get<std::string>() == "mod", "identity_admin_edit_role");

    const auto admin_delete_response = http_request(
        port,
        "DELETE",
        "/admin/users/ops-user",
        "identity-admin-secret",
        "",
        {{"User-Agent", "SekaiLinkAdminCLI/1.0"},
         {"X-SekaiLink-Client", "admin-cli"},
         {"X-SekaiLink-Client-Version", "1.0.0-smoke"},
         {"X-SekaiLink-Device-Id", "ops-terminal-1"}});
    require(admin_delete_response.find("200 OK") != std::string::npos, "identity_admin_delete_status");
    const auto admin_info_after_delete_response = http_request(
        port,
        "GET",
        "/admin/users/ops-user",
        "identity-admin-secret",
        "",
        {{"User-Agent", "SekaiLinkAdminCLI/1.0"},
         {"X-SekaiLink-Client", "admin-cli"},
         {"X-SekaiLink-Client-Version", "1.0.0-smoke"},
         {"X-SekaiLink-Device-Id", "ops-terminal-1"}});
    require(admin_info_after_delete_response.find("200 OK") != std::string::npos, "identity_admin_info_after_delete_status");
    const auto admin_info_after_delete_json = nlohmann::json::parse(extract_body(admin_info_after_delete_response));
    bool found_admin_update_context = false;
    bool found_admin_delete_context = false;
    for (const auto& event : admin_info_after_delete_json.at("auth_audit")) {
      if (!event.at("payload").contains("admin_context")) {
        continue;
      }
      const auto event_type = event.at("event_type").get<std::string>();
      if (event_type == "admin_user_updated") {
        found_admin_update_context = true;
      }
      if (event_type == "admin_user_disabled") {
        found_admin_delete_context = true;
      }
    }
    require(found_admin_update_context, "identity_admin_update_audit_context");
    require(found_admin_delete_context, "identity_admin_delete_audit_context");
    const auto disabled_login_response = http_request(
        port,
        "POST",
        "/login",
        "",
        R"({"identity":"ops@example.com","password":"sekailink-password-3"})");
    require(disabled_login_response.find("403") != std::string::npos, "identity_admin_disabled_login_rejected");

    require(fs::exists(state_path), "identity_state_file_missing");
    std::ifstream state_stream(state_path);
    nlohmann::json state_json;
    state_stream >> state_json;
    require(state_json.at("service").get<std::string>() == "sekailink_identity_service", "identity_state_service");
    require(state_json.at("total_requests").get<int>() >= 10, "identity_state_requests");

    ::kill(pid, SIGTERM);
    int status = 0;
    ::waitpid(pid, &status, 0);
    require(WIFEXITED(status) && WEXITSTATUS(status) == 0, "identity_service_exit");

    std::cout << "identity_service_ok=1\n";
    return EXIT_SUCCESS;
  } catch (const std::exception& exception) {
    std::cerr << "identity_service_smoke failed: " << exception.what() << "\n";
    return EXIT_FAILURE;
  }
#endif
}
