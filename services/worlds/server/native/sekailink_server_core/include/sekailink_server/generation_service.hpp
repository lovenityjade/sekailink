#pragma once

#include "nlohmann/json.hpp"

#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <optional>
#include <queue>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace sekailink_server {

enum class GenerationJobStatus {
  Queued,
  Running,
  Succeeded,
  Failed,
};

struct GenerationJob {
  std::string job_id;
  std::string requested_at;
  std::optional<std::string> started_at;
  std::optional<std::string> finished_at;
  std::filesystem::path yaml_path;
  std::filesystem::path output_dir;
  GenerationJobStatus status = GenerationJobStatus::Queued;
  std::optional<int> exit_code;
  std::optional<std::filesystem::path> expected_artifact;
};

enum class GenerationJobSortField {
  JobId,
  RequestedAt,
  Status,
};

struct GenerationServiceConfig {
  std::vector<std::string> command_template;
};

std::string generation_job_status_name(GenerationJobStatus status);
std::string generation_job_sort_field_name(GenerationJobSortField field);
nlohmann::json to_json(const GenerationJob& job);

class GenerationService {
 public:
  explicit GenerationService(GenerationServiceConfig config);
  ~GenerationService();

  GenerationService(const GenerationService&) = delete;
  GenerationService& operator=(const GenerationService&) = delete;

  bool start();
  void stop();
  [[nodiscard]] bool running() const;

  bool submit_job(GenerationJob job);
  [[nodiscard]] std::vector<std::string> list_job_ids(
      std::optional<std::size_t> limit = std::nullopt,
      const std::optional<std::string>& query = std::nullopt,
      const std::optional<std::string>& status = std::nullopt,
      const std::optional<std::string>& requested_after = std::nullopt,
      const std::optional<std::string>& requested_before = std::nullopt,
      GenerationJobSortField sort_by = GenerationJobSortField::JobId,
      bool descending = false,
      std::optional<std::size_t> offset = std::nullopt) const;
  [[nodiscard]] std::vector<GenerationJob> list_jobs(
      std::optional<std::size_t> limit = std::nullopt,
      const std::optional<std::string>& query = std::nullopt,
      const std::optional<std::string>& status = std::nullopt,
      const std::optional<std::string>& requested_after = std::nullopt,
      const std::optional<std::string>& requested_before = std::nullopt,
      GenerationJobSortField sort_by = GenerationJobSortField::JobId,
      bool descending = false,
      std::optional<std::size_t> offset = std::nullopt) const;
  [[nodiscard]] std::optional<GenerationJob> find_job(const std::string& job_id) const;

 private:
  void worker_loop();
  int run_job_process(const GenerationJob& job) const;
  [[nodiscard]] std::vector<std::string> build_argv(const GenerationJob& job) const;

  GenerationServiceConfig config_;
  mutable std::mutex mutex_;
  std::condition_variable condition_;
  std::unordered_map<std::string, GenerationJob> jobs_;
  std::queue<std::string> queued_job_ids_;
  bool running_ = false;
  bool stop_requested_ = false;
  std::thread worker_thread_;
};

}  // namespace sekailink_server
