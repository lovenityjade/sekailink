#pragma once

#include "nlohmann/json.hpp"
#include <sqlite3.h>

#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace sekailink_server {

struct IdentityServiceConfig {
  std::uint16_t http_port = 19095;
  std::string listen_host = "127.0.0.1";
  std::filesystem::path sqlite_path;
  std::uint32_t password_iterations = 120000;
  std::uint32_t password_time_cost = 3;
  std::uint32_t password_memory_kib = 65536;
  std::uint32_t password_parallelism = 1;
  std::uint32_t password_hash_length = 32;
  std::uint32_t password_salt_length = 16;
  std::uint32_t session_ttl_seconds = 60 * 60 * 24 * 30;
  std::uint32_t password_reset_ttl_seconds = 60 * 30;
  std::uint32_t email_verification_ttl_seconds = 60 * 60 * 24;
  std::string public_base_url = "https://sekailink.com";
  std::string password_reset_path = "/reset-password";
  std::string email_verification_path = "/verify-email";
  std::string mail_from = "noreply@sekailink.com";
  std::string sendmail_path = "/usr/sbin/sendmail";
  std::string admin_token;
  std::filesystem::path state_path;
  std::string patreon_client_id;
  std::string patreon_client_secret;
  std::string patreon_redirect_uri;
  std::string patreon_oauth_scopes = "identity identity[email] memberships";
  std::string patreon_api_version = "2";
  std::string patreon_creator_access_token;
  std::string patreon_creator_refresh_token;
};

struct IdentityRequestContext {
  std::optional<std::string> user_agent;
  std::optional<std::string> client_name;
  std::optional<std::string> client_version;
  std::optional<std::string> device_id;
  std::optional<std::string> locale;
};

IdentityServiceConfig load_identity_service_config(const std::filesystem::path& path);
nlohmann::json to_json(const IdentityServiceConfig& config);

class IdentityStore {
 public:
  explicit IdentityStore(std::filesystem::path sqlite_path);
  ~IdentityStore();

  IdentityStore(const IdentityStore&) = delete;
  IdentityStore& operator=(const IdentityStore&) = delete;

  struct RegisteredUser {
    std::int64_t user_id = 0;
    std::string username;
    std::string email;
    bool email_verified = false;
    std::string display_name;
    std::string avatar_url;
    std::string bio;
    std::string locale;
    std::string role = "player";
    std::vector<std::string> permissions;
    std::optional<std::string> disabled_at;
    std::optional<std::string> patreon_id;
    std::optional<std::string> patreon_email;
    std::optional<std::string> patreon_member_id;
    std::optional<std::string> patreon_status;
    std::optional<std::string> patreon_tier;
    bool patreon_is_supporter = false;
    std::optional<std::string> patreon_linked_at;
  };

  struct LoadedUser {
    std::int64_t user_id = 0;
    std::string username;
    std::string email;
    bool email_verified = false;
    std::string display_name;
    std::string avatar_url;
    std::string bio;
    std::string locale;
    std::string password_salt_b64;
    std::string password_hash_b64;
    std::uint32_t password_iterations = 0;
    bool two_factor_enabled = false;
    std::string two_factor_secret_b32;
    std::string role = "player";
    std::vector<std::string> permissions;
    std::optional<std::string> disabled_at;
    std::optional<std::string> patreon_id;
    std::optional<std::string> patreon_email;
    std::optional<std::string> patreon_member_id;
    std::optional<std::string> patreon_status;
    std::optional<std::string> patreon_tier;
    bool patreon_is_supporter = false;
    std::optional<std::string> patreon_linked_at;
  };

  struct SessionRecord {
    std::int64_t session_id = 0;
    std::string session_token;
    std::int64_t user_id = 0;
    std::string username;
    std::string email;
    bool email_verified = false;
    std::string display_name;
    std::string avatar_url;
    std::string bio;
    std::string locale;
    std::string created_at;
    std::string expires_at;
    std::optional<std::string> revoked_at;
    std::optional<std::string> user_agent;
    std::optional<std::string> client_name;
    std::optional<std::string> client_version;
    std::optional<std::string> device_id;
    std::optional<std::string> requested_locale;
    std::string role = "player";
    std::vector<std::string> permissions;
    std::optional<std::string> patreon_tier;
    bool patreon_is_supporter = false;
  };

