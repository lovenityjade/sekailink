#pragma once

#include "libretro_core_api.hpp"
#include "memory_domain_registry.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace sekaiemu::spike {

struct RuntimeMemoryDomainInfo {
  std::string id;
  std::string display_name;
  std::size_t size_bytes = 0;
  bool writable = false;
  std::string endianness = "little";
};

class RuntimeMemoryServer {
 public:
  RuntimeMemoryServer() = default;
  ~RuntimeMemoryServer();

  RuntimeMemoryServer(const RuntimeMemoryServer&) = delete;
  RuntimeMemoryServer& operator=(const RuntimeMemoryServer&) = delete;

  bool Initialize(const std::filesystem::path& save_directory,
                  std::optional<std::filesystem::path> override_socket_path,
                  std::string system_name,
                  CoreApi* core,
                  MemoryDomainRegistry* memory_domains,
                  std::string& error,
                  std::optional<std::filesystem::path> rom_path = std::nullopt);
  void Shutdown();
  void Poll();

  const std::filesystem::path& SocketPath() const { return socket_path_; }
  bool Active() const { return server_fd_ >= 0; }
  bool Locked() const { return locked_; }

 private:
  std::vector<RuntimeMemoryDomainInfo> BuildDomainList() const;
  std::string HandleRequestLine(const std::string& line);
  std::string HandleJsonRequest(const std::string& line);
  std::string BuildSystemResponse() const;
  std::string BuildPreferredCoresResponse() const;
  std::string BuildHashResponse() const;
  std::string BuildMemorySizeResponse(const std::string& line) const;
  std::string BuildDomainsResponse() const;
  std::string BuildGuardResponse(const std::string& line) const;
  std::string BuildReadResponse(const std::string& line) const;
  std::string BuildWriteResponse(const std::string& line);
  std::string BuildDisplayMessageResponse();
  std::string BuildSetMessageIntervalResponse(const std::string& line);

  const std::uint8_t* ResolveReadOnly(std::string_view domain_id,
                                      std::uint32_t address,
                                      std::uint32_t size) const;
  std::uint8_t* ResolveWritable(std::string_view domain_id,
                                std::uint32_t address,
                                std::uint32_t size) const;
  bool ApplyGameBoyRomWrite(std::string_view normalized_domain,
                            std::uint32_t address,
                            const std::vector<std::byte>& bytes);
  void TraceMemoryRequest(std::string_view type,
                          std::string_view domain,
                          std::uint32_t address,
                          std::uint32_t size,
                          bool ok,
                          std::string_view detail) const;

  std::intptr_t server_fd_ = -1;
  std::vector<std::intptr_t> client_fds_;
  std::unordered_map<std::intptr_t, std::string> client_buffers_;
  std::filesystem::path socket_path_;
  std::string system_name_ = "LIBRETRO";
  std::vector<std::uint8_t> rom_data_;
  std::size_t prg_rom_offset_ = 0;
  std::unordered_map<std::size_t, unsigned> game_boy_rom_patch_indices_;
  unsigned next_game_boy_rom_patch_index_ = 0;
  CoreApi* core_ = nullptr;
  MemoryDomainRegistry* memory_domains_ = nullptr;
  bool locked_ = false;
  double message_interval_seconds_ = 0.0;
  mutable std::uint64_t read_trace_count_ = 0;
  mutable std::uint64_t write_trace_count_ = 0;
  mutable std::uint64_t guard_trace_count_ = 0;
};

std::string InferRuntimeSystemName(const std::filesystem::path& core_path,
                                   const std::filesystem::path& game_path = {});

}  // namespace sekaiemu::spike
