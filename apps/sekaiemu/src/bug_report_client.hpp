#pragma once

#include <filesystem>
#include <string>

namespace sekaiemu::spike {

struct BugReportContext {
  std::string title;
  std::string description;
  std::filesystem::path log_path;
  std::string source = "sekaiemu";
  std::string component = "sekaiemu-libretro-host";
  std::string game;
  std::string core;
  std::string linkedworld_id;
  std::string player_alias;
};

bool SubmitBugReport(const BugReportContext& context, std::string* error = nullptr);

}  // namespace sekaiemu::spike

