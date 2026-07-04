#pragma once

#include "launch_options.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace sekaiemu::spike {

struct LaunchResult {
  bool ok = false;
  int exit_code = 1;
  std::string user_message;
  std::string technical_message;
  std::filesystem::path log_path;
};

LaunchResult RunSekaiemu(const LaunchRequest& request);
LaunchResult RunSekaiemuCli(const std::string& executable_name,
                            const std::vector<std::string>& args,
                            const std::filesystem::path& current_directory);

}  // namespace sekaiemu::spike
