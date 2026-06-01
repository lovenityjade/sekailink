#include "sekailink_server/room_server_config.hpp"
#include "sekailink_server/room_server_node.hpp"

#ifndef _WIN32
#include <csignal>
#include <unistd.h>
#endif

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <thread>

using namespace sekailink_server;

namespace {

std::atomic<bool> g_stop_requested{false};
constexpr const char* kLoopbackBindHost = "127.0.0.1";

#ifndef _WIN32
void handle_signal(int) {
  g_stop_requested = true;
}
#endif

void apply_env_auth_overrides(RoomServerNodeConfig& config) {
  if (const auto* token = std::getenv("SEKAILINK_ROOM_SERVER_ADMIN_TOKEN"); token != nullptr && *token != '\0') {
    config.auth_policy.admin_token = token;
  }
  if (const auto* token = std::getenv("SEKAILINK_ROOM_SERVER_RUNTIME_TOKEN"); token != nullptr && *token != '\0') {
    config.auth_policy.runtime_token = token;
  }
  if (const auto* token = std::getenv("SEKAILINK_ROOM_SERVER_CLIENT_REPORT_TOKEN"); token != nullptr && *token != '\0') {
    config.auth_policy.client_report_token = token;
  }
}

void emit_service_log(
    std::ostream& stream,
    const std::string& event,
    nlohmann::json payload = nlohmann::json::object()) {
  payload["service"] = "sekailink_room_server";
  payload["event"] = event;
  payload["time"] = utc_timestamp_now();
  stream << payload.dump() << "\n";
  stream.flush();
}

bool auth_enabled(const std::optional<std::string>& token) {
  return token.has_value() && !token->empty();
}

nlohmann::json auth_summary_json(const RoomServerAuthPolicy& auth_policy) {
  return {
      {"admin_auth_enabled", auth_enabled(auth_policy.admin_token)},
      {"runtime_auth_enabled", auth_enabled(auth_policy.runtime_token)},
      {"client_report_auth_enabled", auth_enabled(auth_policy.client_report_token)},
  };
}

nlohmann::json runtime_surface_json(
    std::uint16_t tcp_port,
    std::uint16_t http_port,
    std::uint64_t room_count) {
  return {
      {"loopback_only", true},
      {"effective_tcp_host", kLoopbackBindHost},
      {"effective_tcp_port", tcp_port},
      {"effective_http_host", kLoopbackBindHost},
      {"effective_http_port", http_port},
      {"room_count", room_count},
  };
}

}  // namespace

int main(int argc, char** argv) {
  RoomServerNodeConfig config;
  config.http_port = 18081;
  std::filesystem::path state_file;
  std::optional<std::string> started_at;
  std::uint64_t last_room_count = 0;

  try {
    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--config" && i + 1 < argc) {
        config = load_room_server_node_config(argv[++i]);
        if (config.http_port == 0) {
          config.http_port = 18081;
        }
      } else if (arg == "--tcp-port" && i + 1 < argc) {
        config.tcp_port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
      } else if (arg == "--http-port" && i + 1 < argc) {
        config.http_port = static_cast<std::uint16_t>(std::stoi(argv[++i]));
      } else if (arg == "--audit-root" && i + 1 < argc) {
        config.audit_root = argv[++i];
      } else if (arg == "--projection-root" && i + 1 < argc) {
        config.projection_root = argv[++i];
      } else if (arg == "--projection-backend" && i + 1 < argc) {
        config.projection_backend = parse_projection_backend(argv[++i]);
      } else if (arg == "--restore-from-audit") {
        config.restore_from_audit = true;
      } else if (arg == "--restore-from-projection") {
        config.restore_from_projection = true;
      } else if (arg == "--purge-expired-periodically") {
        config.purge_expired_periodically = true;
      } else if (arg == "--purge-interval-ms" && i + 1 < argc) {
        config.purge_interval_ms = static_cast<std::uint32_t>(std::stoul(argv[++i]));
      } else if (arg == "--state-file" && i + 1 < argc) {
        state_file = argv[++i];
      }
    }

    apply_env_auth_overrides(config);

    emit_service_log(
        std::cout,
        "room_server_service_bootstrap",
        {
            {"config", to_json(config)},
            {"auth", auth_summary_json(config.auth_policy)},
            {"requested_runtime_surface",
             runtime_surface_json(
                 config.tcp_port,
                 config.http_port,
                 0)},
            {"state_file", state_file.empty() ? nlohmann::json(nullptr) : nlohmann::json(state_file.string())},
        });

#ifndef _WIN32
    std::signal(SIGINT, handle_signal);
    std::signal(SIGTERM, handle_signal);
