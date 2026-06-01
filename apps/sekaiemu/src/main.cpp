#include "launcher.hpp"

#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
  std::vector<std::string> args;
  args.reserve(argc > 1 ? static_cast<std::size_t>(argc - 1) : 0u);
  for (int index = 1; index < argc; ++index) {
    args.emplace_back(argv[index]);
  }

  const auto result = sekaiemu::spike::RunSekaiemuCli(
      argc > 0 ? argv[0] : "sekaiemu",
      args,
      std::filesystem::current_path());

  if (!result.ok) {
    if (!result.user_message.empty()) {
      std::cerr << "[sekaiemu][user] " << result.user_message << "\n";
    }
    if (!result.technical_message.empty()) {
      std::cerr << result.technical_message << "\n";
    }
    if (!result.log_path.empty()) {
      std::cerr << "[sekaiemu][log] " << result.log_path << "\n";
    }
  }

  return result.exit_code;
}
