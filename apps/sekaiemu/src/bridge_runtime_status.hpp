#pragma once

#include <cstdint>
#include <string>

namespace sekaiemu::spike {

enum class BridgeOwner {
  Legacy,
  Sklmi,
};

struct BridgeRuntimeStatus {
  BridgeOwner owner = BridgeOwner::Legacy;
  bool migrated_game = false;
  bool sklmi_active = false;
  bool legacy_bridge_active = false;
  std::string game_name;
  std::string bridge_id;
  std::string ap_host;
  std::uint16_t ap_port = 0;
  std::string ap_path;
  std::string ap_slot_name;
  std::string runtime_memory_socket_path;
  std::string sekaiemu_log_path;
  std::string legacy_bridge_socket_path;
  std::string manifest_path;
  std::string room_state_path;
  std::string tracker_snapshot_path;
  std::string tracker_command_log_path;
  std::string runtime_state_root;
  std::string trace_log_path;
  std::string companion_log_path;
  std::string last_error;
};

}  // namespace sekaiemu::spike