  struct SecurityState {
    bool two_factor_enabled = false;
    std::size_t recovery_code_count = 0;
  };

  struct TwoFactorSetup {
    std::string secret_b32;
  };

  struct PasswordResetRecord {
    std::int64_t user_id = 0;
    std::string username;
    std::string email;
    bool email_verified = false;
    std::string display_name;
    std::string token_hash_b64;
    std::string expires_at;
  };

  struct EmailVerificationRecord {
    std::int64_t user_id = 0;
    std::string username;
    std::string email;
    std::string display_name;
    std::string token_hash_b64;
    std::string expires_at;
  };

  struct AuthAuditRecord {
    std::int64_t event_id = 0;
    std::int64_t user_id = 0;
    std::string event_type;
    std::string created_at;
    nlohmann::json payload;
  };

  struct OAuthLinkStateRecord {
    std::int64_t user_id = 0;
    std::string provider;
    std::string expires_at;
  };

  struct GameKeyRecord {
    std::int64_t key_id = 0;
    std::string key_code;
    std::string status;
    std::vector<std::string> entitlements;
    std::string created_at;
    std::optional<std::string> activated_at;
    std::optional<std::string> deactivated_at;
    std::optional<std::int64_t> bound_user_id;
    std::optional<std::string> bound_username;
    std::string notes;
  };

  struct GameKeyActivationResult {
    enum class Outcome {
      Activated,
      AlreadyActivatedBySelf,
      ActivatedByOther,
      Deactivated,
      NotFound,
      InvalidFormat,
    };

    Outcome outcome = Outcome::NotFound;
    std::optional<GameKeyRecord> key;
  };

