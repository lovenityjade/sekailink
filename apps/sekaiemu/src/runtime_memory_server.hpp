#pragma once

#include "libretro_core_api.hpp"
#include "memory_domain_registry.hpp"

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
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
                  std::string& error);
  void Shutdown();
  void Poll();

  const std::filesystem::path& SocketPath() const { return socket_path_; }
  bool Active() const { return server_fd_ >= 0; }

 private:
  std::vector<RuntimeMemoryDomainInfo> BuildDomainList() const;
  std::string HandleRequestLine(const std::string& line);
  std::string HandleJsonRequest(const std::string& line);
  std::string BuildSystemResponse() const;
  std::string BuildDomainsResponse() const;
  std::string BuildReadResponse(const std::string& line) const;
  std::string BuildWriteResponse(const std::string& line);

  const std::uint8_t* ResolveReadOnly(std::string_view domain_id,
                                      std::uint32_t address,
                                      std::uint32_t size) const;
  std::uint8_t* ResolveWritable(std::string_view domain_id,
                                std::uint32_t address,
                                std::uint32_t size) const;

  std::intptr_t server_fd_ = -1;
  std::vector<std::intptr_t> client_fds_;
  std::filesystem::path socket_path_;
  std::string system_name_ = "LIBRETRO";
  CoreApi* core_ = nullptr;
  MemoryDomainRegistry* memory_domains_ = nullptr;
};

std::string InferRuntimeSystemName(const std::filesystem::path& core_path);

}  // namespace sekaiemu::spike
