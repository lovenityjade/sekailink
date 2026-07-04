#include "sekailink_server/generation_server.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

int main() {
#ifdef _WIN32
  std::cerr << "sekailink_generation_server_smoke failed: not supported on Windows yet\n";
  return 1;
#else
  try {
    namespace fs = std::filesystem;
    const fs::path root = fs::temp_directory_path() / "sekailink_generation_server_smoke";
    fs::remove_all(root);
    fs::create_directories(root / "output");

    const fs::path yaml_path = root / "input.yaml";
    const fs::path artifact = root / "output" / "job-1.done";
    {
      std::ofstream yaml_stream(yaml_path);
      yaml_stream << "game: alttp\n";
    }

    sekailink_server::GenerationTcpServer server(sekailink_server::GenerationServerConfig{
        .tcp_port = 0,
        .auth_token = std::string("generation-secret"),
        .service_config = sekailink_server::GenerationServiceConfig{
            .command_template = {
                "/usr/bin/touch",
                "{expected_artifact}",
            },
        },
    });
    if (!server.start()) {
      throw std::runtime_error("generation_server_start_failed");
    }

    const auto submit = nlohmann::json::parse(sekailink_server::generation_tcp_send_json_line(
        "127.0.0.1",
        server.port(),
        {
            {"auth_token", "generation-secret"},
            {"cmd", "submit_job"},
            {"job_id", "job-1"},
            {"yaml_path", yaml_path.string()},
            {"output_dir", (root / "output").string()},
            {"expected_artifact", artifact.string()},
        }));
    if (submit.at("ok") != true) {
      throw std::runtime_error("generation_submit_failed");
    }

    bool completed = false;
    for (int i = 0; i < 50; ++i) {
      const auto status = nlohmann::json::parse(sekailink_server::generation_tcp_send_json_line(
          "127.0.0.1",
          server.port(),
          {
              {"auth_token", "generation-secret"},
              {"cmd", "job_status"},
              {"job_id", "job-1"},
          }));
      if (status.at("ok") == true && status.at("job").at("status") == "succeeded") {
        completed = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    const auto unauthorized = nlohmann::json::parse(sekailink_server::generation_tcp_send_json_line(
        "127.0.0.1",
        server.port(),
        {
            {"auth_token", "wrong-secret"},
            {"cmd", "list_jobs"},
        }));

    const auto filtered = nlohmann::json::parse(sekailink_server::generation_tcp_send_json_line(
        "127.0.0.1",
        server.port(),
        {
            {"auth_token", "generation-secret"},
            {"cmd", "list_jobs"},
            {"limit", 1},
            {"query", "job"},
            {"status", "succeeded"},
        }));
    if (filtered.at("job_ids").size() != 1 || filtered.at("job_ids").at(0) != "job-1") {
      throw std::runtime_error("generation_filtered_list_failed");
    }
    server.stop();

    if (!completed) {
      throw std::runtime_error("generation_job_not_completed");
    }
    if (!fs::exists(artifact)) {
      throw std::runtime_error("generation_artifact_missing");
    }
    if (unauthorized.at("error") != "unauthorized") {
      throw std::runtime_error("generation_unauthorized_failed");
    }

    std::cout << "generation_server_ok=1\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_generation_server_smoke failed: " << exception.what() << "\n";
    return 1;
  }
#endif
}
