#include "runtime_memory_utils.hpp"

#include <algorithm>
#include <charconv>
#include <cctype>
#include <fstream>
#include <sstream>

namespace sekaiemu::spike {

namespace {

char HexNibble(unsigned value) {
  constexpr char kHex[] = "0123456789ABCDEF";
  return kHex[value & 0x0Fu];
}

}  // namespace

std::string EscapeJson(std::string_view value) {
  std::string out;
  out.reserve(value.size() + 8);
  for (const char ch : value) {
    switch (ch) {
      case '\\': out += "\\\\"; break;
      case '"': out += "\\\""; break;
      case '\n': out += "\\n"; break;
      case '\r': out += "\\r"; break;
      case '\t': out += "\\t"; break;
      default: out.push_back(ch); break;
    }
  }
  return out;
}

std::optional<std::uint64_t> ParseUnsigned(std::string_view text) {
  auto trimmed = std::string(text);
  auto not_space = [](unsigned char c) { return !std::isspace(c); };
  trimmed.erase(trimmed.begin(), std::find_if(trimmed.begin(), trimmed.end(), not_space));
  trimmed.erase(std::find_if(trimmed.rbegin(), trimmed.rend(), not_space).base(), trimmed.end());
  if (trimmed.empty()) {
    return std::nullopt;
  }
  std::uint64_t value = 0;
  auto begin = trimmed.data();
  auto end = trimmed.data() + trimmed.size();
  const bool hex = trimmed.size() > 2 && trimmed[0] == '0' && (trimmed[1] == 'x' || trimmed[1] == 'X');
  if (hex) {
    begin += 2;
  }
  const auto result = std::from_chars(begin, end, value, hex ? 16 : 10);
  if (result.ec != std::errc{} || result.ptr != end) {
    return std::nullopt;
  }
  return value;
}

std::string LowercaseCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::string NormalizeDomainName(std::string value) {
  value = LowercaseCopy(std::move(value));
  for (char& ch : value) {
    if (ch == ' ' || ch == '-' || ch == '.') {
      ch = '_';
    }
  }
  return value;
}

bool IsGbaSystem(std::string_view system_name) {
  return NormalizeDomainName(std::string(system_name)) == "gba";
}

bool IsGameBoySystem(std::string_view system_name) {
  const auto normalized = NormalizeDomainName(std::string(system_name));
  return normalized == "gb" || normalized == "gbc" || normalized == "sgb";
}

bool IsRomDomain(std::string_view normalized_domain) {
  return normalized_domain == "rom" || normalized_domain == "cart_rom" ||
         normalized_domain == "cartridge_rom" || normalized_domain == "prg_rom";
}

bool IsGameBoyCartRamDomain(std::string_view normalized_domain) {
  return normalized_domain == "cart_ram" || normalized_domain == "cartram" ||
         normalized_domain == "save_ram" || normalized_domain == "battery_ram" ||
         normalized_domain == "sram";
}

bool IsGameBoyBusDomain(std::string_view normalized_domain) {
  return normalized_domain == "system_bus" || normalized_domain == "bus" ||
         normalized_domain == "gb_system_bus" || normalized_domain == "gbc_system_bus";
}

std::optional<std::uint16_t> GameBoyRomOffsetToCpuAddress(std::size_t offset) {
  if (offset < 0x4000u) {
    return static_cast<std::uint16_t>(offset);
  }
  const auto bank_offset = offset % 0x4000u;
  const auto cpu_address = 0x4000u + bank_offset;
  if (cpu_address >= 0x8000u) {
    return std::nullopt;
  }
  return static_cast<std::uint16_t>(cpu_address);
}

std::string EncodeGameBoyGameGeniePatch(std::uint16_t cpu_address,
                                        std::uint8_t value,
                                        std::uint8_t compare) {
  const auto compare_rot = static_cast<std::uint8_t>(((compare ^ 0x45u) << 2u) |
                                                    ((compare ^ 0x45u) >> 6u));
  const auto compare_code = static_cast<std::uint8_t>(compare_rot ^ 0xFFu);
  std::string code = "000-000-000";
  code[0] = HexNibble(value >> 4u);
  code[1] = HexNibble(value);
  code[2] = HexNibble(cpu_address >> 8u);
  code[4] = HexNibble(cpu_address >> 4u);
  code[5] = HexNibble(cpu_address);
  code[6] = HexNibble((cpu_address >> 12u) ^ 0x0Fu);
  code[8] = HexNibble(compare_code >> 4u);
  code[10] = HexNibble(compare_code);
  return code;
}

bool IsGbaEwramDomain(std::string_view normalized_domain) {
  return normalized_domain == "ewram" || normalized_domain == "external_work_ram";
}

bool IsGbaIwramDomain(std::string_view normalized_domain) {
  return normalized_domain == "iwram" || normalized_domain == "internal_work_ram";
}

bool IsGbaSramDomain(std::string_view normalized_domain) {
  return normalized_domain == "sram" || normalized_domain == "save_ram" ||
         normalized_domain == "battery_ram";
}

bool IsGbaBusDomain(std::string_view normalized_domain) {
  return normalized_domain == "system_bus" || normalized_domain == "bus" ||
         normalized_domain == "gba_system_bus";
}

std::optional<std::uint32_t> ResolveGbaSramOffset(std::uint32_t address,
                                                  std::uint32_t size,
                                                  std::uint32_t save_ram_size) {
  if (size == 0 || save_ram_size == 0 || address >= kGbaSramMirrorWindowSize) {
    return std::nullopt;
  }
  const auto mirrored = address % save_ram_size;
  if (mirrored > save_ram_size || size > save_ram_size - mirrored) {
    return std::nullopt;
  }
  return mirrored;
}

std::optional<std::uint32_t> GbaRelativeDomainToBusAddress(std::string_view normalized_domain,
                                                           std::uint32_t address,
                                                           std::uint32_t size) {
  auto in_range = [](std::uint32_t start, std::uint32_t length, std::uint32_t region_size) {
    return start <= region_size && length <= region_size - start;
  };
  if (IsGbaEwramDomain(normalized_domain) && in_range(address, size, kGbaEwramSize)) {
    return kGbaEwramBase + address;
  }
  if (IsGbaIwramDomain(normalized_domain) && in_range(address, size, kGbaIwramSize)) {
    return kGbaIwramBase + address;
  }
  if (IsGbaSramDomain(normalized_domain)) {
    return kGbaSramBase + address;
  }
  return std::nullopt;
}

std::optional<std::string> ExtractStringField(std::string_view text, std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\":\"";
  const auto begin = text.find(needle);
  if (begin == std::string_view::npos) {
    return std::nullopt;
  }
  const auto start = begin + needle.size();
  const auto end = text.find('"', start);
  if (end == std::string_view::npos) {
    return std::nullopt;
  }
  return std::string(text.substr(start, end - start));
}

std::optional<std::uint64_t> ExtractUIntField(std::string_view text, std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\":";
  const auto begin = text.find(needle);
  if (begin == std::string_view::npos) {
    return std::nullopt;
  }
  const auto start = begin + needle.size();
  auto end = text.find_first_of(",}]", start);
  if (end == std::string_view::npos) {
    end = text.size();
  }
  return ParseUnsigned(text.substr(start, end - start));
}

std::optional<double> ExtractDoubleField(std::string_view text, std::string_view key) {
  const std::string needle = "\"" + std::string(key) + "\":";
  const auto begin = text.find(needle);
  if (begin == std::string_view::npos) {
    return std::nullopt;
  }
  const auto start = begin + needle.size();
  auto end = text.find_first_of(",}]", start);
  if (end == std::string_view::npos) {
    end = text.size();
  }
  std::string raw(text.substr(start, end - start));
  raw.erase(raw.begin(), std::find_if(raw.begin(), raw.end(), [](unsigned char c) {
    return !std::isspace(c) && c != '"';
  }));
  raw.erase(std::find_if(raw.rbegin(), raw.rend(), [](unsigned char c) {
    return !std::isspace(c) && c != '"';
  }).base(), raw.end());
  if (raw.empty()) {
    return std::nullopt;
  }
  try {
    return std::stod(raw);
  } catch (...) {
    return std::nullopt;
  }
}

std::string JsonError(std::string_view err) {
  return "{\"type\":\"ERROR\",\"err\":\"" + EscapeJson(err) + "\"}";
}

std::string DomainListToJson(const std::vector<RuntimeMemoryDomainInfo>& domains) {
  std::ostringstream json;
  json << "[";
  for (std::size_t i = 0; i < domains.size(); ++i) {
    if (i > 0) json << ",";
    const auto& domain = domains[i];
    json << "{\"name\":\"" << EscapeJson(domain.id)
         << "\",\"size\":" << domain.size_bytes
         << ",\"writable\":" << (domain.writable ? "true" : "false")
         << ",\"endianness\":\"" << EscapeJson(domain.endianness)
         << "\"}";
  }
  json << "]";
  return json.str();
}

std::optional<std::vector<std::byte>> Base64Decode(std::string_view input) {
  static constexpr std::string_view kAlphabet =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  auto decode_char = [](char c) -> std::optional<unsigned> {
    const auto pos = kAlphabet.find(c);
    if (pos == std::string_view::npos) return std::nullopt;
    return static_cast<unsigned>(pos);
  };

  if (input.size() % 4 != 0) {
    return std::nullopt;
  }

  std::vector<std::byte> out;
  out.reserve((input.size() / 4) * 3);
  for (std::size_t i = 0; i < input.size(); i += 4) {
    const auto d0 = decode_char(input[i]);
    const auto d1 = decode_char(input[i + 1]);
    if (!d0.has_value() || !d1.has_value()) {
      return std::nullopt;
    }
    const auto d2 = input[i + 2] == '=' ? std::optional<unsigned>{0} : decode_char(input[i + 2]);
    const auto d3 = input[i + 3] == '=' ? std::optional<unsigned>{0} : decode_char(input[i + 3]);
    if (!d2.has_value() || !d3.has_value()) {
      return std::nullopt;
    }
    const auto combined = (*d0 << 18U) | (*d1 << 12U) | (*d2 << 6U) | *d3;
    out.push_back(static_cast<std::byte>((combined >> 16U) & 0xFFU));
    if (input[i + 2] != '=') out.push_back(static_cast<std::byte>((combined >> 8U) & 0xFFU));
    if (input[i + 3] != '=') out.push_back(static_cast<std::byte>(combined & 0xFFU));
  }
  return out;
}

std::string Base64Encode(const std::byte* data, std::size_t size) {
  static constexpr char kAlphabet[] =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve(((size + 2) / 3) * 4);
  for (std::size_t i = 0; i < size; i += 3) {
    const auto b0 = static_cast<unsigned>(std::to_integer<unsigned char>(data[i]));
    const auto b1 = (i + 1 < size) ? static_cast<unsigned>(std::to_integer<unsigned char>(data[i + 1])) : 0U;
    const auto b2 = (i + 2 < size) ? static_cast<unsigned>(std::to_integer<unsigned char>(data[i + 2])) : 0U;
    const auto combined = (b0 << 16U) | (b1 << 8U) | b2;
    out.push_back(kAlphabet[(combined >> 18U) & 0x3FU]);
    out.push_back(kAlphabet[(combined >> 12U) & 0x3FU]);
    out.push_back(i + 1 < size ? kAlphabet[(combined >> 6U) & 0x3FU] : '=');
    out.push_back(i + 2 < size ? kAlphabet[combined & 0x3FU] : '=');
  }
  return out;
}

std::vector<std::uint8_t> ReadBinaryFile(const std::filesystem::path& path) {
  std::ifstream stream(path, std::ios::binary);
  if (!stream) {
    return {};
  }
  stream.seekg(0, std::ios::end);
  const auto size = stream.tellg();
  if (size <= 0) {
    return {};
  }
  std::vector<std::uint8_t> data(static_cast<std::size_t>(size));
  stream.seekg(0, std::ios::beg);
  stream.read(reinterpret_cast<char*>(data.data()), static_cast<std::streamsize>(data.size()));
  if (!stream) {
    return {};
  }
  return data;
}

const std::uint8_t* ResolveGbaLibretroMemory(const CoreApi& core,
                                             std::string_view normalized_domain,
                                             std::uint32_t address,
                                             std::uint32_t size) {
  if (!core.retro_get_memory_data || !core.retro_get_memory_size || size == 0) {
    return nullptr;
  }

  auto in_range = [](std::uint32_t start, std::uint32_t length, std::uint32_t region_size) {
    return start <= region_size && length <= region_size - start;
  };

  auto* save_base = static_cast<const std::uint8_t*>(core.retro_get_memory_data(RETRO_MEMORY_SAVE_RAM));
  const std::size_t save_ram_size = core.retro_get_memory_size(RETRO_MEMORY_SAVE_RAM);
  const auto save_ram_size32 = static_cast<std::uint32_t>(
      std::min<std::size_t>(save_ram_size, 0xFFFFFFFFull));

  if (IsGbaSramDomain(normalized_domain)) {
    if (!save_base) {
      return nullptr;
    }
    if (in_range(address, size, save_ram_size32)) {
      return save_base + address;
    }
    if (const auto mirrored = ResolveGbaSramOffset(address, size, save_ram_size32)) {
      return save_base + *mirrored;
    }
    return nullptr;
  }

  if (IsGbaBusDomain(normalized_domain) && address >= kGbaSramBase) {
    const auto offset = address - kGbaSramBase;
    if (!save_base) {
      return nullptr;
    }
    if (in_range(offset, size, save_ram_size32)) {
      return save_base + offset;
    }
    if (const auto mirrored = ResolveGbaSramOffset(offset, size, save_ram_size32)) {
      return save_base + *mirrored;
    }
  }

  auto* base = static_cast<const std::uint8_t*>(core.retro_get_memory_data(RETRO_MEMORY_SYSTEM_RAM));
  const std::size_t system_ram_size = core.retro_get_memory_size(RETRO_MEMORY_SYSTEM_RAM);
  if (!base || system_ram_size == 0) {
    return nullptr;
  }

  if (IsGbaEwramDomain(normalized_domain)) {
    return system_ram_size >= kGbaEwramSize && in_range(address, size, kGbaEwramSize)
        ? base + address
        : nullptr;
  }

  if (IsGbaIwramDomain(normalized_domain)) {
    return system_ram_size >= kGbaCombinedRamSize && in_range(address, size, kGbaIwramSize)
        ? base + kGbaEwramSize + address
        : nullptr;
  }

  if (!IsGbaBusDomain(normalized_domain)) {
    return nullptr;
  }

  if (address >= kGbaEwramBase && address - kGbaEwramBase <= kGbaEwramSize &&
      size <= kGbaEwramSize - (address - kGbaEwramBase) && system_ram_size >= kGbaEwramSize) {
    return base + (address - kGbaEwramBase);
  }

  if (address >= kGbaIwramBase && address - kGbaIwramBase <= kGbaIwramSize &&
      size <= kGbaIwramSize - (address - kGbaIwramBase) && system_ram_size >= kGbaCombinedRamSize) {
    return base + kGbaEwramSize + (address - kGbaIwramBase);
  }

  if (system_ram_size >= kGbaCombinedRamSize && in_range(address, size, kGbaCombinedRamSize)) {
    return base + address;
  }

  return nullptr;
}

std::uint8_t* ResolveGbaLibretroMemoryMutable(CoreApi& core,
                                              std::string_view normalized_domain,
                                              std::uint32_t address,
                                              std::uint32_t size) {
  return const_cast<std::uint8_t*>(
      ResolveGbaLibretroMemory(core, normalized_domain, address, size));
}

std::string InferRuntimeSystemName(const std::filesystem::path& core_path,
                                   const std::filesystem::path& game_path) {
  const auto name = LowercaseCopy(core_path.filename().string());
  const auto extension = LowercaseCopy(game_path.extension().string());
  if (name.find("bsnes") != std::string::npos || name.find("snes") != std::string::npos) {
    return "SNES";
  }
  if (name.find("mgba") != std::string::npos) {
    if (extension == ".gb") {
      return "GB";
    }
    if (extension == ".gbc") {
      return "GBC";
    }
    return "GBA";
  }
  if (name.find("gambatte") != std::string::npos) {
    if (extension == ".gbc") {
      return "GBC";
    }
    return "GB";
  }
  if (name.find("nestopia") != std::string::npos || name.find("fce") != std::string::npos) {
    return "NES";
  }
  if (name.find("mupen") != std::string::npos || name.find("parallel") != std::string::npos) {
    return "N64";
  }
  return "LIBRETRO";
}

}  // namespace sekaiemu::spike
