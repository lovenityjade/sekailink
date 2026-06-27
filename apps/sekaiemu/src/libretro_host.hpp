#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace sekaiemu::spike {

struct HostOptions {
  std::filesystem::path core_path;
  std::filesystem::path game_path;
  std::filesystem::path system_directory;
  std::filesystem::path save_directory;
  std::filesystem::path log_directory;
  std::filesystem::path profile_path;
  std::filesystem::path memory_socket_path;
  std::filesystem::path sklmi_runtime_path;
  std::filesystem::path sklmi_manifest_directory;
  std::string ap_host;
  std::uint16_t ap_port = 0;
  std::string ap_path = "/";
  std::string ap_game;
  std::string ap_slot_name;
  std::string player_alias;
  std::string ap_password;
  std::string ap_uuid = "sekailink-sekaiemu";
  std::string ap_tags = "AP,SekaiLink,SKLMI";
  std::filesystem::path tracker_pack_path;
  std::string tracker_variant;
  std::filesystem::path tracker_snapshot_path;
  std::filesystem::path tracker_command_log_path;
  std::filesystem::path tracker_assets_root;
  std::filesystem::path tracker_bundle_path;
  std::filesystem::path tracker_state_path;
  bool tracker_required = false;
  std::filesystem::path chat_inbox_path;
  std::filesystem::path chat_outbox_path;
  std::filesystem::path input_script_path;
  std::filesystem::path dump_frame_path;
  std::uint64_t save_state_at_frame = 0;
  std::uint64_t quit_after_frame = 0;
  std::uint64_t dump_frame_at_frame = 0;
  bool input_script_quit_after_end = false;
  bool load_state_on_start = false;
  bool probe_only = false;
};

class LibretroHost {
 public:
  explicit LibretroHost(HostOptions options);
  ~LibretroHost();

  LibretroHost(const LibretroHost&) = delete;
  LibretroHost& operator=(const LibretroHost&) = delete;

  bool Initialize();
  int Run();

  std::string LastError() const;

 private:
  struct Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace sekaiemu::spike
