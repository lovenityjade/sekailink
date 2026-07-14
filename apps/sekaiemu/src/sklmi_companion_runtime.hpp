#pragma once

#include "sklmi_bridge_registry.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace sekaiemu::spike {

struct SklmiCompanionOptions {
  std::filesystem::path runtime_binary_path;
  std::filesystem::path manifest_directory;
  std::filesystem::path save_directory;
  std::filesystem::path memory_socket_path;
  SklmiBridgeSpec bridge_spec;
  std::string ap_host;
  std::uint16_t ap_port = 0;
  std::string ap_path = "/";
  std::string ap_game;
  std::string ap_slot_name;
  std::string player_alias;
  std::string player_alias_map_json;
  std::string ap_password;
  std::string ap_uuid;
  std::string ap_tags;
  std::filesystem::path tracker_pack_path;
  std::string tracker_variant;
  std::filesystem::path tracker_snapshot_path;
  std::filesystem::path tracker_command_log_path;
  std::filesystem::path tracker_assets_root;
};

class SklmiCompanionRuntime {
 public:
  SklmiCompanionRuntime() = default;
  ~SklmiCompanionRuntime();

  SklmiCompanionRuntime(const SklmiCompanionRuntime&) = delete;
  SklmiCompanionRuntime& operator=(const SklmiCompanionRuntime&) = delete;

  bool Start(const SklmiCompanionOptions& options, std::string& error);
  void Tick(bool& running, int& exit_code, std::string& last_error);
  void Tick(std::string& last_error);
  void Shutdown();

  bool Active() const;
  std::string BridgeId() const;
  const std::filesystem::path& RoomStatePath() const;
  const std::filesystem::path& RuntimeStateRoot() const;
  const std::filesystem::path& TraceLogPath() const;
  const std::filesystem::path& ManifestPath() const;
  const std::filesystem::path& CompanionLogPath() const;
  const std::string& LastExitDetail() const;

 private:
  bool started_ = false;
  std::string bridge_id_;
  std::filesystem::path manifest_path_;
  std::filesystem::path room_state_path_;
  std::filesystem::path runtime_state_root_;
  std::filesystem::path trace_log_path_;
  std::filesystem::path companion_log_path_;
  std::string last_exit_detail_;

#ifdef _WIN32
  void* child_process_handle_ = nullptr;
  unsigned long child_process_id_ = 0;
#else
  int child_pid_ = -1;
#endif
};

}  // namespace sekaiemu::spike
