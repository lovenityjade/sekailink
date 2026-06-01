#include "sekailink_server/generation_service.hpp"
#include "sekailink_server/room_state.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <thread>

int main() {
#ifdef _WIN32
  std::cerr << "sekailink_generation_service_smoke failed: not supported on Windows yet\n";
  return 1;
#else
  try {
    namespace fs = std::filesystem;
    {
      sekailink_server::GenerationService list_service(sekailink_server::GenerationServiceConfig{
          .command_template = {"/usr/bin/true"},
      });
      const auto first = list_service.submit_job(sekailink_server::GenerationJob{
          .job_id = "job-a",
          .requested_at = "2026-03-29T01:00:00Z",
          .yaml_path = "/tmp/a.yaml",
          .output_dir = "/tmp/a",
      });
      const auto second = list_service.submit_job(sekailink_server::GenerationJob{
          .job_id = "job-b",
          .requested_at = "2026-03-29T02:00:00Z",
          .yaml_path = "/tmp/b.yaml",
          .output_dir = "/tmp/b",
          .status = sekailink_server::GenerationJobStatus::Succeeded,
      });
      if (!first || !second) {
        throw std::runtime_error("generation_list_submit_failed");
      }
      const auto requested_desc = list_service.list_jobs(
          std::nullopt,
          std::nullopt,
          std::nullopt,
          std::nullopt,
          std::nullopt,
          sekailink_server::GenerationJobSortField::RequestedAt,
          true);
      if (requested_desc.size() != 2 || requested_desc.at(0).job_id != "job-b") {
        throw std::runtime_error("generation_requested_desc_sort_failed");
      }
      const auto time_window = list_service.list_jobs(
          std::nullopt,
          std::nullopt,
          std::nullopt,
          std::optional<std::string>("2026-03-29T01:30:00Z"),
          std::optional<std::string>("2026-03-29T03:00:00Z"));
      if (time_window.size() != 1 || time_window.at(0).job_id != "job-b") {
        throw std::runtime_error("generation_requested_window_failed");
      }
    }

    const fs::path root = fs::temp_directory_path() / "sekailink_generation_service_smoke";
    fs::remove_all(root);
    fs::create_directories(root / "output");

    const fs::path yaml_path = root / "input.yaml";
    const fs::path artifact = root / "output" / "job-1.done";
    {
      std::ofstream yaml_stream(yaml_path);
      yaml_stream << "game: alttp\n";
    }

    sekailink_server::GenerationService service(sekailink_server::GenerationServiceConfig{
        .command_template = {
            "/usr/bin/touch",
            "{expected_artifact}",
        },
    });
    if (!service.start()) {
      throw std::runtime_error("generation_service_start_failed");
    }

    const auto submitted = service.submit_job(sekailink_server::GenerationJob{
        .job_id = "job-1",
        .requested_at = sekailink_server::utc_timestamp_now(),
        .yaml_path = yaml_path,
        .output_dir = root / "output",
        .expected_artifact = artifact,
    });
    if (!submitted) {
      throw std::runtime_error("generation_job_submit_failed");
    }

    bool completed = false;
    for (int i = 0; i < 50; ++i) {
      const auto job = service.find_job("job-1");
      if (job.has_value() && job->status == sekailink_server::GenerationJobStatus::Succeeded) {
        completed = true;
        break;
      }
      std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    service.stop();

    if (!completed) {
      throw std::runtime_error("generation_job_not_completed");
    }
    if (!fs::exists(artifact)) {
      throw std::runtime_error("generation_artifact_missing");
    }

    std::cout << "generation_service_ok=1\n";
    return 0;
  } catch (const std::exception& exception) {
    std::cerr << "sekailink_generation_service_smoke failed: " << exception.what() << "\n";
    return 1;
  }
#endif
}
