#include "sekailink_server/generation_service.hpp"
#include "sekailink_server/room_state.hpp"

#ifndef _WIN32
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <stdexcept>

namespace sekailink_server {

namespace {

std::string replace_all(std::string value, const std::string& needle, const std::string& replacement) {
  std::size_t pos = 0;
  while ((pos = value.find(needle, pos)) != std::string::npos) {
    value.replace(pos, needle.size(), replacement);
    pos += replacement.size();
  }
  return value;
}

std::string lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return value;
}

std::filesystem::path generation_log_path(const GenerationJob& job) {
  return job.output_dir / (job.job_id + ".generation.log");
}

std::string tail_text_file(const std::filesystem::path& path, std::size_t max_bytes = 4096) {
  std::ifstream file(path, std::ios::binary);
  if (!file) return "";
  file.seekg(0, std::ios::end);
  const auto end = file.tellg();
  const auto size = static_cast<std::streamoff>(end);
  if (size <= 0) return "";
  const auto start = size > static_cast<std::streamoff>(max_bytes) ? size - static_cast<std::streamoff>(max_bytes) : std::streamoff{0};
  file.seekg(start, std::ios::beg);
  std::string text;
  text.resize(static_cast<std::size_t>(size - start));
  file.read(text.data(), static_cast<std::streamsize>(text.size()));
  return text;
}

void write_generation_failure_log(const GenerationJob& job, const std::string& message) {
  std::error_code ignored;
  std::filesystem::create_directories(job.output_dir, ignored);
  std::ofstream file(generation_log_path(job), std::ios::binary | std::ios::trunc);
  if (!file) return;
  file << "generation_preflight_failed: " << message << "\n";
  file << "yaml_path=" << job.yaml_path.string() << "\n";
  file << "output_dir=" << job.output_dir.string() << "\n";
}

}  // namespace

std::string generation_job_status_name(GenerationJobStatus status) {
  switch (status) {
    case GenerationJobStatus::Queued:
      return "queued";
    case GenerationJobStatus::Running:
      return "running";
    case GenerationJobStatus::Succeeded:
      return "succeeded";
    case GenerationJobStatus::Failed:
      return "failed";
  }
  throw std::runtime_error("invalid_generation_job_status");
}

std::string generation_job_sort_field_name(GenerationJobSortField field) {
  switch (field) {
    case GenerationJobSortField::JobId:
      return "job_id";
    case GenerationJobSortField::RequestedAt:
      return "requested_at";
    case GenerationJobSortField::Status:
      return "status";
  }
  throw std::runtime_error("invalid_generation_job_sort_field");
}

nlohmann::json to_json(const GenerationJob& job) {
  auto payload = nlohmann::json{
      {"job_id", job.job_id},
      {"requested_at", job.requested_at},
      {"started_at", job.started_at.has_value() ? nlohmann::json(*job.started_at) : nlohmann::json(nullptr)},
      {"finished_at", job.finished_at.has_value() ? nlohmann::json(*job.finished_at) : nlohmann::json(nullptr)},
      {"yaml_path", job.yaml_path.string()},
      {"output_dir", job.output_dir.string()},
      {"status", generation_job_status_name(job.status)},
      {"exit_code", job.exit_code.has_value() ? nlohmann::json(*job.exit_code) : nlohmann::json(nullptr)},
      {"expected_artifact",
       job.expected_artifact.has_value() ? nlohmann::json(job.expected_artifact->string()) : nlohmann::json(nullptr)},
  };
  const auto log_path = generation_log_path(job);
  payload["log_path"] = log_path.string();
  if (job.status == GenerationJobStatus::Failed) {
    const auto detail = tail_text_file(log_path);
    if (!detail.empty()) {
      payload["error_detail"] = detail;
    }
  }
  return payload;
}

GenerationService::GenerationService(GenerationServiceConfig config)
    : config_(std::move(config)) {}

GenerationService::~GenerationService() {
  stop();
}

bool GenerationService::start() {
  std::scoped_lock lock(mutex_);
  if (running_) {
    return false;
  }
  running_ = true;
  stop_requested_ = false;
  worker_thread_ = std::thread([this]() {
    worker_loop();
  });
  return true;
}

