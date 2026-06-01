#include "tracker_bundle_archive.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <archive.h>
#include <archive_entry.h>

namespace sekaiemu::spike {
namespace {

std::string LowercaseCopy(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
    return static_cast<char>(std::tolower(ch));
  });
  return value;
}

std::uint64_t Fnv1a64(std::string_view value) {
  std::uint64_t hash = 14695981039346656037ull;
  for (const unsigned char ch : value) {
    hash ^= ch;
    hash *= 1099511628211ull;
  }
  return hash;
}

std::string Hex64(std::uint64_t value) {
  constexpr char kDigits[] = "0123456789abcdef";
  std::string out(16, '0');
  for (int index = 15; index >= 0; --index) {
    out[static_cast<std::size_t>(index)] = kDigits[value & 0x0f];
    value >>= 4u;
  }
  return out;
}

std::filesystem::path TrackerArchiveCacheRoot() {
  if (const char* override_root = std::getenv("SEKAIEMU_TRACKER_CACHE_DIR");
      override_root != nullptr && override_root[0] != '\0') {
    return std::filesystem::path(override_root);
  }
  return std::filesystem::temp_directory_path() / "sekaiemu-tracker-bundles";
}

std::filesystem::path TrackerArchiveCachePath(const std::filesystem::path& archive_path) {
  const auto absolute_path = std::filesystem::absolute(archive_path);
  std::ostringstream key;
  key << absolute_path.string();
  std::error_code ec;
  const auto size = std::filesystem::file_size(absolute_path, ec);
  if (!ec) {
    key << ':' << size;
  }
  const auto write_time = std::filesystem::last_write_time(absolute_path, ec);
  if (!ec) {
    key << ':' << write_time.time_since_epoch().count();
  }
  return TrackerArchiveCacheRoot() / (archive_path.stem().string() + "-" + Hex64(Fnv1a64(key.str())));
}

bool IsSafeArchiveEntryPath(std::string_view raw_path) {
  if (raw_path.empty() || raw_path[0] == '/' || raw_path[0] == '\\') {
    return false;
  }
  if (raw_path.find(':') != std::string_view::npos || raw_path.find('\\') != std::string_view::npos) {
    return false;
  }
  std::filesystem::path normalized = std::filesystem::path(std::string(raw_path)).lexically_normal();
  if (normalized.empty() || normalized.is_absolute()) {
    return false;
  }
  for (const auto& part : normalized) {
    if (part == "..") {
      return false;
    }
  }
  return true;
}

void ExtractTrackerArchiveToDirectory(const std::filesystem::path& archive_path,
                                      const std::filesystem::path& output_root) {
  archive* reader = archive_read_new();
  if (reader == nullptr) {
    throw std::runtime_error("tracker_archive_init_failed");
  }
  archive_read_support_format_zip(reader);
  archive_read_support_filter_all(reader);

  const int open_rc = archive_read_open_filename(reader, archive_path.c_str(), 10240);
  if (open_rc != ARCHIVE_OK) {
    const std::string error =
        archive_error_string(reader) != nullptr ? archive_error_string(reader) : "archive_open_failed";
    archive_read_free(reader);
    throw std::runtime_error("tracker_archive_open_failed:" + archive_path.string() + ":" + error);
  }

  archive_entry* entry = nullptr;
  std::vector<char> buffer(64 * 1024);
  int header_rc = ARCHIVE_OK;
  while ((header_rc = archive_read_next_header(reader, &entry)) == ARCHIVE_OK) {
    const char* pathname = archive_entry_pathname(entry);
    if (pathname == nullptr || !IsSafeArchiveEntryPath(pathname)) {
      archive_read_free(reader);
      throw std::runtime_error("tracker_archive_unsafe_entry:" +
                               std::string(pathname == nullptr ? "<null>" : pathname));
    }

    const auto output_path = output_root / std::filesystem::path(pathname).lexically_normal();
    if (archive_entry_filetype(entry) == AE_IFDIR) {
      std::filesystem::create_directories(output_path);
      archive_read_data_skip(reader);
      continue;
    }
    if (archive_entry_filetype(entry) != AE_IFREG) {
      archive_read_data_skip(reader);
      continue;
    }

    std::filesystem::create_directories(output_path.parent_path());
    std::ofstream output(output_path, std::ios::binary | std::ios::trunc);
    if (!output) {
      archive_read_free(reader);
      throw std::runtime_error("tracker_archive_write_failed:" + output_path.string());
    }

    while (true) {
      const la_ssize_t read_size = archive_read_data(reader, buffer.data(), buffer.size());
      if (read_size == 0) {
        break;
      }
      if (read_size < 0) {
        const std::string error =
            archive_error_string(reader) != nullptr ? archive_error_string(reader) : "archive_read_failed";
        archive_read_free(reader);
        throw std::runtime_error("tracker_archive_read_failed:" + archive_path.string() + ":" + error);
      }
      output.write(buffer.data(), read_size);
      if (!output) {
        archive_read_free(reader);
        throw std::runtime_error("tracker_archive_write_failed:" + output_path.string());
      }
    }
  }

  if (header_rc != ARCHIVE_EOF) {
    const std::string error =
        archive_error_string(reader) != nullptr ? archive_error_string(reader) : "archive_header_read_failed";
    archive_read_free(reader);
    throw std::runtime_error("tracker_archive_header_failed:" + archive_path.string() + ":" + error);
  }

  archive_read_free(reader);
}

}  // namespace

bool IsZipArchivePath(const std::filesystem::path& path) {
  const auto extension = LowercaseCopy(path.extension().string());
  return extension == ".zip";
}

std::filesystem::path MaterializeTrackerArchive(const std::filesystem::path& archive_path) {
  const auto cache_path = TrackerArchiveCachePath(archive_path);
  if (std::filesystem::exists(cache_path / "manifest.json")) {
    return cache_path;
  }

  const auto staging_path = cache_path.string() + ".tmp";
  std::error_code ec;
  std::filesystem::remove_all(staging_path, ec);
  std::filesystem::create_directories(staging_path);
  ExtractTrackerArchiveToDirectory(archive_path, staging_path);

  if (!std::filesystem::exists(std::filesystem::path(staging_path) / "manifest.json")) {
    std::filesystem::remove_all(staging_path, ec);
    throw std::runtime_error("tracker_archive_manifest_missing:" + archive_path.string());
  }

  std::filesystem::remove_all(cache_path, ec);
  std::filesystem::rename(staging_path, cache_path, ec);
  if (ec) {
    std::filesystem::remove_all(cache_path, ec);
    std::filesystem::create_directories(cache_path.parent_path());
    std::filesystem::rename(staging_path, cache_path);
  }
  return cache_path;
}

}  // namespace sekaiemu::spike
