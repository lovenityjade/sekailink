#include "sekailink_server/chat_api_service.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

#include <sqlite3.h>

using namespace sekailink_server;

int main() {
  ChatApiServiceConfig config;
  config.chat_gateway_token = "test-token";
  ChatApiService service(config);

  const auto health = service.handle("GET", "/health", std::nullopt, std::nullopt, std::nullopt);
  if (health.status != 200) {
    std::cerr << "health failed\n";
    return EXIT_FAILURE;
  }
  const auto denied = service.handle("GET", "/channels", std::nullopt, std::nullopt, std::nullopt);
  if (denied.status != 401) {
    std::cerr << "auth guard failed\n";
    return EXIT_FAILURE;
  }
  const auto options = service.handle("OPTIONS", "/channels", std::nullopt, std::nullopt, std::nullopt);
  if (options.status != 204) {
    std::cerr << "options failed\n";
    return EXIT_FAILURE;
  }

  const auto manifest_path = std::filesystem::temp_directory_path() / "sekailink-chat-api-release-smoke.json";
  {
    std::ofstream out(manifest_path);
    out << R"({"version":"0.3.1-smoke","channel":"test","platform":"linux-x64","artifact_type":"client-bundle","download_url":"https://sekailink.example/client.zip","sha256":"abc123","fallback_download_url":"https://sekailink.example/client.AppImage","fallback_sha256":"def456","fallback_artifact_type":"appimage","requires_client_updater":"bundle-v1"})";
  }
  ChatApiServiceConfig release_config;
  release_config.chat_gateway_token = "test-token";
  release_config.client_release_manifest_path = manifest_path;
  release_config.generation_handoff_root = std::filesystem::temp_directory_path() / "sekailink-chat-api-generation-handoff";
  release_config.generation_handoff_command = {"/bin/true"};
  const auto config_json = to_json(release_config);
  if (config_json.at("client_release_manifest_configured") != true ||
      config_json.at("generation_handoff_configured") != true ||
      config_json.at("generation_handoff_root_configured") != true) {
    std::cerr << "release config reflection failed\n";
    return EXIT_FAILURE;
  }
  ChatApiService release_service(release_config);
  const auto release = release_service.handle("GET", "/client/release-latest?channel=test&platform=linux-x64", std::nullopt, std::nullopt, std::nullopt);
  if (release.status != 200 ||
      release.body.find("0.3.1-smoke") == std::string::npos ||
      release.body.find("client-bundle") == std::string::npos ||
      release.body.find("def456") == std::string::npos) {
    std::cerr << "release manifest failed\n";
    return EXIT_FAILURE;
  }
  const auto stable_release = release_service.handle("GET", "/client/release-latest?channel=stable&platform=linux-x64", std::nullopt, std::nullopt, std::nullopt);
  if (stable_release.status != 200 || stable_release.body.find("0.3.1-smoke") != std::string::npos) {
    std::cerr << "release channel isolation failed\n";
    return EXIT_FAILURE;
  }

  const auto store_path = std::filesystem::temp_directory_path() / "sekailink-chat-api-presence-smoke.sqlite3";
  std::filesystem::remove(store_path);
  ChatApiServiceConfig store_config;
  store_config.sqlite_path = store_path;
  ChatApiService store_service(store_config);
  (void)store_service;
  sqlite3* db = nullptr;
  if (sqlite3_open(store_path.string().c_str(), &db) != SQLITE_OK || db == nullptr) {
    if (db) sqlite3_close(db);
    std::cerr << "presence schema db open failed\n";
    return EXIT_FAILURE;
  }
  sqlite3_stmt* stmt = nullptr;
  std::set<std::string> columns;
  if (sqlite3_prepare_v2(db, "PRAGMA table_info(chat_presence);", -1, &stmt, nullptr) == SQLITE_OK) {
    while (sqlite3_step(stmt) == SQLITE_ROW) {
      const auto* text = sqlite3_column_text(stmt, 1);
      if (text) columns.insert(reinterpret_cast<const char*>(text));
    }
    sqlite3_finalize(stmt);
  }
  sqlite3_close(db);
  for (const auto& column : {"role", "ready", "local_ready_known", "local_ready", "local_ready_note"}) {
    if (!columns.count(column)) {
      std::cerr << "presence schema missing " << column << "\n";
      return EXIT_FAILURE;
    }
  }
  return EXIT_SUCCESS;
}