void GenerationService::stop() {
  {
    std::scoped_lock lock(mutex_);
    if (!running_) {
      return;
    }
    stop_requested_ = true;
  }
  condition_.notify_all();
  if (worker_thread_.joinable()) {
    worker_thread_.join();
  }
  std::scoped_lock lock(mutex_);
  running_ = false;
}

bool GenerationService::running() const {
  std::scoped_lock lock(mutex_);
  return running_;
}

bool GenerationService::submit_job(GenerationJob job) {
  std::scoped_lock lock(mutex_);
  if (jobs_.contains(job.job_id)) {
    return false;
  }
  const auto job_id = job.job_id;
  job.status = GenerationJobStatus::Queued;
  jobs_.emplace(job_id, std::move(job));
  queued_job_ids_.push(job_id);
  condition_.notify_one();
  return true;
}

std::vector<GenerationJob> GenerationService::list_jobs(
    std::optional<std::size_t> limit,
    const std::optional<std::string>& query,
    const std::optional<std::string>& status,
    const std::optional<std::string>& requested_after,
    const std::optional<std::string>& requested_before,
    GenerationJobSortField sort_by,
    bool descending,
    std::optional<std::size_t> offset) const {
  std::vector<GenerationJob> jobs;
  std::scoped_lock lock(mutex_);
  jobs.reserve(jobs_.size());
  const auto lowered_query = query.has_value() ? std::optional<std::string>(lower(*query)) : std::nullopt;
  const auto lowered_status = status.has_value() ? std::optional<std::string>(lower(*status)) : std::nullopt;
  for (const auto& [_job_id, job] : jobs_) {
    if (lowered_status.has_value() && generation_job_status_name(job.status) != *lowered_status) {
      continue;
    }
    if (requested_after.has_value() && job.requested_at < *requested_after) {
      continue;
    }
    if (requested_before.has_value() && job.requested_at > *requested_before) {
      continue;
    }
    if (lowered_query.has_value()) {
      const auto job_id_l = lower(job.job_id);
      const auto yaml_l = lower(job.yaml_path.string());
      const auto output_l = lower(job.output_dir.string());
      if (job_id_l.find(*lowered_query) == std::string::npos &&
          yaml_l.find(*lowered_query) == std::string::npos &&
          output_l.find(*lowered_query) == std::string::npos) {
        continue;
      }
    }
    jobs.push_back(job);
  }
  std::sort(jobs.begin(), jobs.end(), [sort_by, descending](const GenerationJob& lhs, const GenerationJob& rhs) {
    const auto compare = [&](const GenerationJob& a, const GenerationJob& b) {
      switch (sort_by) {
        case GenerationJobSortField::JobId:
          return a.job_id < b.job_id;
        case GenerationJobSortField::RequestedAt:
          if (a.requested_at == b.requested_at) {
            return a.job_id < b.job_id;
          }
          return a.requested_at < b.requested_at;
        case GenerationJobSortField::Status: {
          const auto a_status = generation_job_status_name(a.status);
          const auto b_status = generation_job_status_name(b.status);
          if (a_status == b_status) {
            return a.job_id < b.job_id;
          }
          return a_status < b_status;
        }
      }
      return a.job_id < b.job_id;
    };
    return descending ? compare(rhs, lhs) : compare(lhs, rhs);
  });
  if (offset.has_value()) {
    const auto bounded_offset = std::min(*offset, jobs.size());
    jobs.erase(jobs.begin(), jobs.begin() + static_cast<std::ptrdiff_t>(bounded_offset));
  }
  if (limit.has_value() && *limit < jobs.size()) {
    jobs.resize(*limit);
  }
  return jobs;
}

std::vector<std::string> GenerationService::list_job_ids(
    std::optional<std::size_t> limit,
    const std::optional<std::string>& query,
    const std::optional<std::string>& status,
    const std::optional<std::string>& requested_after,
    const std::optional<std::string>& requested_before,
    GenerationJobSortField sort_by,
    bool descending,
    std::optional<std::size_t> offset) const {
  std::vector<std::string> job_ids;
  const auto jobs = list_jobs(limit, query, status, requested_after, requested_before, sort_by, descending, offset);
  job_ids.reserve(jobs.size());
  for (const auto& job : jobs) {
    job_ids.push_back(job.job_id);
  }
  return job_ids;
}