  RegisteredUser create_user(
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
      std::uint32_t password_iterations);
  std::optional<LoadedUser> find_user_by_identity(const std::string& identity) const;
  std::optional<LoadedUser> find_user_by_username(const std::string& username) const;
  std::vector<RegisteredUser> list_users(
      std::size_t limit = 100,
      const std::optional<std::string>& query = std::nullopt,
      const std::optional<std::string>& role = std::nullopt,
      const std::optional<bool>& disabled = std::nullopt,
      std::size_t offset = 0) const;
  SessionRecord create_session(
      std::int64_t user_id,
      std::uint32_t ttl_seconds,
      const IdentityRequestContext& context);
  std::optional<SessionRecord> find_session(const std::string& session_token) const;
  std::vector<SessionRecord> list_sessions(std::int64_t user_id) const;
  std::optional<LoadedUser> update_profile(
      std::int64_t user_id,
      const std::optional<std::string>& display_name,
      const std::optional<std::string>& avatar_url,
      const std::optional<std::string>& bio,
      const std::optional<std::string>& locale);
  std::optional<LoadedUser> admin_update_user(
      std::int64_t user_id,
      const std::optional<std::string>& email,
      const std::optional<std::string>& display_name,
      const std::optional<std::string>& avatar_url,
      const std::optional<std::string>& bio,
      const std::optional<std::string>& locale,
      const std::optional<std::string>& role,
      const std::optional<std::vector<std::string>>& permissions,
      const std::optional<bool>& email_verified,
      const std::optional<bool>& disabled);
  SecurityState security_state(std::int64_t user_id) const;
  std::vector<std::string> regenerate_recovery_codes(std::int64_t user_id, std::size_t count);
  TwoFactorSetup issue_two_factor_secret(std::int64_t user_id);
  bool enable_two_factor(std::int64_t user_id, const std::string& secret_b32);
  void disable_two_factor(std::int64_t user_id);
  bool consume_recovery_code(std::int64_t user_id, const std::string& code);
  void update_password(std::int64_t user_id, const std::string& password_salt_b64, const std::string& password_hash_b64, std::uint32_t password_iterations);
  void revoke_all_sessions(std::int64_t user_id);
  bool revoke_session(std::int64_t user_id, std::int64_t session_id);
  std::size_t revoke_other_sessions(std::int64_t user_id, std::optional<std::int64_t> keep_session_id = std::nullopt);
  std::vector<std::int64_t> revoke_device_sessions(std::int64_t user_id, const std::string& device_key);
  void issue_password_reset(
      std::int64_t user_id,
      const std::string& token_hash_b64,
      const std::string& expires_at);
  std::optional<PasswordResetRecord> find_password_reset(const std::string& raw_token) const;
  void consume_password_reset(std::int64_t user_id, const std::string& raw_token);
  void issue_email_verification(
      std::int64_t user_id,
      const std::string& token_hash_b64,
      const std::string& expires_at);
  std::optional<EmailVerificationRecord> find_email_verification(const std::string& raw_token) const;
  void consume_email_verification(std::int64_t user_id, const std::string& raw_token);
  void mark_email_verified(std::int64_t user_id);
  void issue_oauth_link_state(
      std::int64_t user_id,
      const std::string& provider,
      const std::string& state_hash_b64,
      const std::string& expires_at);
  std::optional<OAuthLinkStateRecord> consume_oauth_link_state(
      std::int64_t user_id,
      const std::string& provider,
      const std::string& raw_state);
  std::optional<LoadedUser> link_patreon_account(
      std::int64_t user_id,
      const std::optional<std::string>& patreon_id,
      const std::optional<std::string>& patreon_email,
      const std::optional<std::string>& patreon_member_id,
      const std::optional<std::string>& patreon_status,
      const std::optional<std::string>& patreon_tier,
      bool patreon_is_supporter,
      const std::optional<std::string>& patreon_linked_at);
  std::optional<LoadedUser> unlink_patreon_account(std::int64_t user_id);
  std::vector<GameKeyRecord> generate_game_keys(
      std::size_t count,
      const std::vector<std::string>& entitlements,
      const std::string& notes);
  std::optional<GameKeyRecord> find_game_key_by_code(const std::string& key_code) const;
  std::vector<GameKeyRecord> list_game_keys(
      std::size_t limit = 100,
      const std::optional<std::string>& status = std::nullopt,
      const std::optional<std::string>& entitlement = std::nullopt,
      const std::optional<std::string>& username = std::nullopt,
      std::size_t offset = 0) const;
  std::vector<GameKeyRecord> list_user_game_keys(std::int64_t user_id) const;
  GameKeyActivationResult activate_game_key(std::int64_t user_id, const std::string& key_code);
  std::optional<GameKeyRecord> update_game_key(
      const std::string& key_code,
      const std::optional<std::string>& status,
      const std::optional<std::vector<std::string>>& entitlements,
      const std::optional<std::string>& notes);
  bool user_has_active_entitlement(std::int64_t user_id, const std::string& entitlement) const;
  std::vector<std::string> list_user_active_entitlements(std::int64_t user_id) const;
  void append_auth_audit(std::int64_t user_id, const std::string& event_type, const nlohmann::json& payload);
  std::vector<AuthAuditRecord> list_auth_audit(std::int64_t user_id, std::size_t limit) const;

 private:
  void open();
  void close();
  void init_schema();
  void exec(const std::string& sql) const;
  void exec_allow_duplicate_column(const std::string& sql) const;

  std::filesystem::path sqlite_path_;
  sqlite3* db_ = nullptr;
};

class IdentityService {
 public:
  explicit IdentityService(IdentityServiceConfig config);

  [[nodiscard]] nlohmann::json handle(
      const std::string& method,
      const std::string& path,
      const std::optional<std::string>& bearer_token,
      const std::optional<nlohmann::json>& body,
      const IdentityRequestContext& context) const;

