#pragma once

#include <filesystem>
#include <string_view>

namespace sekaiemu::spike {

bool InitializeLogger(const std::filesystem::path& log_path);
void ShutdownLogger();
const std::filesystem::path& ActiveLogPath();

void LogInfo(std::string_view message);
void LogWarn(std::string_view message);
void LogError(std::string_view message);

}  // namespace sekaiemu::spike