std::optional<GenerationJob> GenerationService::find_job(const std::string& job_id) const {
  std::scoped_lock lock(mutex_);
  const auto it = jobs_.find(job_id);
  if (it == jobs_.end()) {
    return std::nullopt;
  }
  return it->second;
}

void GenerationService::worker_loop() {
  while (true) {
    std::string job_id;
    {
      std::unique_lock lock(mutex_);
      condition_.wait(lock, [this]() {
        return stop_requested_ || !queued_job_ids_.empty();
      });
      if (stop_requested_ && queued_job_ids_.empty()) {
        break;
      }
      job_id = queued_job_ids_.front();
      queued_job_ids_.pop();
      auto& job = jobs_.at(job_id);
      job.status = GenerationJobStatus::Running;
      job.started_at = utc_timestamp_now();
    }

    int exit_code = run_job_process(*find_job(job_id));

    {
      std::scoped_lock lock(mutex_);
      auto& job = jobs_.at(job_id);
      job.exit_code = exit_code;
      job.finished_at = utc_timestamp_now();
      job.status = exit_code == 0 ? GenerationJobStatus::Succeeded : GenerationJobStatus::Failed;
    }
  }
}

int GenerationService::run_job_process(const GenerationJob& job) const {
#ifdef _WIN32
  throw std::runtime_error("generation_service_not_supported_on_windows_yet");
#else
  std::error_code ec;
  if (!std::filesystem::exists(job.yaml_path, ec)) {
    write_generation_failure_log(job, "yaml_path_missing");
    return 2;
  }
  ec.clear();
  std::filesystem::create_directories(job.output_dir, ec);
  if (ec) {
    write_generation_failure_log(job, "output_dir_create_failed:" + ec.message());
    return 2;
  }

  const auto argv_strings = build_argv(job);
  if (argv_strings.empty()) {
    write_generation_failure_log(job, "empty_command_template");
    return 1;
  }

  std::vector<char*> argv;
  argv.reserve(argv_strings.size() + 1);
  for (const auto& arg : argv_strings) {
    argv.push_back(const_cast<char*>(arg.c_str()));
  }
  argv.push_back(nullptr);

  const pid_t pid = ::fork();
  if (pid < 0) {
    return 1;
  }
  if (pid == 0) {
    ::setenv("SKIP_REQUIREMENTS_UPDATE", "1", 0);
    ::setenv("PYTHONNOUSERSITE", "1", 0);
    ::setenv("SEKAILINK_DISABLE_SPEEDUPS", "1", 0);
    const auto log_path = generation_log_path(job);
    const int log_fd = ::open(log_path.c_str(), O_CREAT | O_WRONLY | O_TRUNC, 0644);
    if (log_fd >= 0) {
      ::dup2(log_fd, STDOUT_FILENO);
      ::dup2(log_fd, STDERR_FILENO);
      if (log_fd > STDERR_FILENO) {
        ::close(log_fd);
      }
    }
    ::execvp(argv[0], argv.data());
    _exit(127);
  }

  int status = 0;
  if (::waitpid(pid, &status, 0) < 0) {
    return 1;
  }
  if (WIFEXITED(status)) {
    return WEXITSTATUS(status);
  }
  return 1;
#endif
}

std::vector<std::string> GenerationService::build_argv(const GenerationJob& job) const {
  std::vector<std::string> argv;
  argv.reserve(config_.command_template.size());
  const auto artifact = job.expected_artifact.has_value() ? job.expected_artifact->string() : std::string();
  for (const auto& token : config_.command_template) {
    auto expanded = token;
    expanded = replace_all(expanded, "{job_id}", job.job_id);
    expanded = replace_all(expanded, "{yaml_path}", job.yaml_path.string());
    expanded = replace_all(expanded, "{output_dir}", job.output_dir.string());
    expanded = replace_all(expanded, "{expected_artifact}", artifact);
    argv.push_back(std::move(expanded));
  }
  return argv;
}

}  // namespace sekailink_server
