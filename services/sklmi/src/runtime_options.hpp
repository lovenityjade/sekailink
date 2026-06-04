#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace sekailink::sklmi {

struct RuntimeOptions {
    std::filesystem::path memory_socket_path;
    std::filesystem::path bridge_manifest_path;
    std::filesystem::path room_state_path;
    std::filesystem::path runtime_state_root;
    std::filesystem::path trace_log_path;
    std::filesystem::path tracker_pack_path;
    std::filesystem::path tracker_snapshot_path;
    std::filesystem::path tracker_command_log_path;
    std::filesystem::path tracker_assets_root;
    std::string driver_instance_id;
    std::string mode = "offline";
    std::string tracker_variant;
    std::string room_host = "127.0.0.1";
    std::uint16_t room_port = 0;
    std::uint16_t room_control_port = 0;
    std::uint16_t room_runtime_port = 0;
    std::string room_session_name;
    int room_slot_id = 0;
    std::string room_control_channel = "core";
    std::string room_control_auth_token;
    std::string room_runtime_auth_token;
    std::string room_runtime_session_token;
    std::string ap_host;
    std::uint16_t ap_port = 0;
    std::string ap_path = "/";
    std::string ap_game;
    std::string ap_slot_name;
    std::string player_alias;
    std::string ap_password;
    std::string ap_uuid = "sekailink-sklmi";
    std::vector<std::string> ap_tags = {"AP", "SekaiLink", "SKLMI"};
    std::uint64_t tick_ms = 16;
    std::optional<std::uint64_t> max_ticks;
};

struct RuntimeParseResult {
    bool ok = false;
    bool show_help = false;
    RuntimeOptions options;
    std::string error;
};

RuntimeParseResult ParseRuntimeOptions(const std::vector<std::string>& args);
std::string RuntimeUsage();

}  // namespace sekailink::sklmi
