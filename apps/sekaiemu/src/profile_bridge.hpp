#pragma once

#include "bridge_ipc.hpp"
#include "memory_profile.hpp"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace sekaiemu::spike {

struct ProfileBridgeCallbacks {
  std::function<std::vector<std::uint8_t>(const WatchRegion&)> read_region;
  std::function<std::uint8_t*(std::string_view, std::uint32_t, std::uint32_t)> resolve_mutable;
  std::function<void(const std::string&)> trace;
  std::function<void(std::uint32_t, const std::string&)> location_triggered;
};

class ProfileBridge {
 public:
  bool Initialize(const std::filesystem::path& profile_path,
                  const std::filesystem::path& save_directory,
                  std::string& error);

  void Tick(const ProfileBridgeCallbacks& callbacks);

  bool Active() const;
  const MemoryProfile* Profile() const;
  const std::filesystem::path& SocketPath() const;

 private:
  std::vector<std::uint8_t> ReadProfileRegion(const ProfileBridgeCallbacks& callbacks,
                                              const WatchRegion& region) const;
  const ReceiveSlot* FindReceiveSlot(std::string_view kind) const;
 const ItemAlias* FindItemAlias(std::string_view kind, std::string_view alias_name) const;
  bool ReadSlotBusy(const ProfileBridgeCallbacks& callbacks, const ReceiveSlot& slot) const;
  bool WriteSlotValue(const ProfileBridgeCallbacks& callbacks,
                      const ReceiveSlot& slot,
                      std::uint32_t value) const;
  std::optional<std::uint32_t> ReadSlotValue(const ProfileBridgeCallbacks& callbacks,
                                             const ReceiveSlot& slot) const;
  std::optional<std::uint32_t> ResolveInjectionCode(std::string_view kind,
                                                    std::string_view raw_value) const;
  void ProcessIncomingCommands(const ProfileBridgeCallbacks& callbacks);
  void AppendBridgeLog(const std::string& line);

  std::optional<MemoryProfile> profile_;
  BridgeIpc bridge_ipc_;
  std::vector<std::uint8_t> watched_region_snapshot_;
  std::vector<bool> flag_check_snapshot_;
  std::vector<bool> oot_check_snapshot_;
};

}  // namespace sekaiemu::spike