 private:
  [[nodiscard]] nlohmann::json handle_register(const nlohmann::json& body, const IdentityRequestContext& context) const;
  [[nodiscard]] nlohmann::json handle_login(const nlohmann::json& body, const IdentityRequestContext& context) const;
  [[nodiscard]] nlohmann::json handle_me(const std::optional<std::string>& bearer_token) const;
  [[nodiscard]] nlohmann::json handle_sessions(const std::optional<std::string>& bearer_token) const;
  [[nodiscard]] nlohmann::json handle_revoke_session(
      const std::optional<std::string>& bearer_token,
      const nlohmann::json& body) const;
  [[nodiscard]] nlohmann::json handle_revoke_other_sessions(const std::optional<std::string>& bearer_token) const;
  [[nodiscard]] nlohmann::json handle_profile(const std::string& username) const;
  [[nodiscard]] nlohmann::json handle_update_profile(
      const std::optional<std::string>& bearer_token,
      const nlohmann::json& body) const;
  [[nodiscard]] nlohmann::json handle_security(const std::optional<std::string>& bearer_token) const;
  [[nodiscard]] nlohmann::json handle_auth_audit(const std::optional<std::string>& bearer_token) const;
  [[nodiscard]] nlohmann::json handle_regenerate_recovery_codes(
      const std::optional<std::string>& bearer_token) const;
  [[nodiscard]] nlohmann::json handle_two_factor_setup(const std::optional<std::string>& bearer_token) const;
  [[nodiscard]] nlohmann::json handle_two_factor_enable(
      const std::optional<std::string>& bearer_token,
      const nlohmann::json& body) const;
  [[nodiscard]] nlohmann::json handle_two_factor_disable(
      const std::optional<std::string>& bearer_token,
      const nlohmann::json& body) const;
  [[nodiscard]] nlohmann::json handle_email_verification_request(
      const std::optional<std::string>& bearer_token) const;
  [[nodiscard]] nlohmann::json handle_patreon_link_begin(
      const std::optional<std::string>& bearer_token) const;
  [[nodiscard]] nlohmann::json handle_patreon_link_complete(
      const std::optional<std::string>& bearer_token,
      const nlohmann::json& body) const;
  [[nodiscard]] nlohmann::json handle_patreon_link_status(
      const std::optional<std::string>& bearer_token) const;
  [[nodiscard]] nlohmann::json handle_patreon_link_unlink(
      const std::optional<std::string>& bearer_token) const;
  [[nodiscard]] nlohmann::json handle_my_game_keys(
      const std::optional<std::string>& bearer_token) const;
  [[nodiscard]] nlohmann::json handle_my_game_key_activate(
      const std::optional<std::string>& bearer_token,
      const nlohmann::json& body) const;
  [[nodiscard]] nlohmann::json handle_my_game_key_check(
      const std::optional<std::string>& bearer_token,
      const nlohmann::json& body) const;
  [[nodiscard]] nlohmann::json handle_game_key_lookup(const nlohmann::json& body) const;
  [[nodiscard]] nlohmann::json handle_admin_generate_game_keys(
      const std::optional<std::string>& bearer_token,
      const nlohmann::json& body,
      const IdentityRequestContext& context) const;
  [[nodiscard]] nlohmann::json handle_admin_list_game_keys(
      const std::optional<std::string>& bearer_token,
      const IdentityRequestContext& context,
      const std::optional<std::string>& status = std::nullopt,
      const std::optional<std::string>& entitlement = std::nullopt,
      const std::optional<std::string>& username = std::nullopt,
      std::size_t limit = 100,
      std::size_t offset = 0) const;
  [[nodiscard]] nlohmann::json handle_admin_game_key_info(
      const std::optional<std::string>& bearer_token,
      const std::string& key_code,
      const IdentityRequestContext& context) const;
  [[nodiscard]] nlohmann::json handle_admin_update_game_key(
      const std::optional<std::string>& bearer_token,
      const std::string& key_code,
      const nlohmann::json& body,
      const IdentityRequestContext& context) const;
  [[nodiscard]] nlohmann::json handle_email_verification_complete(const nlohmann::json& body) const;
  [[nodiscard]] nlohmann::json handle_password_reset_request(const nlohmann::json& body) const;
  [[nodiscard]] nlohmann::json handle_password_reset_complete(const nlohmann::json& body) const;
  [[nodiscard]] nlohmann::json handle_admin_add_user(
      const std::optional<std::string>& bearer_token,
      const nlohmann::json& body,
      const IdentityRequestContext& context) const;
  [[nodiscard]] nlohmann::json handle_admin_list_users(
      const std::optional<std::string>& bearer_token,
      const IdentityRequestContext& context,
      const std::optional<std::string>& query = std::nullopt,
      const std::optional<std::string>& role = std::nullopt,
      const std::optional<bool>& disabled = std::nullopt,
      std::size_t limit = 100,
      std::size_t offset = 0) const;
  [[nodiscard]] nlohmann::json handle_admin_user_info(
      const std::optional<std::string>& bearer_token,
      const std::string& username,
      const IdentityRequestContext& context) const;
  [[nodiscard]] nlohmann::json handle_admin_list_user_sessions(
      const std::optional<std::string>& bearer_token,
      const std::string& username,
      const IdentityRequestContext& context) const;
  [[nodiscard]] nlohmann::json handle_admin_list_user_audit(
      const std::optional<std::string>& bearer_token,
      const std::string& username,
      const IdentityRequestContext& context,
      const std::optional<std::string>& event_type,
      std::size_t limit,
      std::size_t offset) const;
  [[nodiscard]] nlohmann::json handle_admin_list_user_devices(
      const std::optional<std::string>& bearer_token,
      const std::string& username,
      const IdentityRequestContext& context) const;
  [[nodiscard]] nlohmann::json handle_admin_revoke_user_session(
      const std::optional<std::string>& bearer_token,
      const std::string& username,
      std::int64_t session_id,
      const IdentityRequestContext& context) const;
  [[nodiscard]] nlohmann::json handle_admin_revoke_other_user_sessions(
      const std::optional<std::string>& bearer_token,
      const std::string& username,
      const IdentityRequestContext& context) const;
  [[nodiscard]] nlohmann::json handle_admin_revoke_user_device_sessions(
      const std::optional<std::string>& bearer_token,
      const std::string& username,
      const nlohmann::json& body,
      const IdentityRequestContext& context) const;
  [[nodiscard]] nlohmann::json handle_admin_edit_user(
      const std::optional<std::string>& bearer_token,
      const std::string& username,
      const nlohmann::json& body,
      const IdentityRequestContext& context) const;
  [[nodiscard]] nlohmann::json handle_admin_delete_user(
      const std::optional<std::string>& bearer_token,
      const std::string& username,
      const IdentityRequestContext& context) const;
  [[nodiscard]] nlohmann::json handle_admin_force_password_reset(
      const std::optional<std::string>& bearer_token,
      const std::string& username,
      const IdentityRequestContext& context) const;
  [[nodiscard]] static nlohmann::json session_to_json(const IdentityStore::SessionRecord& session);
  [[nodiscard]] static nlohmann::json user_to_json(const IdentityStore::LoadedUser& user, bool include_private_links = false);
  [[nodiscard]] static nlohmann::json patreon_to_json(const IdentityStore::LoadedUser& user, bool include_private);
  [[nodiscard]] static nlohmann::json game_key_to_json(const IdentityStore::GameKeyRecord& key, bool include_binding = true);
  [[nodiscard]] nlohmann::json build_game_key_summary(std::int64_t user_id) const;
  [[nodiscard]] bool is_admin_authorized(const std::optional<std::string>& bearer_token) const;
  void send_email_verification_email(
      const IdentityStore::LoadedUser& user,
      const std::string& raw_token,
      const std::string& expires_at) const;
  void send_password_reset_email(
      const IdentityStore::LoadedUser& user,
      const std::string& raw_token,
      const std::string& expires_at) const;
  void record_request() const;
  void record_error() const;
  void write_state_file() const;

  IdentityServiceConfig config_;
  mutable IdentityStore store_;
  mutable std::mutex state_mutex_;
  mutable std::uint64_t total_requests_ = 0;
  mutable std::uint64_t total_errors_ = 0;
};

class IdentityHttpServer {
 public:
  explicit IdentityHttpServer(IdentityServiceConfig config);
  ~IdentityHttpServer();

  IdentityHttpServer(const IdentityHttpServer&) = delete;
  IdentityHttpServer& operator=(const IdentityHttpServer&) = delete;

  [[nodiscard]] bool start();
  void stop();
  [[nodiscard]] std::uint16_t port() const;
  void serve_one() const;

 private:
  int listen_fd_ = -1;
  std::uint16_t bound_port_ = 0;
  IdentityService service_;
  IdentityServiceConfig config_;
};

}  // namespace sekailink_server
