#include "logger.hpp"

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <sstream>
#include <streambuf>

namespace sekaiemu::spike {

namespace {

constexpr std::uintmax_t kMaxSekaiemuLogBytes = 8u * 1024u * 1024u;

class TeeStreambuf final : public std::streambuf {
 public:
  TeeStreambuf(std::streambuf* primary, std::streambuf* secondary, std::uintmax_t initial_secondary_bytes)
      : primary_(primary), secondary_(secondary), secondary_bytes_(initial_secondary_bytes) {}

 protected:
  int overflow(int ch) override {
    if (ch == EOF) {
      return sync() == 0 ? 0 : EOF;
    }

    const int primary_result = primary_ ? primary_->sputc(static_cast<char>(ch)) : ch;
    int secondary_result = ch;
    if (secondary_ && secondary_bytes_ < kMaxSekaiemuLogBytes) {
      secondary_result = secondary_->sputc(static_cast<char>(ch));
      if (secondary_result != EOF) {
        ++secondary_bytes_;
      }
      if (secondary_bytes_ >= kMaxSekaiemuLogBytes && !limit_notice_written_) {
        WriteLimitNotice();
      }
    } else if (secondary_ && !limit_notice_written_) {
      WriteLimitNotice();
    }
    return (primary_result == EOF || secondary_result == EOF) ? EOF : ch;
  }

  int sync() override {
    int result = 0;
    if (primary_ && primary_->pubsync() != 0) {
      result = -1;
    }
    if (secondary_ && secondary_->pubsync() != 0) {
      result = -1;
    }
    return result;
  }

 private:
  void WriteLimitNotice() {
    if (!secondary_) {
      return;
    }
    static constexpr char kNotice[] =
        "\n[sekaiemu][log] file log limit reached; suppressing additional native log output\n";
    secondary_->sputn(kNotice, sizeof(kNotice) - 1);
    secondary_->pubsync();
    limit_notice_written_ = true;
  }

  std::streambuf* primary_ = nullptr;
  std::streambuf* secondary_ = nullptr;
  std::uintmax_t secondary_bytes_ = 0;
  bool limit_notice_written_ = false;
};

std::mutex g_logger_mutex;
std::ofstream g_log_stream;
std::unique_ptr<TeeStreambuf> g_cerr_tee;
std::streambuf* g_original_cerr = nullptr;
std::filesystem::path g_active_log_path;

std::string TimestampNow() {
  const auto now = std::chrono::system_clock::now();
  const auto time = std::chrono::system_clock::to_time_t(now);
  std::tm local_time{};
#ifdef _WIN32
  localtime_s(&local_time, &time);
#else
  localtime_r(&time, &local_time);
#endif

  std::ostringstream stream;
  stream << std::put_time(&local_time, "%Y-%m-%d %H:%M:%S");
  return stream.str();
}

void LogLine(std::string_view level, std::string_view message) {
  std::lock_guard lock(g_logger_mutex);
  std::cerr << "[" << TimestampNow() << "][" << level << "] " << message << "\n";
}

}  // namespace

bool InitializeLogger(const std::filesystem::path& log_path) {
  std::lock_guard lock(g_logger_mutex);
  ShutdownLogger();

  std::error_code ec;
  std::filesystem::create_directories(log_path.parent_path(), ec);
  if (ec) {
    return false;
  }

  std::uintmax_t initial_log_bytes = 0;
  if (std::filesystem::exists(log_path, ec) && !ec) {
    initial_log_bytes = std::filesystem::file_size(log_path, ec);
    if (ec) {
      initial_log_bytes = 0;
    }
  }

  g_log_stream.open(log_path, std::ios::app);
  if (!g_log_stream) {
    return false;
  }

  g_original_cerr = std::cerr.rdbuf();
  g_cerr_tee = std::make_unique<TeeStreambuf>(g_original_cerr, g_log_stream.rdbuf(), initial_log_bytes);
  std::cerr.rdbuf(g_cerr_tee.get());
  g_active_log_path = log_path;
  return true;
}

void ShutdownLogger() {
  if (g_original_cerr) {
    std::cerr.rdbuf(g_original_cerr);
    g_original_cerr = nullptr;
  }
  g_cerr_tee.reset();
  if (g_log_stream.is_open()) {
    g_log_stream.flush();
    g_log_stream.close();
  }
  g_active_log_path.clear();
}

const std::filesystem::path& ActiveLogPath() { return g_active_log_path; }

void LogInfo(std::string_view message) { LogLine("INFO", message); }

void LogWarn(std::string_view message) { LogLine("WARN", message); }

void LogError(std::string_view message) { LogLine("ERROR", message); }

}  // namespace sekaiemu::spike
