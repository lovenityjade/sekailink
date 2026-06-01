#pragma once

#include <filesystem>

namespace sekaiemu::spike {

bool IsZipArchivePath(const std::filesystem::path& path);
std::filesystem::path MaterializeTrackerArchive(const std::filesystem::path& archive_path);

}  // namespace sekaiemu::spike
