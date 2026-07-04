#ifndef _WIN32
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

#include "nlohmann/json.hpp"
#include "sekailink_server/generation_server.hpp"

int main() {
#ifdef _WIN32
  std::cerr << "sekailink_generation_server_service_smoke failed: not supported on Windows yet\n";
  return 1;
#else
  try {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "sekailink_generation_server_service_smoke";
    fs::remove_all(root);
    fs::create_directories(root / "output");

    const fs::path yaml_path = root / "input.yaml";
    const fs::path artifact = root / "output" / "job-1.done";
    const fs::path config_path = root / "generation_server.json";
    const fs::path state_path = root / "generation_server_state.json";
    {
      std::ofstream yaml_stream(yaml_path);
      yaml_stream << "game: alttp\n";
    }
    {
      std::ofstream config_stream(config_path);
      config_stream << R"({
  "tcp_port": 19093,
  "auth_token": "generation-service-secret",
  "state_path": ")" << state_path.string() << R"(",
  "command_template": [
    "/usr/bin/touch",
    "{expected_artifact}"
  ]
}
)";
    }

    const auto self_path = fs::read_symlink("/proc/self/exe");
    const std::string binary = (self_path.parent_path() / "sekailink_generation_server_service").string();
    const pid_t pid = ::fork();
    if (pid < 0) {
      throw std::runtime_error("fork_failed");
    }
    if (pid == 0) {
      ::execl(binary.c_str(), binary.c_str(), "--config", config_path.c_str(), static_cast<char*>(nullptr));
      _exit(127);
    }

    bool ready = false;
    for (int i = 0; i < 50; ++i) {
      try {
        const auto jobs = nlohmann::json::parse(sekailink_server::generation_tcp_send_json_line(
            "127.0.0.1",
            19093,
            {
                {"auth_token", "generation-service-secret"},
                {"cmd", "list_jobs"},
            }));
        if (jobs.at("ok") == true) {
          ready = true;
          break;
        }
      } catch (...) {
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (!ready) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("generation_server_not_ready");
    }

    const auto submit = nlohmann::json::parse(sekailink_server::generation_tcp_send_json_line(
        "127.0.0.1",
        19093,
        {
            {"auth_token", "generation-service-secret"},
            {"cmd", "submit_job"},
            {"job_id", "job-1"},
            {"yaml_path", yaml_path.string()},
            {"output_dir", (root / "output").string()},
            {"expected_artifact", artifact.string()},
        }));
    if (submit.at("ok") != true) {
      ::kill(pid, SIGTERM);
      ::waitpid(pid, nullptr, 0);
      throw std::runtime_error("generation_service_submit_failed");
    }

    bool completed = false;
    for (int i = 0; i < 50; ++i) {
      const auto status = nlohmann::json::parse(sekailink_server::generation_tcp_send_json_line(
          "127.0.0.1",
          19093,
          {
              {"auth_token", "generation-service-secret"},
              {"cmd", "job_status"},
              {"job_id", "job-1"},
          }));
      if (status.at("ok") == true && status.at("job").at("status") == "succeeded") {
        completed = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    const auto filtered = nlohmann::json::parse(sekailink_server::generation_tcp_send_json_line(
        "127.0.0.1",
        19093,
        {
            {"auth_token", "generation-service-secret"},
            {"cmd", "list_jobs"},
            {"limit", 1},
            {"query", "job"},
            {"status", "succeeded"},
        }));
    if (filtered.at("job_ids").size() != 1 || filtered.at("job_ids").at(0) != "job-1") {
      throw std::runtime_error("generation_service_filtered_list");
    }
    ::kill(pid, SIGTERM);
    int status = 0;
    ::waitpid(pid, &status, 0);
    if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
      throw std::runtime_error("generation_service_exit_failed");
    }

    if (!completed) {
      throw std::runtime_error("generation_service_job_not_completed");
    }
    if (!fs::exists(artifact)) {
      throw std::runtime_error("generation_service_artifact_missing");
    }
    if (!fs::exists(state_path)) {
      throw std::runtime_error("generation_service_state_missing");
    }
    {
      std::ifstream state_stream(state_path);
      nlohmann::json state_json;
      state_stream >> state_json;
      if (state_json.at("service").get<std::string>() != "sekailink_generation_server") {
        throw std::runtime_error("generation_service_state_service");
      }
      if (state_json.at("job_counts").at("succeeded").get<int>() < 1) {
        throw std::runtime_error("generation_service_state_counts");
      }
    }

    std::cout << "generation_server_service_ok=1\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_generation_server_service_smoke failed: " << exception.what() << "\n";
    return 1;
  }
#endif
}
