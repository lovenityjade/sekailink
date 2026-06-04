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

class TeeStreambuf final : public std::streambuf {
 public:
  TeeStreambuf(std::streambuf* primary, std::streambuf* secondary)
      : primary_(primary), secondary_(secondary) {}

 protected:
  int overflow(int ch) override {
    if (ch == EOF) {
      return sync() == 0 ? 0 : EOF;
    }

    const int primary_result = primary_ ? primary_->sputc(static_cast<char>(ch)) : ch;
    const int secondary_result = secondary_ ? secondary_->sputc(static_cast<char>(ch)) : ch;
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
  std::streambuf* primary_ = nullptr;
  std::streambuf* secondary_ = nullptr;
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

  g_log_stream.open(log_path, std::ios::app);
  if (!g_log_stream) {
    return false;
  }

  g_original_cerr = std::cerr.rdbuf();
  g_cerr_tee = std::make_unique<TeeStreambuf>(g_original_cerr, g_log_stream.rdbuf());
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
