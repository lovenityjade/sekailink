#pragma once

#include "runtime_memory_server.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace sekaiemu::spike {

inline constexpr std::uint32_t kGbaEwramBase = 0x02000000u;
inline constexpr std::uint32_t kGbaEwramSize = 0x00040000u;
inline constexpr std::uint32_t kGbaIwramBase = 0x03000000u;
inline constexpr std::uint32_t kGbaIwramSize = 0x00008000u;
inline constexpr std::uint32_t kGbaSramBase = 0x0E000000u;
inline constexpr std::uint32_t kGbaSramMirrorWindowSize = 0x00020000u;
inline constexpr std::uint32_t kGbaCombinedRamSize = kGbaEwramSize + kGbaIwramSize;

std::string EscapeJson(std::string_view value);
std::optional<std::uint64_t> ParseUnsigned(std::string_view text);
std::string LowercaseCopy(std::string value);
std::string NormalizeDomainName(std::string value);
bool IsGbaSystem(std::string_view system_name);
bool IsGameBoySystem(std::string_view system_name);
bool IsRomDomain(std::string_view normalized_domain);
bool IsGameBoyCartRamDomain(std::string_view normalized_domain);
bool IsGameBoyBusDomain(std::string_view normalized_domain);
std::optional<std::uint16_t> GameBoyRomOffsetToCpuAddress(std::size_t offset);
std::string EncodeGameBoyGameGeniePatch(std::uint16_t cpu_address,
                                        std::uint8_t value,
                                        std::uint8_t compare);
bool IsGbaEwramDomain(std::string_view normalized_domain);
bool IsGbaIwramDomain(std::string_view normalized_domain);
bool IsGbaSramDomain(std::string_view normalized_domain);
bool IsGbaBusDomain(std::string_view normalized_domain);
std::optional<std::uint32_t> ResolveGbaSramOffset(std::uint32_t address,
                                                  std::uint32_t size,
                                                  std::uint32_t save_ram_size);
std::optional<std::uint32_t> GbaRelativeDomainToBusAddress(std::string_view normalized_domain,
                                                           std::uint32_t address,
                                                           std::uint32_t size);
std::optional<std::string> ExtractStringField(std::string_view text, std::string_view key);
std::optional<std::uint64_t> ExtractUIntField(std::string_view text, std::string_view key);
std::optional<double> ExtractDoubleField(std::string_view text, std::string_view key);
std::string JsonError(std::string_view err);
std::string DomainListToJson(const std::vector<RuntimeMemoryDomainInfo>& domains);
std::optional<std::vector<std::byte>> Base64Decode(std::string_view input);
std::string Base64Encode(const std::byte* data, std::size_t size);
std::vector<std::uint8_t> ReadBinaryFile(const std::filesystem::path& path);
const std::uint8_t* ResolveGbaLibretroMemory(const CoreApi& core,
                                             std::string_view normalized_domain,
                                             std::uint32_t address,
                                             std::uint32_t size);
std::uint8_t* ResolveGbaLibretroMemoryMutable(CoreApi& core,
                                              std::string_view normalized_domain,
                                              std::uint32_t address,
                                              std::uint32_t size);

}  // namespace sekaiemu::spike
