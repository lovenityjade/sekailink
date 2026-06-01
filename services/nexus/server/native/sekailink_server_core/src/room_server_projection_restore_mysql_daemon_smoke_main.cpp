#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

std::string require_env(const char* name) {
  const auto* value = std::getenv(name);
  if (value == nullptr || *value == '\0') {
    throw std::runtime_error(std::string("missing_env_") + name);
  }
  return value;
}

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
    const auto mysql_host = require_env("SEKAILINK_MYSQL_HOST");
    const auto mysql_port = require_env("SEKAILINK_MYSQL_PORT");
    const auto mysql_user = require_env("SEKAILINK_MYSQL_USER");
    const auto mysql_password = require_env("SEKAILINK_MYSQL_PASSWORD");
    const auto mysql_database = require_env("SEKAILINK_MYSQL_DATABASE");

    const auto root = std::filesystem::temp_directory_path() / "sekailink_room_server_projection_restore_mysql_daemon_smoke";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);

    const auto truncate_command =
        "mysql -h" + mysql_host + " -u" + mysql_user + " -p" + mysql_password + " -D " + mysql_database +
        " -e 'TRUNCATE TABLE room_records; TRUNCATE TABLE room_event_records; TRUNCATE TABLE client_report_records;'";
    if (std::system(truncate_command.c_str()) != 0) {
      throw std::runtime_error("mysql_truncate_failed");
    }

    const auto seed_input = root / "seed_input.jsonl";
    {
      std::ofstream out(seed_input);
      out << "{\"cmd\":\"create_room\",\"room_id\":\"projection-mysql-daemon-room\",\"room_type\":\"async\",\"game\":\"alttp\",\"slot_id\":1,\"slot_name\":\"Jade\"}\n";
      out << "{\"cmd\":\"record_check\",\"room_id\":\"projection-mysql-daemon-room\",\"location_id\":777}\n";
      out << "{\"cmd\":\"ingest_client_report\",\"room_id\":\"projection-mysql-daemon-room\",\"report\":{\"report_type\":\"runtime_error\",\"severity\":\"error\",\"message\":\"projection mysql restore smoke\"}}\n";
      out << "{\"cmd\":\"snapshot_room\",\"room_id\":\"projection-mysql-daemon-room\"}\n";
    }

    const auto env_prefix =
        "SEKAILINK_MYSQL_HOST=" + mysql_host + " " +
        "SEKAILINK_MYSQL_PORT=" + mysql_port + " " +
        "SEKAILINK_MYSQL_USER=" + mysql_user + " " +
        "SEKAILINK_MYSQL_PASSWORD=" + mysql_password + " ";

    const auto seed_command =
        "cat \"" + seed_input.string() + "\" | " + env_prefix +
        "/home/nobara-user/sekailink/build-server-core/sekailink_room_server_daemon --projection-root \"" + mysql_database +
        "\" --projection-backend mysql > /dev/null";
    if (std::system(seed_command.c_str()) != 0) {
      throw std::runtime_error("seed_projection_mysql_daemon_failed");
    }

    const auto restore_input = root / "restore_input.jsonl";
    {
      std::ofstream out(restore_input);
      out << "{\"cmd\":\"client_reports\",\"room_id\":\"projection-mysql-daemon-room\"}\n";
      out << "{\"cmd\":\"snapshot_room\",\"room_id\":\"projection-mysql-daemon-room\"}\n";
    }

    const auto restore_output = root / "restore_output.jsonl";
    const auto restore_command =
        "cat \"" + restore_input.string() + "\" | " + env_prefix +
        "/home/nobara-user/sekailink/build-server-core/sekailink_room_server_daemon --projection-root \"" + mysql_database +
        "\" --projection-backend mysql --restore-from-projection > \"" + restore_output.string() + "\"";
    if (std::system(restore_command.c_str()) != 0) {
      throw std::runtime_error("restore_projection_mysql_daemon_failed");
    }

    const auto output = read_all(restore_output);
    require(output.find("projection mysql restore smoke") != std::string::npos, "restored_mysql_report_missing");
    require(output.find("777") != std::string::npos, "restored_mysql_check_missing");

    std::cout << "restore_projection_mysql_daemon_ok=1\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_room_server_projection_restore_mysql_daemon_smoke failed: " << exception.what() << "\n";
    return 1;
  }
}
