#include "patch_materializer.hpp"

#include "host_io_utils.hpp"
#include "launch_options.hpp"
#include "logger.hpp"

#include <archive.h>
#include <archive_entry.h>
#include <bzlib.h>
#include <openssl/evp.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>

#include <nlohmann/json.hpp>

namespace sekaiemu::spike {
namespace {

class PatchError : public std::runtime_error {
 public:
  using std::runtime_error::runtime_error;
};

std::string TrimCopy(std::string value) {
  const auto first = value.find_first_not_of(" \t\r\n");
  if (first == std::string::npos) {
    return {};
  }
  const auto last = value.find_last_not_of(" \t\r\n");
  return value.substr(first, last - first + 1);
}

std::string JsonString(const nlohmann::json& value,
                       std::initializer_list<const char*> keys) {
  for (const char* key : keys) {
    const auto it = value.find(key);
    if (it != value.end() && it->is_string()) {
      const auto parsed = TrimCopy(it->get<std::string>());
      if (!parsed.empty()) {
        return parsed;
      }
    }
  }
  return {};
}

bool JsonStringArrayContains(const nlohmann::json& value,
                             std::initializer_list<const char*> keys,
                             std::string_view needle) {
  for (const char* key : keys) {
    const auto it = value.find(key);
    if (it == value.end()) {
      continue;
    }
    if (it->is_string()) {
      return TrimCopy(it->get<std::string>()) == needle;
    }
    if (!it->is_array()) {
      continue;
    }
    for (const auto& entry : *it) {
      if (entry.is_string() && TrimCopy(entry.get<std::string>()) == needle) {
        return true;
      }
    }
  }
  return false;
}

std::vector<std::uint8_t> ReadArchiveEntry(const std::filesystem::path& archive_path,
                                           std::string_view entry_name) {
  archive* reader = archive_read_new();
  if (!reader) {
    throw PatchError("archive_init_failed");
  }
  archive_read_support_format_zip(reader);
  archive_read_support_filter_all(reader);

  const int open_rc = archive_read_open_filename(reader, archive_path.c_str(), 10240);
  if (open_rc != ARCHIVE_OK) {
    const std::string error = archive_error_string(reader) != nullptr
                                  ? archive_error_string(reader)
                                  : "archive_open_failed";
    archive_read_free(reader);
    throw PatchError(error);
  }

  archive_entry* entry = nullptr;
  std::vector<std::uint8_t> bytes;
  bool found = false;
  while (archive_read_next_header(reader, &entry) == ARCHIVE_OK) {
    const char* pathname = archive_entry_pathname(entry);
    if (!pathname || entry_name != pathname) {
      archive_read_data_skip(reader);
      continue;
    }
    found = true;
    std::array<char, 4096> buffer{};
    while (true) {
      const la_ssize_t read_size = archive_read_data(reader, buffer.data(), buffer.size());
      if (read_size == 0) {
        break;
      }
      if (read_size < 0) {
        const std::string error = archive_error_string(reader) != nullptr
                                      ? archive_error_string(reader)
                                      : "archive_read_failed";
        archive_read_free(reader);
        throw PatchError(error);
      }
      bytes.insert(bytes.end(),
                   reinterpret_cast<const std::uint8_t*>(buffer.data()),
                   reinterpret_cast<const std::uint8_t*>(buffer.data()) + read_size);
    }
    break;
  }

  archive_read_free(reader);
  if (!found) {
    throw PatchError("archive_entry_missing:" + std::string(entry_name));
  }
  return bytes;
}

std::string Md5Hex(const std::vector<std::uint8_t>& bytes) {
  std::array<unsigned char, EVP_MAX_MD_SIZE> digest{};
  std::size_t digest_size = 0;
  if (EVP_Q_digest(nullptr, "MD5", nullptr, bytes.data(), bytes.size(), digest.data(), &digest_size) != 1) {
    throw PatchError("md5_failed");
  }

  std::ostringstream output;
  output << std::hex << std::setfill('0');
  for (std::size_t index = 0; index < digest_size; ++index) {
    output << std::setw(2) << static_cast<int>(digest[index]);
  }
  return output.str();
}

void StripSnesHeader(std::vector<std::uint8_t>& bytes) {
  if ((bytes.size() & 0x7fffU) == 512U && bytes.size() > 512U) {
    bytes.erase(bytes.begin(), bytes.begin() + 512);
  }
}

void ApplySnesChecksum(std::vector<std::uint8_t>& rom) {
  if (rom.size() < 0x8000U) {
    throw PatchError("snes_rom_too_small");
  }
  std::uint32_t checksum = 0x01FEU;
  for (std::size_t index = 0; index < 0x7FDCU && index < rom.size(); ++index) {
    checksum += rom[index];
  }
  for (std::size_t index = 0x7FE0U; index < rom.size(); ++index) {
    checksum += rom[index];
  }
  checksum &= 0xFFFFU;
  const auto inverse = static_cast<std::uint16_t>(checksum ^ 0xFFFFU);
  rom[0x7FDC] = static_cast<std::uint8_t>(inverse & 0xFFU);
  rom[0x7FDD] = static_cast<std::uint8_t>((inverse >> 8) & 0xFFU);
  rom[0x7FDE] = static_cast<std::uint8_t>(checksum & 0xFFU);
  rom[0x7FDF] = static_cast<std::uint8_t>((checksum >> 8) & 0xFFU);
}

std::int64_t ReadPatchOffset(const std::uint8_t* buffer) {
  std::int64_t value = static_cast<std::int64_t>(buffer[7] & 0x7FU);
  for (int index = 6; index >= 0; --index) {
    value = (value << 8) | static_cast<std::int64_t>(buffer[index]);
  }
  if ((buffer[7] & 0x80U) != 0) {
    value = -value;
  }
  return value;
}

std::vector<std::uint8_t> DecompressBz2(const std::uint8_t* bytes,
                                        std::size_t size,
                                        std::size_t expected_size) {
  std::vector<std::uint8_t> output(expected_size);
  unsigned int destination_size = static_cast<unsigned int>(expected_size);
  int rc = BZ2_bzBuffToBuffDecompress(reinterpret_cast<char*>(output.data()),
                                      &destination_size,
                                      const_cast<char*>(reinterpret_cast<const char*>(bytes)),
                                      static_cast<unsigned int>(size),
                                      0,
                                      0);
  if (rc != BZ_OK) {
    throw PatchError("bz2_decompression_failed:" + std::to_string(rc));
  }
  output.resize(destination_size);
  return output;
}

std::vector<std::uint8_t> ApplyBsdiff4(const std::vector<std::uint8_t>& base_rom,
                                       const std::vector<std::uint8_t>& patch_bytes) {
  if (patch_bytes.size() < 32U) {
    throw PatchError("bsdiff4_patch_too_small");
  }
  const std::string header(reinterpret_cast<const char*>(patch_bytes.data()), 8);
  if (header != "BSDIFF40") {
    throw PatchError("bsdiff4_header_invalid");
  }

  const auto control_length = ReadPatchOffset(patch_bytes.data() + 8);
  const auto diff_length = ReadPatchOffset(patch_bytes.data() + 16);
  const auto new_size = ReadPatchOffset(patch_bytes.data() + 24);
  if (control_length < 0 || diff_length < 0 || new_size < 0) {
    throw PatchError("bsdiff4_header_negative_length");
  }

  const std::size_t control_offset = 32U;
  const std::size_t diff_offset = control_offset + static_cast<std::size_t>(control_length);
  const std::size_t extra_offset = diff_offset + static_cast<std::size_t>(diff_length);
  if (extra_offset > patch_bytes.size()) {
    throw PatchError("bsdiff4_patch_truncated");
  }

  const auto control_block = DecompressBz2(patch_bytes.data() + control_offset,
                                           static_cast<std::size_t>(control_length),
                                           static_cast<std::size_t>(new_size) + (1024U * 1024U));
  const auto diff_block = DecompressBz2(patch_bytes.data() + diff_offset,
                                        static_cast<std::size_t>(diff_length),
                                        static_cast<std::size_t>(new_size));
  const auto extra_block = DecompressBz2(patch_bytes.data() + extra_offset,
                                         patch_bytes.size() - extra_offset,
                                         static_cast<std::size_t>(new_size));

  std::vector<std::uint8_t> patched(static_cast<std::size_t>(new_size));
  std::size_t control_position = 0;
  std::size_t diff_position = 0;
  std::size_t extra_position = 0;
  std::int64_t base_position = 0;
  std::size_t new_position = 0;

  while (new_position < patched.size()) {
    if (control_position + 24U > control_block.size()) {
      throw PatchError("bsdiff4_control_exhausted");
    }
    const auto add_length = ReadPatchOffset(control_block.data() + control_position);
    const auto copy_length = ReadPatchOffset(control_block.data() + control_position + 8);
    const auto seek_adjustment = ReadPatchOffset(control_block.data() + control_position + 16);
    control_position += 24U;

    if (add_length < 0 || copy_length < 0) {
      throw PatchError("bsdiff4_negative_copy_length");
    }
    const auto add_size = static_cast<std::size_t>(add_length);
    const auto copy_size = static_cast<std::size_t>(copy_length);
    if (new_position + add_size > patched.size() || diff_position + add_size > diff_block.size()) {
      throw PatchError("bsdiff4_diff_block_exhausted");
    }

    for (std::size_t index = 0; index < add_size; ++index) {
      std::uint8_t base_byte = 0;
      const auto source_index = base_position + static_cast<std::int64_t>(index);
      if (source_index >= 0 && static_cast<std::size_t>(source_index) < base_rom.size()) {
        base_byte = base_rom[static_cast<std::size_t>(source_index)];
      }
      patched[new_position + index] =
          static_cast<std::uint8_t>((base_byte + diff_block[diff_position + index]) & 0xFFU);
    }
    new_position += add_size;
    diff_position += add_size;
    base_position += static_cast<std::int64_t>(add_size);

    if (new_position + copy_size > patched.size() || extra_position + copy_size > extra_block.size()) {
      throw PatchError("bsdiff4_extra_block_exhausted");
    }
    std::copy_n(extra_block.data() + extra_position, copy_size, patched.data() + new_position);
    new_position += copy_size;
    extra_position += copy_size;
    base_position += seek_adjustment;
  }

  return patched;
}

bool ChecksumMatches(const nlohmann::json& manifest, std::string_view checksum) {
  return JsonStringArrayContains(manifest, {"base_checksum", "baseChecksum"}, checksum);
}

std::filesystem::path ResolveOutputPath(const LaunchRequest& request, const nlohmann::json& manifest) {
  std::string stem = request.patch_path.stem().string();
  if (stem.empty()) {
    stem = "patched-game";
  }
  std::string ending = JsonString(manifest, {"result_file_ending", "resultFileEnding"});
  if (ending.empty()) {
    ending = request.base_rom_path.extension().string();
    if (ending.empty()) {
      ending = ".rom";
    }
  }
  if (!ending.empty() && ending.front() == '.') {
    return request.save_directory / "patched" / (stem + ending);
  }
  return request.save_directory / "patched" / (stem + "." + ending);
}

bool OutputIsCurrent(const std::filesystem::path& output_path,
                     const std::filesystem::path& patch_path,
                     const std::filesystem::path& base_rom_path) {
  std::error_code ec;
  if (!std::filesystem::exists(output_path, ec) || ec) {
    return false;
  }
  const auto output_time = std::filesystem::last_write_time(output_path, ec);
  if (ec) {
    return false;
  }
  const auto patch_time = std::filesystem::last_write_time(patch_path, ec);
  if (ec) {
    return false;
  }
  const auto base_time = std::filesystem::last_write_time(base_rom_path, ec);
  if (ec) {
    return false;
  }
  return output_time >= patch_time && output_time >= base_time;
}

void WriteWholeFile(const std::filesystem::path& path, const std::vector<std::uint8_t>& bytes) {
  std::filesystem::create_directories(path.parent_path());
  std::ofstream output(path, std::ios::binary | std::ios::trunc);
  if (!output) {
    throw PatchError("patch_output_open_failed:" + path.string());
  }
  output.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
  if (!output.good()) {
    throw PatchError("patch_output_write_failed:" + path.string());
  }
}

}  // namespace

PatchMaterializationResult MaterializePatchedGame(const LaunchRequest& request) {
  PatchMaterializationResult result;
  if (request.patch_path.empty()) {
    result.ok = true;
    result.game_path = request.game_path;
    return result;
  }

  try {
    LogInfo("Materializing patched game from archive.");
    const auto manifest_bytes = ReadArchiveEntry(request.patch_path, "archipelago.json");
    const auto delta_bytes = ReadArchiveEntry(request.patch_path, "delta.bsdiff4");
    const auto manifest = nlohmann::json::parse(manifest_bytes.begin(), manifest_bytes.end());

    const auto output_path = ResolveOutputPath(request, manifest);
    if (OutputIsCurrent(output_path, request.patch_path, request.base_rom_path)) {
      LogInfo("Reusing existing patched output: " + output_path.string());
      result.ok = true;
      result.game_path = output_path;
      return result;
    }

    auto base_rom = ReadWholeFile(request.base_rom_path);
    if (base_rom.empty()) {
      throw PatchError("base_rom_read_failed:" + request.base_rom_path.string());
    }
    StripSnesHeader(base_rom);
    const auto checksum = Md5Hex(base_rom);
    if (!ChecksumMatches(manifest, checksum)) {
      throw PatchError("base_rom_checksum_mismatch:" + checksum);
    }

    auto patched = ApplyBsdiff4(base_rom, delta_bytes);
    if (JsonString(manifest, {"result_file_ending", "resultFileEnding"}) == ".sfc") {
      ApplySnesChecksum(patched);
    }
    WriteWholeFile(output_path, patched);
    LogInfo("Patched game written to " + output_path.string());
    result.ok = true;
    result.game_path = output_path;
    return result;
  } catch (const std::exception& exception) {
    result.technical_error = exception.what();
    return result;
  }
}

}  // namespace sekaiemu::spike