#endif

    RoomServerNode node(config);
    if (!node.start()) {
      if (!state_file.empty()) {
        write_room_server_state_file(
            state_file,
            config,
            {
                .ok = false,
                .status = "failed",
                .updated_at = utc_timestamp_now(),
                .loopback_only = true,
                .admin_auth_enabled = auth_enabled(config.auth_policy.admin_token),
                .runtime_auth_enabled = auth_enabled(config.auth_policy.runtime_token),
                .client_report_auth_enabled = auth_enabled(config.auth_policy.client_report_token),
                .last_error = std::string("room_server_node_start_failed"),
            });
      }
      emit_service_log(
          std::cerr,
          "room_server_service_start_failed",
          {
              {"auth", auth_summary_json(config.auth_policy)},
              {"error", "room_server_node_start_failed"},
              {"runtime_surface", runtime_surface_json(0, 0, 0)},
          });
      return EXIT_FAILURE;
    }

    started_at = utc_timestamp_now();
    const auto boot_room_count = static_cast<std::uint64_t>(node.registry().list_room_ids().size());
    last_room_count = boot_room_count;
    if (!state_file.empty()) {
      write_room_server_state_file(
          state_file,
          config,
          {
              .ok = true,
              .status = "running",
              .started_at = started_at,
              .updated_at = *started_at,
              .effective_tcp_host = kLoopbackBindHost,
              .effective_tcp_port = node.tcp_port(),
              .effective_http_host = kLoopbackBindHost,
              .effective_http_port = node.http_port(),
              .boot_room_count = boot_room_count,
              .room_count = boot_room_count,
              .stop_requested = false,
              .loopback_only = true,
              .admin_auth_enabled = auth_enabled(config.auth_policy.admin_token),
              .runtime_auth_enabled = auth_enabled(config.auth_policy.runtime_token),
              .client_report_auth_enabled = auth_enabled(config.auth_policy.client_report_token),
          });
    }

    emit_service_log(
        std::cout,
        "room_server_service_started",
        {
            {"boot_room_count", boot_room_count},
            {"auth", auth_summary_json(config.auth_policy)},
            {"runtime_surface", runtime_surface_json(node.tcp_port(), node.http_port(), boot_room_count)},
            {"state_file", state_file.empty() ? nlohmann::json(nullptr) : nlohmann::json(state_file.string())},
        });

    std::cout << "room_server_service_started tcp_port=" << node.tcp_port()
              << " http_port=" << node.http_port() << "\n";
    std::cout.flush();

    while (!g_stop_requested) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    const auto stop_time = utc_timestamp_now();
    const auto stopping_room_count = static_cast<std::uint64_t>(node.registry().list_room_ids().size());
    last_room_count = stopping_room_count;
    if (!state_file.empty()) {
      write_room_server_state_file(
          state_file,
          config,
          {
              .ok = true,
              .status = "stopping",
              .started_at = started_at,
              .updated_at = stop_time,
              .effective_tcp_host = kLoopbackBindHost,
              .effective_tcp_port = node.tcp_port(),
              .effective_http_host = kLoopbackBindHost,
              .effective_http_port = node.http_port(),
              .boot_room_count = boot_room_count,
              .room_count = stopping_room_count,
              .stop_requested = true,
              .loopback_only = true,
              .admin_auth_enabled = auth_enabled(config.auth_policy.admin_token),
              .runtime_auth_enabled = auth_enabled(config.auth_policy.runtime_token),
              .client_report_auth_enabled = auth_enabled(config.auth_policy.client_report_token),
          });
    }

    emit_service_log(
        std::cout,
        "room_server_service_stopping",
        {
            {"auth", auth_summary_json(config.auth_policy)},
            {"runtime_surface", runtime_surface_json(node.tcp_port(), node.http_port(), stopping_room_count)},
        });

    node.stop();
    const auto stopped_room_count = static_cast<std::uint64_t>(node.registry().list_room_ids().size());
    last_room_count = stopped_room_count;

    if (!state_file.empty()) {
      write_room_server_state_file(
          state_file,
          config,
          {
              .ok = true,
              .status = "stopped",
              .started_at = started_at,
              .updated_at = utc_timestamp_now(),
              .boot_room_count = boot_room_count,
              .room_count = stopped_room_count,
              .stop_requested = true,
              .loopback_only = true,
              .admin_auth_enabled = auth_enabled(config.auth_policy.admin_token),
              .runtime_auth_enabled = auth_enabled(config.auth_policy.runtime_token),
              .client_report_auth_enabled = auth_enabled(config.auth_policy.client_report_token),
          });
    }

    emit_service_log(
        std::cout,
        "room_server_service_stopped",
        {
            {"boot_room_count", boot_room_count},
            {"auth", auth_summary_json(config.auth_policy)},
            {"runtime_surface", runtime_surface_json(0, 0, stopped_room_count)},
        });
    return EXIT_SUCCESS;
  } catch (const std::exception& exception) {
    if (!state_file.empty()) {
      try {
        write_room_server_state_file(
            state_file,
            config,
            {
                .ok = false,
                .status = "failed",
                .started_at = started_at,
                .updated_at = utc_timestamp_now(),
                .room_count = last_room_count,
                .loopback_only = true,
                .admin_auth_enabled = auth_enabled(config.auth_policy.admin_token),
                .runtime_auth_enabled = auth_enabled(config.auth_policy.runtime_token),
                .client_report_auth_enabled = auth_enabled(config.auth_policy.client_report_token),
                .last_error = std::string(exception.what()),
            });
      } catch (...) {
      }
    }
    emit_service_log(
        std::cerr,
        "room_server_service_failed",
        {
            {"auth", auth_summary_json(config.auth_policy)},
            {"error", exception.what()},
            {"last_known_room_count", last_room_count},
        });
    return EXIT_FAILURE;
  }
}
