#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void require(bool condition, const std::string& message) {
  if (!condition) {
    throw std::runtime_error(message);
  }
}

std::string read_all(const std::filesystem::path& path) {
  std::ifstream in(path);
  if (!in.is_open()) {
    throw std::runtime_error("read_all_failed");
  }
  return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
}

}  // namespace

int main() {
  try {
    const auto root = std::filesystem::temp_directory_path() / "sekailink_room_server_restore_daemon_smoke";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const auto seed_input = root / "seed_input.jsonl";
    {
      std::ofstream out(seed_input);
      out << "{\"cmd\":\"create_room\",\"room_id\":\"restore-daemon-room\",\"room_type\":\"async\",\"game\":\"alttp\",\"slot_id\":1,\"slot_name\":\"Jade\"}\n";
      out << "{\"cmd\":\"record_check\",\"room_id\":\"restore-daemon-room\",\"location_id\":12345}\n";
      out << "{\"cmd\":\"snapshot_room\",\"room_id\":\"restore-daemon-room\"}\n";
    }

    const auto seed_command =
        "cat \"" + seed_input.string() + "\" | " +
        "/home/nobara-user/sekailink/build-server-core/sekailink_room_server_daemon --audit-root \"" + root.string() + "\" > /dev/null";
    if (std::system(seed_command.c_str()) != 0) {
      throw std::runtime_error("seed_daemon_failed");
    }

    const auto restore_input = root / "restore_input.jsonl";
    {
      std::ofstream out(restore_input);
      out << "{\"cmd\":\"list_rooms\"}\n";
      out << "{\"cmd\":\"snapshot_room\",\"room_id\":\"restore-daemon-room\"}\n";
    }

    const auto restore_output = root / "restore_output.jsonl";
    const auto restore_command =
        "cat \"" + restore_input.string() + "\" | " +
        "/home/nobara-user/sekailink/build-server-core/sekailink_room_server_daemon --audit-root \"" + root.string() +
        "\" --restore-from-audit > \"" + restore_output.string() + "\"";
    if (std::system(restore_command.c_str()) != 0) {
      throw std::runtime_error("restore_daemon_failed");
    }

    const auto output = read_all(restore_output);
    require(output.find("restore-daemon-room") != std::string::npos, "restored_room_missing");
    require(output.find("12345") != std::string::npos, "restored_check_missing");

    std::cout << "restore_daemon_ok=1\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_server_restore_daemon_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
